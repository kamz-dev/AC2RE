#include "scimitar/guidance.h"

#include <cmath>

#include "port/world.h"

namespace scimitar {

namespace {
// g_CompressVector3To48_Range (PC 0x1E3E6F0; set by a static initializer, Mac 0x15D10) and the step.
constexpr float kCompressRange = 163.84f;
constexpr float kCompressStep = 0.005f;
// Both AddContactPoints drop a line whose p1 - p0 is within this on every lane (Mac 0x1F5ABE0).
constexpr float kDegenerateEpsilon = 0.0005f;

float Lane(const Vec3& v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }
float& Lane(Vec3& v, int i) { return i == 0 ? v.x : i == 1 ? v.y : v.z; }

std::int32_t SignExtend10(std::uint32_t v) { return (v & 0x200) ? static_cast<std::int32_t>(v | 0xFFFFFC00u) : static_cast<std::int32_t>(v); }

void AddContact(GuidanceReportAccumulator& accumulator, GuidanceContactObject&& contact) {
    // The mask, then the degenerate test.
    if (((1u << (contact.m_SubType & 0x3F)) & accumulator.m_SubTypeMask) == 0) return;
    const Vec3 d = contact.m_LineGlobalPoint1 - contact.m_LineGlobalPoint0;
    if (std::fabs(d.x) <= kDegenerateEpsilon && std::fabs(d.y) <= kDegenerateEpsilon &&
        std::fabs(d.z) <= kDegenerateEpsilon)
        return;
    accumulator.m_Report.push_back(contact);
}

// GuidanceZone::AddContactPoint (PC 0x3FE3C0, Mac 0x62C770): the whole line, moved to world
// (GuidanceContactObject::Transform PC 0x4214B0).
void AddContactPoint(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
                     const GuidanceObject& object) {
    const Gear::Matrix44& m = component.m_PortGlobalMatrix;
    GuidanceContactObject c;
    c.m_Entity = &component;
    c.m_ParentGuidanceObject = &object;
    c.m_SubType = static_cast<std::uint8_t>(object.GetSubType());
    c.m_LineGlobalPoint0 = m.TransformVertex(component.GetPoint(object.GetLineIndex0()));
    c.m_LineGlobalPoint1 = m.TransformVertex(component.GetPoint(object.GetLineIndex1()));
    c.m_LineGlobalNormalLeft = m.TransformVector(object.GetLineNormalLeft());
    c.m_LineGlobalNormalRight = m.TransformVector(object.GetLineNormalRight());
    AddContact(accumulator, std::move(c));
}

// The cut overload (PC 0x3FE4F0, Mac 0x62C2F0): the cut's two points (already in world), the normals turned.
void AddContactPoint(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
                     const GuidanceObject& object, const Vec3& cutStart, const Vec3& cutEnd) {
    const Gear::Matrix44& m = component.m_PortGlobalMatrix;
    GuidanceContactObject c;
    c.m_Entity = &component;
    c.m_ParentGuidanceObject = &object;
    c.m_SubType = static_cast<std::uint8_t>(object.GetSubType());
    c.m_LineGlobalPoint0 = cutStart;
    c.m_LineGlobalPoint1 = cutEnd;
    c.m_LineGlobalNormalLeft = m.TransformVector(object.GetLineNormalLeft());
    c.m_LineGlobalNormalRight = m.TransformVector(object.GetLineNormalRight());
    AddContact(accumulator, std::move(c));
}
}  // namespace

// --- Data ---

Vec3 LoadDecN4(std::uint32_t packed) {
    return {static_cast<float>(SignExtend10(packed & 0x3FF)) / 511.0f,
            static_cast<float>(SignExtend10((packed >> 10) & 0x3FF)) / 511.0f,
            static_cast<float>(SignExtend10((packed >> 20) & 0x3FF)) / 511.0f};
}

std::uint32_t StoreDecN4(const Vec3& v) {
    // Each lane clamped to [-1, 1], times 511, truncated; w (0 here) at bit 30.
    auto lane = [](float f) {
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(f * 511.0f)) & 0x3FF;
    };
    return lane(v.x) | (lane(v.y) << 10) | (lane(v.z) << 20);
}

