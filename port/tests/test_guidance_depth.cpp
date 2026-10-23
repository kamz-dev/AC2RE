// The guidance depth detector and the occluded report (RE/19_guidance.md 6): the pull-down's call, a 1.2 m strip
// turned 60 degrees down off each ledge line, sliced against the collision meshes.

#include <algorithm>
#include <cmath>

#include "check.h"
#include "port/world.h"
#include "scimitar/guidance_depth.h"

using scimitar::GuidanceContactObject;
using scimitar::GuidanceDepthReport;
using scimitar::Vec3;

namespace {
// The 8 m roof's north edge (y = 5, z = 8, x from -5 to 5), as a guidance cast reports it.
std::vector<GuidanceContactObject> NorthEdge(const port::BoxWorld& world) {
    scimitar::GuidanceZoneSphere zone;
    zone.m_Matrix.m_Rows[3] = {0, 5, 8};
    zone.m_Radius = 0.5f;
    std::vector<GuidanceContactObject> report;
    zone.CastWorld(report, false, scimitar::kGuidanceContactObjectSubType_All, world);
    return report;
}

// The pull-down's calls (HumanLedge::CastPullDownGuidanceReport): depth 1.2, -60 degrees, offset (0, 0.02, 0.02);
// clearance 1.2.
std::vector<GuidanceContactObject> Occluded(const port::BoxWorld& world, const std::vector<GuidanceContactObject>& in,
                                            std::vector<GuidanceDepthReport>* depthsOut = nullptr) {
    std::vector<GuidanceDepthReport> depths;
    scimitar::CastGuidanceDepthDetector(in, world, depths, 1.2f, -1.0471976f, {0, 0.02f, 0.02f});
    std::vector<GuidanceContactObject> out;
    scimitar::CreateOccludedGuidanceReport(in, depths, 1.2f, out);
    if (depthsOut) *depthsOut = depths;
    return out;
}

port::BoxWorld Roof() {
    port::BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 8}});
    return world;
}

float MinX(const GuidanceContactObject& c) { return std::min(c.m_LineGlobalPoint0.x, c.m_LineGlobalPoint1.x); }
float MaxX(const GuidanceContactObject& c) { return std::max(c.m_LineGlobalPoint0.x, c.m_LineGlobalPoint1.x); }
}  // namespace

TEST(depth_detector_keeps_an_edge_over_an_open_drop) {
    const port::BoxWorld world = Roof();
    const auto edge = NorthEdge(world);
    CHECK(edge.size() == 1);
    std::vector<GuidanceDepthReport> depths;
    const auto out = Occluded(world, edge, &depths);
    CHECK(depths.size() == 1 && depths[0].m_Tracks.size() == 1);
    // 10 m of edge plus 0.5 m overshoot at each end, in 0.1 m bins, all at the full 1.2 m.
    const auto& track = depths[0].m_Tracks[0];
    CHECK(track.m_Depths.size() == 111);
    CHECK(*std::min_element(track.m_Depths.begin(), track.m_Depths.end()) >= 1.2f - 1e-5f);
    CHECK(out.size() == 1);
    CHECK_NEAR(MinX(out[0]), -5.0f, 1e-3f);
    CHECK_NEAR(MaxX(out[0]), 5.0f, 1e-3f);
    // The clearance before and after the piece: the overshoot, free.
    CHECK_NEAR(out[0].m_Floats[0], 0.5f, 1e-3f);
    CHECK_NEAR(out[0].m_Floats[1], 0.5f, 1e-3f);
}

TEST(depth_detector_cuts_the_edge_where_a_balcony_blocks_the_strip) {
    // A balcony 0.5 m under the edge, 1 m deep, from x = -1 to 1: the strip (60 degrees down) meets its top 0.6 m out.
    port::BoxWorld world = Roof();
    world.add({{-1, 5, 0}, {1, 6, 7.5f}});
    const auto out = Occluded(world, NorthEdge(world));
    CHECK(out.size() == 2);
    const GuidanceContactObject& a = MinX(out[0]) < MinX(out[1]) ? out[0] : out[1];
    const GuidanceContactObject& b = MinX(out[0]) < MinX(out[1]) ? out[1] : out[0];
    CHECK_NEAR(MinX(a), -5.0f, 1e-3f);
    CHECK_NEAR(MaxX(a), -1.0f, 0.15f);  // to the 0.1 m bin
    CHECK_NEAR(MinX(b), 1.0f, 0.15f);
    CHECK_NEAR(MaxX(b), 5.0f, 1e-3f);
}

TEST(depth_detector_drops_an_edge_facing_a_near_wall) {
    // A building 0.5 m across the street, taller: the strip hits its wall everywhere.
    port::BoxWorld world = Roof();
    world.add({{-6, 5.5f, 0}, {6, 9, 12}});
    CHECK(Occluded(world, NorthEdge(world)).empty());
}

TEST(depth_detector_zeroes_the_edge_inside_a_pillar) {
    // A pillar the edge line runs through (x from 2 to 3): its faces cross the edge, the intersections bracket the
    // inside, and those bins get depth 0 (GuidanceDepthTrack::ProcessIntersections).
    port::BoxWorld world = Roof();
    world.add({{2, 4.8f, 0}, {3, 5.2f, 10}});
    const auto edge = NorthEdge(world);
    std::vector<GuidanceDepthReport> depths;
    const auto out = Occluded(world, edge, &depths);
    const auto& track = depths[0].m_Tracks[0];
    CHECK(!track.m_Intersections.empty());
    // The track runs from x = 5.5 (the edge's p0 at x = 5, minus the overshoot) towards -5.5: x = 2.5 is t = 3.0.
    CHECK_NEAR(track.m_Depths[30], 0.0f, 1e-6f);
    CHECK(out.size() == 2);
    const float gapLo = std::min(MaxX(out[0]), MaxX(out[1]));
    const float gapHi = std::max(MinX(out[0]), MinX(out[1]));
    CHECK_NEAR(gapLo, 2.0f, 0.15f);
    CHECK_NEAR(gapHi, 3.0f, 0.15f);
}
