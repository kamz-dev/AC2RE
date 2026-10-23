#pragma once
// Anchor points: where a hand or a foot holds the guidance (RE/19_guidance.md 5). The contexts that climb, hang or
// shimmy find them with the searches below, on top of the guidance casts.

#include <cstdint>
#include <vector>

#include "scimitar/guidance.h"
#include "scimitar/math.h"

namespace port {
class World;
}

namespace scimitar::ac2 {

// scimitar::ac2::AnchorPointType (Revelations PDB).
enum AnchorPointType : int {
    AnchorPointType_Ledge = 0,
    AnchorPointType_Surface = 1,
    AnchorPointType_Rope = 2,
    AnchorPointType_Pole = 3,
    AnchorPointType_ZipLine = 4,
};

// scimitar::ac2::AnchorPoint (0x50, Revelations PDB): a point and a normal in world space and in the object's.
struct AnchorPoint {
    Vec3 m_GlobalPosition;       // +0x00
    Vec3 m_GlobalNormal;         // +0x10
    Vec3 m_LocalObjectPosition;  // +0x20
    Vec3 m_LocalObjectNormal;    // +0x30
    // +0x40 m_Object (EntityInWorldHandle). PORT: the guidance component stands in for its entity.
    const GuidanceSystemComponent* m_Object = nullptr;
    AnchorPointType m_AnchorPointType = AnchorPointType_Ledge;  // +0x44
    float m_RopeT = 0;                                          // +0x48

    // Mac 0x1449F88: the global point and normal, and the object's local ones through its inverse matrix; then
    // SetupRopeAnchor (PORT: no ropes).
    void SetFromGlobal(const Vec3& position, const Vec3& normal, const GuidanceSystemComponent* object,
                       AnchorPointType type);
};

// The anchor type of a contact's subtype (GetClosestPointInBoxWithOffset / GetNextVerticalLimbPos): Rope -> Rope,
// Pole -> Pole, Surface -> Surface, else Ledge.
AnchorPointType AnchorPointTypeFromSubType(std::uint32_t subType);

// Mac 0x144A4E0: the line's flat normal (GuidanceContactObject::GetLineGlobalFlatNormal); a ladder's is its
// bottom-most normal, flattened (PORT: GetLineBottomMostNormal isn't decoded; taken as the lower face's normal).
Vec3 GetFlatNormalFromGuidanceSubType(const GuidanceContactObject& contact);

// Mac 0xD1AFFE: the closest point to `point` on p0-p1 shortened by `minDist` at each end; the middle when the
// segment is no longer than 2 minDist + 0.0005.
Vec3 ProjectPointOnSegmentWithMinDist(const Vec3& point, const Vec3& p0, const Vec3& p1, float minDist);

// Mac 0xD1B29A: where p0-p1 crosses the plane (point, normal); when it doesn't, the end nearer the plane; then moved
// in from the ends by `minDist` (ProjectPointOnSegmentWithMinDist).
Vec3 GetBestPointOnSegmentInPlane(const Vec3& planePoint, const Vec3& planeNormal, const Vec3& p0, const Vec3& p1,
                                  float minDist);

// Mac 0xD1F156 (-> 0xD1EA7A without the limb occlusion). A GuidanceZoneBox at `position` (Y = `axisY`, Z = `axisZ`,
// X = Y x Z; half extents) cast in cut mode (or, PORT, the given report as it is: GuidanceZone::RefineReport isn't
// decoded); among the contacts in `subTypeMask` (1: any) whose flat normal is within `maxAngle` of -Y, the one
// closest to `point` (in the box's frame when `pointIsLocal`) after ProjectPointOnSegmentWithMinDist. The anchor
// is that point with the flat normal. False (and a cleared anchor) when there is none.
bool GetClosestPointInBoxWithOffset(AnchorPoint& anchor, const port::World& world,
                                    const std::vector<GuidanceContactObject>* report, const Vec3& position,
                                    const Vec3& axisY, const Vec3& axisZ, float halfX, float halfY, float halfZ,
                                    float maxAngle, const Vec3& point, bool pointIsLocal, float minDist,
                                    std::uint32_t subTypeMask);

// Mac 0xD206F8 (-> 0xD1F226). The next hold for a limb on a vertical climb, in the frame at `position` (Y =
// `normal`, X = normal x K, Z = X x Y): among `report`'s lines (surfaces only with `withSurfaces`) cut to the box
// x in +-width/2, y in +-0.52, z in [minZ, maxZ], facing back (flat normal . Y < -0.5), the point in the plane
// x = `lateral` (GetBestPointOnSegmentInPlane); moving `up` (along the entity's banking), the one farthest from
// minZ - 0.1, else from maxZ + 0.1; within 0.01 m of each other, the one nearer x = `lateral`. PORT: the limb
// occlusion capsules (the `occlusion` flag) aren't ported.
bool GetNextVerticalLimbPos(AnchorPoint& anchor, const std::vector<GuidanceContactObject>& report,
                            const Vec3& entityBanking, const Vec3& position, const Vec3& up, const Vec3& normal,
                            float minZ, float maxZ, float width, float lateral, bool withSurfaces);

}  // namespace scimitar::ac2