bool EdgeFilter::AcceptEdge(const Vec3& normalLeft, const Vec3& normalRight) const {
    const float left = dot(normalLeft, Gear::kBaseK);
    const float right = dot(normalRight, Gear::kBaseK);
    const float c = m_SlopeCosAngle;
    const std::uint8_t f = m_Filter;
    if (left > c) {  // the left face is ground
        if ((f & EdgeWallGround) && c > std::fabs(right)) return true;
        if (f & EdgeGroundCeiling) return -c > right;
        return false;
    }
    if (-c > left)  // ceiling
        return ((f & EdgeWallCeiling) && c > std::fabs(right)) || ((f & EdgeGroundCeiling) && right > c);
    // A wall: against a wall, the angle between the two normals must be above WallAngle.
    if ((f & EdgeWallWall) && c > std::fabs(right)) {
        const Vec3 nl = normalized(normalLeft), nr = normalized(normalRight);
        const float d = dot(nl, nr);
        float angle = 0.0f;
        if (d < 1.0f) angle = d <= -1.0f ? 3.1415927f : std::fabs(std::acos(d));
        if (angle > m_WallAngle) return true;
    }
    if ((f & EdgeWallGround) && right > c) return true;
    if (!(f & EdgeWallCeiling)) return false;
    return -c > right;
}

Vec3 GuidanceSystemComponent::GetPoint(std::uint32_t index) const {
    const std::uint16_t* p = &m_CompressPoints[3 * static_cast<size_t>(index)];
    return Vec3{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])} * kCompressStep -
           Vec3{kCompressRange, kCompressRange, kCompressRange};
}

std::uint32_t GuidanceSystemComponent::PortAddPoint(const Vec3& local) {
    auto encode = [](float v) {
        long q = std::lround((v + kCompressRange) / kCompressStep);
        if (q < 0) q = 0;
        if (q > 0xFFFF) q = 0xFFFF;
        return static_cast<std::uint16_t>(q);
    };
    m_CompressPoints.push_back(encode(local.x));
    m_CompressPoints.push_back(encode(local.y));
    m_CompressPoints.push_back(encode(local.z));
    return static_cast<std::uint32_t>(m_CompressPoints.size() / 3 - 1);
}

bool GuidanceSystemComponent::IsValid(const GuidanceObject& object) const {
    if (!object.GetValid()) return false;
    return m_EdgeFilter.AcceptEdge(m_PortGlobalMatrix.TransformVector(object.GetLineNormalLeft()),
                                   m_PortGlobalMatrix.TransformVector(object.GetLineNormalRight()));
}

void GuidanceSystemComponent::PortGetAABBox(Vec3& min, Vec3& max) const {
    min = Vec3{3.4e38f, 3.4e38f, 3.4e38f};
    max = Vec3{-3.4e38f, -3.4e38f, -3.4e38f};
    for (size_t i = 0; i < m_CompressPoints.size() / 3; ++i) {
        const Vec3 p = m_PortGlobalMatrix.TransformVertex(GetPoint(static_cast<std::uint32_t>(i)));
        for (int k = 0; k < 3; ++k) {
            Lane(min, k) = std::fmin(Lane(min, k), Lane(p, k));
            Lane(max, k) = std::fmax(Lane(max, k), Lane(p, k));
        }
    }
}

// --- Zones ---

bool GuidanceZone::CastWorld(std::vector<GuidanceContactObject>& report, bool cut, std::uint32_t subTypeMask,
                             const port::World& world, bool clear) const {
    if (clear) report.clear();
    GuidanceReportAccumulator accumulator{report, subTypeMask};
    Vec3 min, max;
    GetAABBox(min, max);
    // PORT: the broadphase (PhysicWorkspace::GetOverlappingEntities / Havok) is the world's own overlap test, and the
    // components come straight from it (CastEntityInternal's MultiInertComponent split has no counterpart).
    std::vector<const GuidanceSystemComponent*> components;
    world.GetGuidanceSystemComponents(min, max, components);
    bool any = false;
    for (const GuidanceSystemComponent* component : components) {
        if (!component->m_PortCastEnabled) continue;  // CastEntityInternal: +4 & 0x4000000
        any = Cast(accumulator, *component, cut) || any;
    }
    return any;
}

void GuidanceZoneSphere::GetAABBox(Vec3& min, Vec3& max) const {
    const Vec3 r{m_Radius, m_Radius, m_Radius};
    min = m_Matrix.m_Rows[3] - r;
    max = m_Matrix.m_Rows[3] + r;
}

