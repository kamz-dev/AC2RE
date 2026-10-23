#pragma once
// Guidance: the climbable-geometry data on world entities and the zone casts that find it (RE/19_guidance.md).
// Names, layouts and rules from the game (Mac symbols, matched on PC; layouts from the Revelations PDB and AC2's
// reflection). An entity carries GuidanceSystemComponents: packed lines between compressed points, in the entity's
// local space. A GuidanceZone (sphere, box) cast collects the lines it touches as GuidanceContactObjects in world space.

#include <cstdint>
#include <vector>

#include "scimitar/math.h"

namespace port {
class World;
}

namespace scimitar {

// scimitar::GuidanceObjectSubType (Revelations PDB; AC2 not checked past what the code shows).
enum GuidanceObjectSubType : std::uint32_t {
    GuidanceObjectSubTypeNone = 0,
    GuidanceObjectSubTypeLedgeGrab = 1,
    GuidanceObjectSubTypeBeam = 2,
    GuidanceObjectSubTypeLadder = 3,
    GuidanceObjectSubTypePole = 4,
    GuidanceObjectSubTypeRope = 5,
    GuidanceObjectSubTypeSurface = 6,
    GuidanceObjectSubTypeQuadruped = 7,
    GuidanceObjectSubTypeKiosk = 8,
    GuidanceObjectSubTypeBoatPlank = 9,
    GuidanceObjectSubTypeBoatInnerEndEdge = 10,
    GuidanceObjectSubTypeBoatInnerSideEdge = 11,
    GuidanceObjectSubTypeBoatMiddleEdge = 12,
    GuidanceObjectSubTypeHayStack = 13,
    GuidanceObjectSubTypeZipLine = 14,
};

// A cast's subtype mask: GuidanceContactObjectSubType is the subtype's bit; -1 takes all.
inline constexpr std::uint32_t kGuidanceContactObjectSubType_All = 0xFFFFFFFFu;

// Gear::VectorDecN4: x, y, z as signed 10-bit fields (bits 0, 10, 20) of 511ths, w the signed 2 bits at 30.
// Gear::Math::SSEIntrinsics::LoadDecN4 / StoreDecN4 (Mac GuidanceObject::GetNormal0 0xA4B5A0 / SetNormal0 0xA4B880).
Vec3 LoadDecN4(std::uint32_t packed);
std::uint32_t StoreDecN4(const Vec3& v);

// scimitar::GuidanceObject (0xC; reflected PC 0x1CB66A0): one line between two of its component's points.
struct GuidanceObject {
    // +0 m_Packing (PC bits, LSB first): bit 0 Valid, 1-5 SubType, 6-18 Index0, 19-31 Index1.
    std::uint32_t m_Packing = 0;
    std::uint32_t m_Normal0 = 0;  // +4 LineNormalLeft (VectorDecN4)
    std::uint32_t m_Normal1 = 0;  // +8 LineNormalRight

    bool GetValid() const { return (m_Packing & 1) != 0; }
    std::uint32_t GetSubType() const { return (m_Packing >> 1) & 0x1F; }
    std::uint32_t GetLineIndex0() const { return (m_Packing >> 6) & 0x1FFF; }  // PC 0x415AF0, Mac 0xA4B360
    std::uint32_t GetLineIndex1() const { return m_Packing >> 19; }           // PC 0x415B00, Mac 0xA4B350
    Vec3 GetLineNormalLeft() const { return LoadDecN4(m_Normal0); }    // PC 0x416B50, Mac 0xA4B420
    Vec3 GetLineNormalRight() const { return LoadDecN4(m_Normal1); }   // PC 0x416B90, Mac 0xA4B4E0

