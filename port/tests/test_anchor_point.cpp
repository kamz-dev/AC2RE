// Anchor points (RE/19_guidance.md 5): the closest point in a box, the next vertical limb position and their helpers.

#include <cmath>

#include "check.h"
#include "port/world.h"
#include "scimitar/ac2/anchor_point.h"

using namespace scimitar::ac2;
using scimitar::GuidanceContactObject;
using scimitar::GuidanceZoneBox;
using scimitar::Vec3;

namespace {
// The 8 m roof: its north edge is y = 5, z = 8.
port::BoxWorld Roof() {
    port::BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 8}});
    return world;
}

// The lines a box around `centre` finds, cut (as a limb search's report).
std::vector<GuidanceContactObject> CutAround(const port::World& world, Vec3 centre, float half) {
    GuidanceZoneBox box;
    box.m_Matrix.m_Rows[3] = centre;
    box.m_Min = {-half, -half, -half};
    box.m_Max = {half, half, half};
    std::vector<GuidanceContactObject> report;
    box.CastWorld(report, true, scimitar::kGuidanceContactObjectSubType_All, world);
    return report;
}
}  // namespace

TEST(project_point_on_segment_keeps_min_dist_from_the_ends) {
    const Vec3 p = ProjectPointOnSegmentWithMinDist({-3, 1, 0}, {0, 0, 0}, {4, 0, 0}, 0.5f);
    CHECK_NEAR(p.x, 0.5f, 1e-6f);  // clamped to the shortened segment
    const Vec3 mid = ProjectPointOnSegmentWithMinDist({-3, 1, 0}, {0, 0, 0}, {0.9f, 0, 0}, 0.5f);
    CHECK_NEAR(mid.x, 0.45f, 1e-6f);  // too short: the middle
}

TEST(best_point_in_plane_is_the_crossing_or_the_nearer_end) {
    Vec3 p = GetBestPointOnSegmentInPlane({0.3f, 0, 0}, {1, 0, 0}, {-1, 2, 0}, {1, 2, 0}, 0);
    CHECK_NEAR(p.x, 0.3f, 1e-6f);
    p = GetBestPointOnSegmentInPlane({5, 0, 0}, {1, 0, 0}, {-1, 2, 0}, {1, 2, 0}, 0);
    CHECK_NEAR(p.x, 1.0f, 1e-6f);
}

TEST(closest_point_in_box_finds_the_pull_down_anchor) {
    // HumanLedge::EnterPullDown's call (Mac 0x133E8DA): at the gap's edge, Y = -normal, Z = K, 0.1 m half extents,
    // 45 degrees, the box centre, LedgeGrab / Pole / Rope / HayStack.
    const port::BoxWorld world = Roof();
    AnchorPoint anchor;
    CHECK(GetClosestPointInBoxWithOffset(anchor, world, nullptr, {1, 5, 8}, {0, -1, 0}, {0, 0, 1}, 0.1f, 0.1f, 0.1f,
                                         0.78539819f, {0, 0, 0}, true, 0.0f, 0x2032));
    CHECK_NEAR(anchor.m_GlobalPosition.x, 1.0f, 1e-3f);
    CHECK_NEAR(anchor.m_GlobalPosition.y, 5.0f, 1e-3f);
    CHECK_NEAR(anchor.m_GlobalPosition.z, 8.0f, 1e-3f);
    CHECK_NEAR(anchor.m_GlobalNormal.y, 1.0f, 1e-3f);
    CHECK(anchor.m_AnchorPointType == AnchorPointType_Ledge);
    CHECK(anchor.m_Object != nullptr);
    CHECK_NEAR(anchor.m_LocalObjectPosition.y, 5.0f, 1e-3f);  // the box entity sits at (0, 0, 8)
    CHECK_NEAR(anchor.m_LocalObjectPosition.z, 0.0f, 1e-3f);
    // Facing the other way, the edge doesn't oppose Y: nothing.
    CHECK(!GetClosestPointInBoxWithOffset(anchor, world, nullptr, {1, 5, 8}, {0, 1, 0}, {0, 0, 1}, 0.1f, 0.1f,
                                          0.1f, 0.78539819f, {0, 0, 0}, true, 0.0f, 0x2032));
    // A Beam-only mask: nothing either.
    CHECK(!GetClosestPointInBoxWithOffset(anchor, world, nullptr, {1, 5, 8}, {0, -1, 0}, {0, 0, 1}, 0.1f, 0.1f,
                                          0.1f, 0.78539819f, {0, 0, 0}, true, 0.0f, 1u << 2));
}

TEST(next_vertical_limb_pos_places_both_hands_on_the_edge) {
    // Hanging at the roof's north edge, facing the wall (south): the frame's X is the right hand's side.
    const port::BoxWorld world = Roof();
    const std::vector<GuidanceContactObject> report = CutAround(world, {0, 5, 8}, 1.0f);
    const Vec3 facing{0, -1, 0};
    AnchorPoint left, right;
    CHECK(GetNextVerticalLimbPos(left, report, {0, 0, 1}, {0, 5, 7.9f}, {0, 0, 1}, facing, -0.3f, 0.3f, 0.7f, -0.25f,
                                 false));
    CHECK(GetNextVerticalLimbPos(right, report, {0, 0, 1}, {0, 5, 7.9f}, {0, 0, 1}, facing, -0.3f, 0.3f, 0.7f, 0.25f,
                                 false));
    // X = facing x K = (-1, 0, 0): lateral +0.25 is world x = -0.25.
    CHECK_NEAR(left.m_GlobalPosition.x, 0.25f, 1e-3f);
    CHECK_NEAR(right.m_GlobalPosition.x, -0.25f, 1e-3f);
    CHECK_NEAR(right.m_GlobalPosition.y, 5.0f, 1e-3f);
    CHECK_NEAR(right.m_GlobalPosition.z, 8.0f, 1e-3f);
    // Too far below the edge (out of z in [-0.3, 0.3]): nothing.
    AnchorPoint none;
    CHECK(!GetNextVerticalLimbPos(none, report, {0, 0, 1}, {0, 5, 7.5f}, {0, 0, 1}, facing, -0.3f, 0.3f, 0.7f, 0, false));
}

TEST(next_vertical_limb_pos_takes_the_highest_line_going_up) {
    // A second, lower ledge 0.2 m under the edge, in the same wall plane: going up picks the top one, going down the
    // lower one.
    port::BoxWorld world = Roof();
    world.add({{-5, 4.9f, 0}, {5, 5, 7.8f}});
    std::vector<GuidanceContactObject> report = CutAround(world, {0, 5, 8}, 1.0f);
    AnchorPoint hand;
    CHECK(GetNextVerticalLimbPos(hand, report, {0, 0, 1}, {0, 5, 7.9f}, {0, 0, 1}, {0, -1, 0}, -0.3f, 0.3f, 0.7f, 0,
                                 false));
    CHECK_NEAR(hand.m_GlobalPosition.z, 8.0f, 1e-3f);
    CHECK(GetNextVerticalLimbPos(hand, report, {0, 0, 1}, {0, 5, 7.9f}, {0, 0, -1}, {0, -1, 0}, -0.3f, 0.3f, 0.7f, 0,
                                 false));
    CHECK_NEAR(hand.m_GlobalPosition.z, 7.8f, 1e-3f);
}