bool GuidanceZoneSphere::Cast(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
                              bool cut) const {
    // GuidanceZoneSphere::Cast (PC 0x3FB3E0) -> InternalCollideWithZoneSphere (PC 0x40F390, Mac 0x3F0ED0). PORT: the
    // component's vftable +0xC4 check that opens it isn't decoded (taken as passing).
    if (!component.m_PortCastEnabled) return false;
    const Vec3 centre = component.m_PortGlobalMatrix.GetAffineInverse().TransformVertex(m_Matrix.m_Rows[3]);
    bool any = false;
    for (const GuidanceObject& object : component.m_GuidanceObjects) {
        if (!component.IsValid(object)) continue;
        const Vec3 p0 = component.GetPoint(object.GetLineIndex0());
        const Vec3 p1 = component.GetPoint(object.GetLineIndex1());
        if (cut) {
            // IncludeCutWithSphere (PC 0x416430, Mac 0x803A60), the cut to world (GuidanceCutObject::Transform).
            Vec3 a, b;
            if (!IntersectSegmentWithSphere(p0, p1, centre, m_Radius, a, b)) continue;
            AddContactPoint(accumulator, component, object, component.m_PortGlobalMatrix.TransformVertex(a),
                            component.m_PortGlobalMatrix.TransformVertex(b));
        } else {
            // CollideWithSphere (PC 0x416260, Mac 0x803360).
            if (!IntersectSegmentWithSphere(p0, p1, centre, m_Radius)) continue;
            AddContactPoint(accumulator, component, object);
        }
        any = true;
    }
    return any;
}

void GuidanceZoneBox::GetAABBox(Vec3& min, Vec3& max) const {
    // PORT: not decoded for the box; the AABB of its eight corners.
    min = Vec3{3.4e38f, 3.4e38f, 3.4e38f};
    max = Vec3{-3.4e38f, -3.4e38f, -3.4e38f};
    for (int i = 0; i < 8; ++i) {
        const Vec3 corner{(i & 1) ? m_Max.x : m_Min.x, (i & 2) ? m_Max.y : m_Min.y, (i & 4) ? m_Max.z : m_Min.z};
        const Vec3 p = m_Matrix.TransformVertex(corner);
        for (int k = 0; k < 3; ++k) {
            Lane(min, k) = std::fmin(Lane(min, k), Lane(p, k));
            Lane(max, k) = std::fmax(Lane(max, k), Lane(p, k));
        }
    }
}

bool GuidanceZoneBox::Cast(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
                           bool cut) const {
    // InternalCollideWithZoneBox (Mac 0x3F08D0): the box's frame in the component's local space (the entity's affine
    // inverse after the zone's matrix).
    if (!component.m_PortCastEnabled) return false;
    const Gear::Matrix44 box = m_Matrix * component.m_PortGlobalMatrix.GetAffineInverse();
    bool any = false;
    for (const GuidanceObject& object : component.m_GuidanceObjects) {
        if (!component.IsValid(object)) continue;
        const Vec3 p0 = component.GetPoint(object.GetLineIndex0());
        const Vec3 p1 = component.GetPoint(object.GetLineIndex1());
        if (cut) {
            Vec3 a, b;  // IncludeCutWithBox (Mac 0x803BC0)
            if (!IntersectSegmentOOBBox(p0, p1, box, m_Min, m_Max, a, b)) continue;
            AddContactPoint(accumulator, component, object, component.m_PortGlobalMatrix.TransformVertex(a),
                            component.m_PortGlobalMatrix.TransformVertex(b));
        } else {
            if (!IntersectSegmentWithOOBBox(p0, p1, box, m_Min, m_Max)) continue;  // CollideWithBox (Mac 0x8034B0)
            AddContactPoint(accumulator, component, object);
        }
        any = true;
    }
    return any;
}

// --- Geometry ---

Vec3 PointSegmentProjection(const Vec3& p, const Vec3& a, const Vec3& b) {
    const Vec3 ab = b - a;
    const float t = dot(p - a, ab);
    if (t <= 0.0f) return a;
    const float len2 = dot(ab, ab);
    if (t < len2) return ab * (t / len2) + a;
    return b;
}

bool IntersectSegmentWithSphere(const Vec3& p0, const Vec3& p1, const Vec3& centre, float radius) {
    const Vec3 d = PointSegmentProjection(centre, p0, p1) - centre;
    return radius * radius > dot(d, d);
}

bool IntersectSegmentWithSphere(const Vec3& p0, const Vec3& p1, const Vec3& centre, float radius, Vec3& cutStart,
                                Vec3& cutEnd) {
    const Vec3 d = p1 - p0;
    const Vec3 m = p0 - centre;
    const float a = dot(d, d);
    const float b = dot(m, d);
    const float disc = b * b - (dot(m, m) - radius * radius) * a;
    if (disc < 0.0f) return false;
    if (disc <= 0.0f) {
        const float t = -b / a;
        if (t < 0.0f || t > 1.0f) return false;
        cutStart = p0 + d * t;
        cutEnd = p1;
        return true;
    }
    const float s = std::sqrt(disc);
    const float inv = 1.0f / a;
    const float t0 = (-b - s) * inv;
    const float t1 = (s - b) * inv;
    if (t0 > 1.0f) return false;
    if (t0 < 0.0f) {
        if (t1 < 0.0f) return false;
        const Vec3 e = d * t1;
        cutEnd = a > dot(e, e) ? p0 + e : p1;
        cutStart = p0;
        return true;
    }
    if (t1 > 1.0f) {
        cutStart = p0 + d * t0;
        cutEnd = p1;
        return true;
    }
    cutStart = p0 + d * t0;
    cutEnd = d * t1 + p0;
    return true;
}

