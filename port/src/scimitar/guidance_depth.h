#pragma once
// The guidance depth detector and the occluded report (RE/19_guidance.md 6; Mac addresses, PC not mapped). For each
// guidance line, a strip hanging off it (out from the wall, turned about the edge) is sliced against the world's
// collision meshes; the nearest geometry along the line is binned into a depth track, and the parts of the line
// whose strip stays clear are kept, with the clearance before and after them.

#include <cstdint>
#include <vector>

#include "scimitar/guidance.h"
#include "scimitar/math.h"

namespace port {
class World;
}

namespace scimitar {

// scimitar::GuidanceDepthDetectorSettings (0x20).
struct GuidanceDepthDetectorSettings {
    Vec3 m_Offset;           // +0x00 the strip's origin offset (x along the edge, y out, z up)
    float m_Angle = 0;       // +0x10 the strip turned about the edge (rad; the pull-down's: -60 degrees)
    float m_MaxDepth = 0;    // +0x14 the strip's depth; a bin's default depth
};

// scimitar::GuidanceIntersection (0xC): where geometry meets the edge line.
struct GuidanceIntersection {
    float m_T = 0;               // +0 along the track
    std::uint32_t m_Direction = 0;  // +4 1 when the triangle's normal points against the edge (dot <= 0)
    long m_Level = 0;            // +8 the nesting level
};

struct GuidanceClearanceInterval {
    float m_T0 = 0, m_T1 = 0;
};
struct GuidanceClearanceInfo {
    float m_Clearance0 = 0, m_Clearance1 = 0;
};

// scimitar::GuidanceWorkspacePlane (0x60): a strip, workspace-local.
struct GuidanceWorkspacePlane {
    Gear::Matrix44 m_Matrix;  // +0x00 X the slicing plane's normal, Y the depth, Z along the edge, T the start
    Vec3 m_Offset;            // +0x40 the world offset added to the origin
    float m_Length = 0;       // +0x50 along Y (MaxDepth)
    float m_Height = 0;       // +0x54 along Z (the line's length + 2 overshoots)
    const GuidanceContactObject* m_Parent = nullptr;  // +0x58 the input line
};

// scimitar::GuidanceDepthTrack (0xB0; Mac ctor 0x697EB0).
struct GuidanceDepthTrack {
    Vec3 m_GlobalOffset;            // +0x00
    Gear::Matrix44 m_PlaneMatrix;   // +0x10 global
    float m_Length = -1;            // +0x50 along Z
    float m_Resolution = -1;        // +0x54 bin size
    float m_Overshoot = -1;         // +0x58
    std::vector<float> m_Depths;    // +0x5C one per bin
    std::vector<GuidanceIntersection> m_Intersections;  // +0x64
    Gear::Matrix44 m_InvPlaneMatrix;  // +0x70

    void SetupTrack(const Gear::Matrix44& workspace, const GuidanceWorkspacePlane& plane, float resolution,
                    float overshoot);                                        // Mac 0x696EA0
    void AddSegment(const GuidanceContactObject& contact);                   // Mac 0x698620
    void AddIntersection(float t, const Vec3& axis, const GuidanceContactObject& contact);  // Mac 0x698380
    void ProcessIntersections();                                             // Mac 0x698A60
    void GetMaskFromValidIntervals(const std::vector<GuidanceClearanceInterval>& intervals,
                                   std::vector<bool>& mask) const;           // Mac 0x697980
    void GetClearanceIntervals(const std::vector<bool>& mask,
                               std::vector<GuidanceClearanceInterval>& intervals) const;  // Mac 0x697160
    void GetOvershootClearances(float clearance, float& c0, float& c1) const;  // Mac 0x6963C0
    void GetClearanceReportFromIntervals(const std::vector<GuidanceClearanceInterval>& intervals,
                                         const GuidanceContactObject& contact,
                                         std::vector<GuidanceContactObject>& out) const;  // Mac 0x696760
    void FillClearanceInfo(std::uint32_t track, float clearance, GuidanceContactObject& piece) const;  // 0x696C30
};

// scimitar::GuidanceDepthReport (0xC): one track per input line.
struct GuidanceDepthReport {
    std::vector<GuidanceDepthTrack> m_Tracks;
};

// scimitar::GuidanceDepthDetector (Mac ctor 0x3EA420): Resolution 0.1, Overshoot 0 (the Human's detector is set to
// 0.1 / 0.5 before each cast, HumanGuidance::SetupDepthDetector).
struct GuidanceDepthDetector {
    float m_Resolution = 0.1f;
    float m_Overshoot = 0.0f;
    std::vector<GuidanceDepthDetectorSettings> m_Settings;

    // HumanGuidance::GetDepthReports (Mac 0xF05BF6): the workspace around the report (its endpoints' AABB grown by
    // the largest MaxDepth), AddTasksToWorkspace (0x3E8D50), the broadphase and the slicing
    // (GuidanceWorkspaceCastPlanes::ProcessTriangles2 0x493F00), FillReports (0x3EA920). `out` is left untouched
    // when the report is empty.
    void GetDepthReports(const std::vector<GuidanceContactObject>& report, const port::World& world,
                         std::vector<GuidanceDepthReport>& out) const;

    // GetCombinedGuidanceReport (Mac 0x3EB6E0) with FillClearanceInfos (0x3E8A70): the contact cut to the bins clear
    // to `clearances[k]` in every track k, appended to `out`.
    void GetCombinedGuidanceReport(const GuidanceContactObject& contact,
                                   const std::vector<const GuidanceDepthTrack*>& tracks,
                                   const std::vector<float>& clearances,
                                   std::vector<GuidanceContactObject>& out) const;
};

// Human::CastGuidanceDepthDetector (Mac 0x12C8CA4 -> core 0x12C5056): one settings entry {offset, angle, maxDepth},
// the detector at Resolution 0.1, Overshoot 0.5.
void CastGuidanceDepthDetector(const std::vector<GuidanceContactObject>& report, const port::World& world,
                               std::vector<GuidanceDepthReport>& out, float maxDepth, float angle, const Vec3& offset);
// Human::CreateOccludedGuidanceReport (Mac 0x12C9FCE -> core 0x12C81C0): each line cut to its clear parts; appends.
void CreateOccludedGuidanceReport(const std::vector<GuidanceContactObject>& report,
                                  const std::vector<GuidanceDepthReport>& depthReports, float clearance,
                                  std::vector<GuidanceContactObject>& out);

// IntersectFinitePlaneWithTriangleEx (Mac 0x1C6190): the part of the triangle's cut by the plane (M: X its normal,
// Y / Z its extents L / H from T) inside the rectangle; d0..d2 the vertices' signed distances.
bool IntersectFinitePlaneWithTriangleEx(const Gear::Matrix44& plane, float length, float height, const Vec3& v0,
                                        const Vec3& v1, const Vec3& v2, float d0, float d1, float d2, Vec3& a,
                                        Vec3& b);

}  // namespace scimitar