    void SetValid(bool valid) { m_Packing = (m_Packing & ~1u) | (valid ? 1u : 0u); }
    void SetSubType(std::uint32_t t) { m_Packing = (m_Packing & 0xFFFFFFC1u) | ((t & 0x1F) << 1); }  // Mac 0xA4B2B0
    void SetLineIndex0(std::uint32_t i) { m_Packing = (m_Packing & ~(0x1FFFu << 6)) | ((i & 0x1FFF) << 6); }
    void SetLineIndex1(std::uint32_t i) { m_Packing = (m_Packing & 0x7FFFFu) | (i << 19); }
    void SetLineNormalLeft(const Vec3& n) { m_Normal0 = StoreDecN4(n); }
    void SetLineNormalRight(const Vec3& n) { m_Normal1 = StoreDecN4(n); }
};

// scimitar::EdgeFilter (0xC): which pairs of faces make an edge (RE/19 1).
struct EdgeFilter {
    // m_Filter bits (PC, LSB first; the PDB's are the mirror).
    enum : std::uint8_t {
        EdgeWallWall = 0x01,
        EdgeWallGround = 0x02,
        EdgeWallCeiling = 0x04,
        EdgeWallVoid = 0x08,
        EdgeGroundCeiling = 0x10,
        EdgeGroundVoid = 0x20,
        EdgeCeilingVoid = 0x40,
    };
    // EdgeFilter::Reset (Mac 0x450E20): DEFAULT_SLOPE_COS_ANGLE, DEFAULT_WALL_ANGLE, no bits.
    float m_SlopeCosAngle = 0.70703125f;  // +0
    float m_WallAngle = 0.3926991f;       // +4
    std::uint8_t m_Filter = 0;            // +8

    // PC 0x4858E0, Mac 0x4513B0: the faces by their normals' z (ground, wall, ceiling); the pair's bit.
    bool AcceptEdge(const Vec3& normalLeft, const Vec3& normalRight) const;
};

// scimitar::GuidanceContactObject (0x60; reflected PC 0x1CB70B8): a line in world space, as a cast returns it.
struct GuidanceSystemComponent;
struct GuidanceContactObject {
    Vec3 m_LineGlobalPoint0;       // +0x10
    Vec3 m_LineGlobalPoint1;       // +0x20
    Vec3 m_LineGlobalNormalLeft;   // +0x30
    Vec3 m_LineGlobalNormalRight;  // +0x40
    // +0x50 m_Entity. PORT: the component stands in for its owner entity.
    const GuidanceSystemComponent* m_Entity = nullptr;
    const GuidanceObject* m_ParentGuidanceObject = nullptr;  // +0x54
    std::uint8_t m_SubType = 0;                              // +0x5C bits 0-5
};

// scimitar::GuidanceSystemComponent (0x2C; reflected PC 0x1CB5EE8, vftable PC 0x18D5DC0).
struct GuidanceSystemComponent {
    // PORT: the owner entity's global matrix (the game reads it through Component +8, the entity).
    Gear::Matrix44 m_PortGlobalMatrix;
    bool m_PortCastEnabled = true;               // +4 & 0x4000000: the component takes part in casts
    std::vector<std::uint16_t> m_CompressPoints; // +0x0C, 3 per point
    std::vector<GuidanceObject> m_GuidanceObjects;  // +0x14
    EdgeFilter m_EdgeFilter;                     // +0x1C
    bool m_Dynamic = false;                      // +0x28