bool IntersectSegmentWithOOBBox(const Vec3& p0, const Vec3& p1, const Gear::Matrix44& box, const Vec3& min,
                                const Vec3& max) {
    // A separating-axis test: the box's three axes, then the three axes h x m against each box axis.
    const Vec3 centre = box.TransformVertex((min + max) * 0.5f);
    const Vec3 h = (p1 - p0) * 0.5f;
    const Vec3 ext = (max - min) * 0.5f;
    const Vec3 m = p0 + h - centre;
    const Vec3* axis = box.m_Rows;
    const float ad0 = std::fabs(dot(h, axis[0]));
    if (std::fabs(dot(m, axis[0])) > ad0 + ext.x) return false;
    const float ad1 = std::fabs(dot(h, axis[1]));
    if (std::fabs(dot(m, axis[1])) > ad1 + ext.y) return false;
    const float ad2 = std::fabs(dot(h, axis[2]));
    if (std::fabs(dot(m, axis[2])) > ad2 + ext.z) return false;
    const Vec3 c = cross(h, m);
    auto separated = [](float r, float e) { return (r > 0.0005f || std::fabs(e) > 0.0005f) && r > e; };
    if (separated(std::fabs(dot(axis[0], c)), ad2 * ext.y + ad1 * ext.z)) return false;
    if (separated(std::fabs(dot(axis[1], c)), ad2 * ext.x + ad0 * ext.z)) return false;
    if (separated(std::fabs(dot(axis[2], c)), ad1 * ext.x + ad0 * ext.y)) return false;
    return true;
}

bool IntersectRayAABBox(const Vec3& origin, const Vec3& dir, const Vec3& min, const Vec3& max, Vec3& hit) {
    // Woo's test (Graphics Gems): the candidate planes, the largest distance, the hit checked on the other slabs.
    bool inside = true;
    Vec3 maxT{-1, -1, -1};
    for (int i = 0; i < 3; ++i) {
        float candidate;
        if (Lane(min, i) > Lane(origin, i))
            candidate = Lane(min, i);
        else if (Lane(origin, i) > Lane(max, i))
            candidate = Lane(max, i);
        else
            continue;
        inside = false;
        Lane(hit, i) = candidate;
        if (Lane(dir, i) != 0.0f) Lane(maxT, i) = (candidate - Lane(origin, i)) / Lane(dir, i);
    }
    if (inside) {
        hit = origin;
        return true;
    }
    int plane = maxT.y > maxT.x ? 1 : 0;
    if (maxT.z > Lane(maxT, plane)) plane = 2;
    const float t = Lane(maxT, plane);
    if (!(t >= 0.0f)) return false;
    for (int i = 0; i < 3; ++i) {
        if (i == plane) continue;
        const float v = Lane(origin, i) + t * Lane(dir, i);
        Lane(hit, i) = v;
        if (Lane(min, i) > v || v > Lane(max, i)) return false;
    }
    return true;
}

bool IntersectSegmentAABBox(const Vec3& p0, const Vec3& p1, const Vec3& min, const Vec3& max, Vec3& cutStart,
                            Vec3& cutEnd) {
    const Vec3 d = p1 - p0;
    if (!IntersectRayAABBox(p0, d, min, max, cutStart)) return false;
    return IntersectRayAABBox(p1, d * -1.0f, min, max, cutEnd);
}

bool IntersectSegmentOOBBox(const Vec3& p0, const Vec3& p1, const Gear::Matrix44& box, const Vec3& min,
                            const Vec3& max, Vec3& cutStart, Vec3& cutEnd) {
    // Matrix44::SetInverse: the port's frames are orthonormal, so the affine inverse.
    const Gear::Matrix44 inverse = box.GetAffineInverse();
    Vec3 a, b;
    if (!IntersectSegmentAABBox(inverse.TransformVertex(p0), inverse.TransformVertex(p1), min, max, a, b))
        return false;
    cutStart = box.TransformVertex(a);
    cutEnd = box.TransformVertex(b);
    return true;
}

}  // namespace scimitar
