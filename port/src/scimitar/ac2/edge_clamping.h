#pragma once
// Edge clamping: how the game keeps a walking Human from stepping off an edge (RE/11_ground.md 16). The controller
// (AssassinTask::UpdateGroundMovement) finds the gaps around the player and turns the clamping on; Human::Update's
// EdgeClampingComputation (PC 0x931330) then cuts the frame's root motion against the guidance edges near the gap.

#include <vector>

#include "scimitar/math.h"

namespace port {
class World;
}

namespace scimitar::ac2 {

// scimitar::ac2::IHuman::Gap (Mac symbol), 0x40 bytes; member names from Revelations' GapInfo. AC2 swaps the last two
// (Revelations: +0x38 m_EdgeSubType, +0x3C m_NoEdge): the AC2 constructor (Mac 0x146D0EA) writes a byte at +0x38 and a
// dword at +0x3C, and GetGapList's no-edge gap sets +0x38 to 1.
struct Gap {
    Vec3 m_HolePosition;   // +0x00 never written by GetGapList
    Vec3 m_EdgePosition;   // +0x10 the foot of the character's position on the edge
    Vec3 m_EdgeNormal;     // +0x20 flat, pointing away from the wall, out over the drop
    float m_Depth = 0;     // +0x30 the drop in front of the edge
    float m_SquareDistance = 0;  // +0x34 |edge - pos|^2, negative when pos is on the normal's side
    bool m_NoEdge = false;       // +0x38 the gap is the drop under pos itself
    int m_EdgeSubType = 0;       // +0x3C GuidanceObjectSubType (the guidance line's subtype & 0x3F)
};

// scimitar::ac2::EdgeClampingSettings (Mac symbol), at Human +0x9F0 on PC (Mac +0x9C0). The AC2 layout isn't
// Revelations' (+0 direction, +0x10 distance, +0x14 angle); the fields below are named by their use (hypothesis):
// GetModifiedSpeed reads +0 as the edge distance, FilterSegmentListWithNormalAngle +0x10 and +0x20.
struct EdgeClampingSettings {
    float m_EdgeDistance = 0.2f;     // +0x00 the Human constructor (PC 0x932DC0) sets 0.2; nothing else writes it
    Vec3 m_DirectionForRejectAngle;  // +0x10
    float m_RejectAngle = 0;         // +0x20 0: no rejection
};

// scimitar::ac2::Segment (Mac symbol; Revelations layout): +0 m_Points[2], +0x20 m_Normal, +0x30 m_Entity.
struct Segment {
    Vec3 m_Points[2];
    Vec3 m_Normal;
};

// Mac 0x144CD7A, PC 0x5F9EE0: the line isn't degenerate (0.0005) and slopes at most 40 degrees.
bool IsEdgeLeveled(const Vec3& p0, const Vec3& p1);
// GuidanceContactObject::GetLineGlobalFlatNormal (Mac 0x144A2D4, PC 0x414140): the horizontal normal of the line,
// on the side of the lower of its two faces (the wall), i.e. pointing out over the drop.
Vec3 GetLineGlobalFlatNormal(const Vec3& p0, const Vec3& p1, const Vec3& faceNormal0, const Vec3& faceNormal1);
// scimitar::PointSegmentProjection (Mac 0x213630, PC 0x10E8F90): the closest point of the segment.
Vec3 PointSegmentProjection(const Vec3& p, const Vec3& a, const Vec3& b);
// scimitar::ProjectPointOnPlane (Mac 0x212850, PC 0x10EA7E0): p moved along `dir` onto the plane (q, n).
Vec3 ProjectPointOnPlane(const Vec3& p, const Vec3& q, const Vec3& n, const Vec3& dir);

// Mac 0xD37540, PC 0x1007D20.
bool GetEdgeSegmentNearCharacterPos(const port::World& world, const Vec3& entityPosition, const Vec3& entitySight,
                                    const Vec3& gapPosition, const Vec3& gapNormal, const Vec3& direction,
                                    float radius, const EdgeClampingSettings& settings, std::vector<Segment>& segments,
                                    unsigned maxCount);
// Mac 0xD36E56, PC 0x10074B0.
void FilterSegmentListWithNormalAngle(std::vector<Segment>& segments, const Vec3& direction, float rejectAngle);
// Mac 0xD3616E, PC 0x1007780. Returns 0 (no hit), 1 (crosses a pulled-back edge plane) or 2 (starts past one).
int DisplacementCollideWithPlane(Vec3& hit, int& index, const Vec3& start, const Vec3& end, float edgeDistance,
                                 const std::vector<Segment>& shifted, const std::vector<Segment>& segments);
// Mac 0xD3675C, PC 0x1009420. Cuts the displacement `d` from `position` against the segments; the result is flat.
bool DisplacementCullingBySegment(Vec3& out, const Vec3& position, const Vec3& d, float edgeDistance,
                                  std::vector<Segment>& segments);

}  // namespace scimitar::ac2