    // The local position of point i: u16 x 0.005 - g_CompressVector3To48_Range (163.84) per axis (PC 0x3FDBC0).
    Vec3 GetPoint(std::uint32_t index) const;
    // PORT: appends a point (rounded to the nearest 5 mm; the game's points are precompiled data) and returns its index.
    std::uint32_t PortAddPoint(const Vec3& local);
    // PC 0x40EFE0, Mac 0x9F79E0: the Valid bit and the EdgeFilter on the two normals turned to world.
    bool IsValid(const GuidanceObject& object) const;
    // PORT: the component's world AABB, for the port's stand-in broadphase.
    void PortGetAABBox(Vec3& min, Vec3& max) const;
};

// scimitar::GuidanceReportAccumulator: the report and the cast's subtype mask.
struct GuidanceReportAccumulator {
    std::vector<GuidanceContactObject>& m_Report;
    std::uint32_t m_SubTypeMask;
};

// scimitar::GuidanceZone: +0x10 m_Matrix (the zone's own), +0x50 m_ParentEntity (PORT: none; the matrix is global).
struct GuidanceZone {
    Gear::Matrix44 m_Matrix;
    virtual ~GuidanceZone() = default;
    virtual void GetAABBox(Vec3& min, Vec3& max) const = 0;  // vftable +0x20
    // vftable +0x28 (GuidanceZoneSphere::Cast PC 0x3FB3E0): an enabled component's InternalCollideWithZone<shape>.
    virtual bool Cast(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
                      bool cut) const = 0;
    // PC 0x3FEA00, Mac 0x62D230: every component in the world near the zone (PORT: the world's stand-in broadphase
    // over the zone's AABB), cast; `cut` keeps only the part of each line inside the zone.
    bool CastWorld(std::vector<GuidanceContactObject>& report, bool cut, std::uint32_t subTypeMask,
                   const port::World& world, bool clear = true) const;
};

// scimitar::GuidanceZoneSphere: +0x60 m_Radius (PC constructor 0x3FAC60 sets 1).
struct GuidanceZoneSphere final : GuidanceZone {
    float m_Radius = 1.0f;
    void GetAABBox(Vec3& min, Vec3& max) const override;  // PC 0x3FB240: the centre +- the radius
    // -> GuidanceSystemComponent::InternalCollideWithZoneSphere (PC 0x40F390, Mac 0x3F0ED0).
    bool Cast(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
              bool cut) const override;
};

// scimitar::GuidanceZoneBox: +0x60 m_Min, +0x70 m_Max (local bounds).
struct GuidanceZoneBox final : GuidanceZone {
    Vec3 m_Min, m_Max;
    void GetAABBox(Vec3& min, Vec3& max) const override;
    // -> GuidanceSystemComponent::InternalCollideWithZoneBox (Mac 0x3F08D0).
    bool Cast(GuidanceReportAccumulator& accumulator, const GuidanceSystemComponent& component,
              bool cut) const override;
};

// Geometry the casts use (Mac addresses).
// p moved along `dir` onto the plane (q, n), the cosine clamped to [-1, 1] (Mac 0x212850, PC 0x10EA7E0).
Vec3 ProjectPointOnPlane(const Vec3& p, const Vec3& q, const Vec3& n, const Vec3& dir);
// Where p0-p1 crosses the plane (point, normal), when it does within the segment (Mac 0x1C2E50, PC 0x10FA690).
bool IntersectSegmentWithInfinitePlane(Vec3& hit, const Vec3& p0, const Vec3& p1, const Vec3& planePoint,
                                       const Vec3& planeNormal);
// The closest point of the segment a-b to p (Mac 0x213630, PC 0x10E8F90).
Vec3 PointSegmentProjection(const Vec3& p, const Vec3& a, const Vec3& b);
bool IntersectSegmentWithSphere(const Vec3& p0, const Vec3& p1, const Vec3& centre, float radius);  // 0x1C16D0
bool IntersectSegmentWithSphere(const Vec3& p0, const Vec3& p1, const Vec3& centre, float radius, Vec3& cutStart,
                                Vec3& cutEnd);  // 0x1C2C10
bool IntersectSegmentWithOOBBox(const Vec3& p0, const Vec3& p1, const Gear::Matrix44& box, const Vec3& min,
                                const Vec3& max);  // 0x1C5980
bool IntersectRayAABBox(const Vec3& origin, const Vec3& dir, const Vec3& min, const Vec3& max, Vec3& hit);  // 0x1C13B0
bool IntersectSegmentAABBox(const Vec3& p0, const Vec3& p1, const Vec3& min, const Vec3& max, Vec3& cutStart,
                            Vec3& cutEnd);  // 0x1C1780
bool IntersectSegmentOOBBox(const Vec3& p0, const Vec3& p1, const Gear::Matrix44& box, const Vec3& min,
                            const Vec3& max, Vec3& cutStart, Vec3& cutEnd);  // 0x1C5780

}  // namespace scimitar
