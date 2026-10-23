#include "scimitar/ac2/anchor_point.h"

#include <cmath>

#include "port/world.h"
#include "scimitar/ac2/edge_clamping.h"

namespace scimitar::ac2 {

void AnchorPoint::SetFromGlobal(const Vec3& position, const Vec3& normal, const GuidanceSystemComponent* object,
                                AnchorPointType type) {
    m_GlobalPosition = position;
    m_GlobalNormal = normal;
    m_Object = object;
    if (object) {
        const Gear::Matrix44 inverse = object->m_PortGlobalMatrix.GetAffineInverse();
        m_LocalObjectPosition = inverse.TransformVertex(position);
        m_LocalObjectNormal = inverse.TransformVector(normal);
    }
    m_AnchorPointType = type;
}

AnchorPointType AnchorPointTypeFromSubType(std::uint32_t subType) {
    switch (subType & 0x3F) {
        case GuidanceObjectSubTypeRope: return AnchorPointType_Rope;
        case GuidanceObjectSubTypePole: return AnchorPointType_Pole;
        case GuidanceObjectSubTypeSurface: return AnchorPointType_Surface;
        default: return AnchorPointType_Ledge;
    }
}

Vec3 GetFlatNormalFromGuidanceSubType(const GuidanceContactObject& contact) {
    if ((contact.m_SubType & 0x3F) == GuidanceObjectSubTypeLadder) {
        const Vec3& n = contact.m_LineGlobalNormalLeft.z < contact.m_LineGlobalNormalRight.z
                            ? contact.m_LineGlobalNormalLeft
                            : contact.m_LineGlobalNormalRight;
        return normalized(Vec3{n.x, n.y, 0});  // ClearZAndNormalize
    }
    return GetLineGlobalFlatNormal(contact.m_LineGlobalPoint0, contact.m_LineGlobalPoint1,
                                   contact.m_LineGlobalNormalLeft, contact.m_LineGlobalNormalRight);
}

Vec3 ProjectPointOnSegmentWithMinDist(const Vec3& point, const Vec3& p0, const Vec3& p1, float minDist) {
    const Vec3 d = p1 - p0;
    const float len = length(d);
    if (minDist + minDist + 0.0005f >= len) return p0 + d * 0.5f;
    const Vec3 n = d * (1.0f / len);
    const Vec3 end = p0 + n * (len - minDist);
    const Vec3 start = p0 + n * minDist;
    return PointSegmentProjection(point, start, end);
}

Vec3 GetBestPointOnSegmentInPlane(const Vec3& planePoint, const Vec3& planeNormal, const Vec3& p0, const Vec3& p1,
                                  float minDist) {
    Vec3 hit;
    if (IntersectSegmentWithInfinitePlane(hit, p0, p1, planePoint, planeNormal)) {
        if (minDist != 0.0f) hit = ProjectPointOnSegmentWithMinDist(hit, p0, p1, minDist);
        return hit;
    }
    // Neither end is on the plane's other side: the end nearer it (each projected along the normal).
    const Vec3 d0 = p0 - ProjectPointOnPlane(p0, planePoint, planeNormal, planeNormal);
    const Vec3 d1 = p1 - ProjectPointOnPlane(p1, planePoint, planeNormal, planeNormal);
    const Vec3 nearest = dot(d1, d1) <= dot(d0, d0) ? p1 : p0;
    return ProjectPointOnSegmentWithMinDist(nearest, p0, p1, minDist);
}

bool GetClosestPointInBoxWithOffset(AnchorPoint& anchor, const port::World& world,
                                    const std::vector<GuidanceContactObject>* report, const Vec3& position,
                                    const Vec3& axisY, const Vec3& axisZ, float halfX, float halfY, float halfZ,
                                    float maxAngle, const Vec3& point, bool pointIsLocal, float minDist,
                                    std::uint32_t subTypeMask) {
    GuidanceZoneBox box;
    box.m_Matrix.m_Rows[0] = cross(axisY, axisZ);
    box.m_Matrix.m_Rows[1] = axisY;
    box.m_Matrix.m_Rows[2] = axisZ;
    box.m_Matrix.m_Rows[3] = position;
    box.m_Min = {-halfX, -halfY, -halfZ};
    box.m_Max = {halfX, halfY, halfZ};
    std::vector<GuidanceContactObject> contacts;
    if (report)
        contacts = *report;  // PORT: GuidanceZone::RefineReport(report, cut, 8) isn't decoded
    else
        box.CastWorld(contacts, true, kGuidanceContactObjectSubType_All, world);
    // GuidanceReport::RemoveZeroLengthGuidancesFromReport: the casts already drop degenerate lines.
    const Vec3 target = pointIsLocal ? box.m_Matrix.TransformVertex(point) : point;
    float best = 1000.0f;
    const GuidanceContactObject* found = nullptr;
    Vec3 foundPoint;
    for (const GuidanceContactObject& c : contacts) {
        const std::uint32_t bit = 1u << (c.m_SubType & 0x3F);
        if (subTypeMask != 1 && (bit & subTypeMask) == 0) continue;
        const Vec3 flat = GetFlatNormalFromGuidanceSubType(c);
        if (Gear::GetAngleAbsolute(flat, axisY) < 3.1415927f - maxAngle) continue;
        const Vec3 p = ProjectPointOnSegmentWithMinDist(target, c.m_LineGlobalPoint0, c.m_LineGlobalPoint1, minDist);
        const Vec3 d = p - target;
        const float d2 = dot(d, d);
        if (!(best > d2)) continue;
        // PORT: a ladder's point is corrected onto its step (CorrectPositionOnLaddetStep); no ladders yet.
        found = &c;
        foundPoint = p;
        best = d2;
    }
    if (!found) {
        anchor.SetFromGlobal({}, {}, nullptr, AnchorPointType_Ledge);
        return false;
    }
    anchor.SetFromGlobal(foundPoint, GetFlatNormalFromGuidanceSubType(*found), found->m_Entity,
                         AnchorPointTypeFromSubType(found->m_SubType));
    return true;
}

bool GetNextVerticalLimbPos(AnchorPoint& anchor, const std::vector<GuidanceContactObject>& report,
                            const Vec3& entityBanking, const Vec3& position, const Vec3& up, const Vec3& normal,
                            float minZ, float maxZ, float width, float lateral, bool withSurfaces) {
    Gear::Matrix44 frame;
    frame.m_Rows[0] = normalized(cross(normal, Gear::kBaseK));
    frame.m_Rows[1] = normal;
    frame.m_Rows[2] = cross(frame.m_Rows[0], normal);
    frame.m_Rows[3] = position;
    const Gear::Matrix44 inverse = frame.GetAffineInverse();  // PORT: Matrix44::GetInverse of an orthonormal frame
    // The search goes up (along the entity's banking) or down: the target is past the other end of the box.
    Vec3 bestLocal;
    float target;
    if (dot(up, entityBanking) >= 0.0f) {
        bestLocal = {lateral, 0, maxZ};
        target = minZ - 0.1f;
    } else {
        bestLocal = {lateral, 0, minZ};
        target = maxZ + 0.1f;
    }
    const Vec3 planePoint = position + frame.m_Rows[0] * lateral;
    const Vec3 boxMin{-width * 0.5f, -0.52f, minZ};
    const Vec3 boxMax{width * 0.5f, 0.52f, maxZ};
    float best = -1.0f;
    const GuidanceContactObject* found = nullptr;
    Vec3 foundPoint, foundNormal;
    for (const GuidanceContactObject& c : report) {
        if ((c.m_SubType & 0x3F) == GuidanceObjectSubTypeSurface && !withSurfaces) continue;
        Vec3 a, b;
        if (!IntersectSegmentOOBBox(c.m_LineGlobalPoint0, c.m_LineGlobalPoint1, frame, boxMin, boxMax, a, b)) continue;
        const Vec3 flat = GetLineGlobalFlatNormal(c.m_LineGlobalPoint0, c.m_LineGlobalPoint1,
                                                  c.m_LineGlobalNormalLeft, c.m_LineGlobalNormalRight);
        if (!(dot(frame.m_Rows[1], flat) < -0.5f)) continue;
        const Vec3 p = GetBestPointOnSegmentInPlane(planePoint, frame.m_Rows[0], a, b, 0.0f);
        const Vec3 local = inverse.TransformVertex(p);
        const float score = std::fabs(local.z - target);
        const bool keep = std::fabs(score - best) >= 0.01f ? score <= best
                                                           : std::fabs(bestLocal.x - lateral) <= std::fabs(local.x - lateral);
        if (keep) continue;
        found = &c;
        foundPoint = p;
        foundNormal = flat;
        bestLocal = local;
        best = score;
    }
    if (!found) {
        anchor.SetFromGlobal({}, {}, nullptr, AnchorPointType_Ledge);
        return false;
    }
    anchor.SetFromGlobal(foundPoint, foundNormal, found->m_Entity, AnchorPointTypeFromSubType(found->m_SubType));
    return true;
}

HangEntityPose ComputeHangWallEntityPosition(const Vec3& hand0, const Vec3& hand1, const Vec3& normal0,
                                             const Vec3& normal1, const Vec3& toe0, const Vec3& toe1,
                                             bool useHigherHand) {
    // The sight against the two normals; when they are nearly opposite (cos < -0.98480773, 170 degrees), the first.
    Vec3 sight = normal0 * -1.0f + normal1 * -1.0f;
    if (dot(normal0, normal1) < -0.98480773f) sight = normal0;
    sight = sight * (1.0f / length(sight));
    const Vec3 handsMid = (hand1 + hand0) * 0.5f;
    const Vec3 toesMid = (toe0 + toe1) * 0.5f;
    Vec3 side = cross(sight, Gear::kBaseK);
    side = side * (1.0f / length(side));
    const Vec3 toesInPlane = ProjectPointOnPlane(toesMid, handsMid, side, side);
    Vec3 up = handsMid - toesInPlane;
    up = up * (1.0f / length(up));
    Vec3 sightOut = cross(up, side);
    sightOut = sightOut * (1.0f / length(sightOut));
    HangEntityPose pose;
    pose.sight = sightOut;
    pose.up = up;
    pose.position = handsMid;
    const bool takeHand1 = useHigherHand ? hand0.z < hand1.z : hand1.z < hand0.z;
    pose.position.z = takeHand1 ? hand1.z : hand0.z;
    pose.position = pose.position - up * 1.1f - sightOut * 0.5f;
    return pose;
}

HangEntityPose ComputeHangFreeEntityPosition(const Vec3& hand0, const Vec3& hand1, const Vec3& normal0,
                                             const Vec3& normal1) {
    HangEntityPose pose;
    pose.position = (hand0 + hand1) * 0.5f;
    pose.position.z = hand0.z <= hand1.z ? hand1.z : hand0.z;
    pose.up = Gear::kBaseK;
    Vec3 n = normal0 + normal1;
    n.z = 0;
    pose.sight = n * (-1.0f / length(n));
    pose.position = pose.position - pose.up * 2.4f;
    return pose;
}

}  // namespace scimitar::ac2
