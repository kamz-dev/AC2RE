// Edge clamping (RE/11_ground.md 16): the gaps the controller finds, the clamp's geometry, and walking up to edges.

#include <cmath>

#include "check.h"
#include "port/sim.h"
#include "scimitar/ac2/edge_clamping.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

using namespace port;
using scimitar::Vec3;
using scimitar::ac2::Gap;
using scimitar::ac2::Segment;

namespace {
// A 10 x 10 m roof `height` above the floor; its north edge is y = 5.
BoxWorld Roof(float height) {
    BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, height}});
    return world;
}

MoveInput Toward(Vec3 dir, unsigned buttons = 0) {
    MoveInput in;
    in.moveDir = scimitar::normalized(dir);
    in.moveMag = 1;
    in.buttons = buttons;
    return in;
}

bool SawInAir(const Sim& sim) {
    for (const Frame& f : sim.timeline())
        if (f.context == HumanContextId::InAir) return true;
    return false;
}
}  // namespace

TEST(edge_segment_cuts_a_displacement_at_the_pulled_back_plane) {
    // DisplacementCullingBySegment: an edge along x at y = 5, normal +y; the plane is pulled back by 0.2.
    std::vector<Segment> segs{Segment{{{-5, 5, 0}, {5, 5, 0}}, {0, 1, 0}}};
    Vec3 out;
    CHECK(scimitar::ac2::DisplacementCullingBySegment(out, {0, 4, 0}, {0.3f, 1.0f, 0}, 0.2f, segs));
    // Hits y = 4.8 at t = 0.8, then slides along the edge to x = 0.3.
    CHECK_NEAR(out.x, 0.3f, 1e-5f);
    CHECK_NEAR(out.y, 0.8f, 1e-5f);
}

TEST(edge_segment_stops_outward_motion_past_the_plane) {
    // Already inside the 0.2 band: the outward part of the displacement is removed.
    std::vector<Segment> segs{Segment{{{-5, 5, 0}, {5, 5, 0}}, {0, 1, 0}}};
    Vec3 out;
    CHECK(scimitar::ac2::DisplacementCullingBySegment(out, {0, 4.9f, 0}, {0, 0.5f, 0}, 0.2f, segs));
    CHECK_NEAR(out.x, 0.0f, 1e-6f);
    CHECK_NEAR(out.y, 0.0f, 1e-6f);
}

TEST(gap_list_finds_a_roof_edge_and_its_drop) {
    BoxWorld world = Roof(3);
    scimitar::ac2::Human human;
    human.SetPortWorld(&world);
    std::vector<Gap> gaps;
    // 0.5 m from the north edge: only that edge is within 0.75 m.
    CHECK(human.GetGapList(gaps, {0, 4.5f, 3}, 0.75f, 0.45f, true));
    CHECK(gaps.size() == 1);
    CHECK_NEAR(gaps[0].m_EdgePosition.y, 5.0f, 1e-5f);
    CHECK_NEAR(gaps[0].m_EdgeNormal.y, 1.0f, 1e-5f);  // out over the drop
    CHECK_NEAR(gaps[0].m_Depth, 3.0f, 1e-5f);
    CHECK_NEAR(gaps[0].m_SquareDistance, 0.25f, 1e-5f);  // on the roof's side: positive
    CHECK(!gaps[0].m_NoEdge);
    // Under 0.45 m the drop is no gap.
    BoxWorld step = Roof(0.4f);
    human.SetPortWorld(&step);
    CHECK(!human.GetGapList(gaps, {0, 4.5f, 0.4f}, 0.75f, 0.45f, true));
}

TEST(low_profile_walk_stops_at_a_deep_edge) {
    BoxWorld world = Roof(3);
    Sim sim(world, {.position = {0, 3, 3}});
    sim.run(8.0f, [](float) { return Toward({0, 1, 0}); });
    CHECK(!SawInAir(sim));
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ground);
    // Clamped at the edge distance (0.2) or braked before it.
    CHECK(sim.human().m_Position.y <= 4.8f + 1e-3f);
    CHECK(sim.human().m_Position.y > 4.0f);
    CHECK_NEAR(sim.human().m_Position.z, 3.0f, 1e-6f);
}

TEST(low_profile_walk_slides_along_a_deep_edge_beyond_70_degrees) {
    BoxWorld world = Roof(3);
    Sim sim(world, {.position = {-4, 4.2f, 3}});
    // 75 degrees off the edge's normal: neither the break nor GapForStopWalking (both need < 70 degrees); the clamp
    // slides the walk along the edge.
    const float a = 1.3089969f;
    sim.run(4.0f, [a](float) { return Toward({std::sin(a), std::cos(a), 0}); });
    CHECK(!SawInAir(sim));
    CHECK(sim.human().m_Position.y <= 4.8f + 1e-3f);
    CHECK(sim.human().m_Position.x > -3.0f);
}

TEST(low_profile_walk_into_a_deep_edge_within_70_degrees_stops) {
    // Within 0.4 m (square distance < 0.16) of a drop over 2 m, facing and pushing within 70 degrees of the edge's
    // normal: break hard and GapForStopWalking's speed ratio 0 (PC 0xDF96F2..0xDF9842).
    BoxWorld world = Roof(3);
    Sim sim(world, {.position = {-3, 4.2f, 3}});
    sim.run(4.0f, [](float) { return Toward({std::sin(1.0472f), std::cos(1.0472f), 0}); });
    CHECK(!SawInAir(sim));
    CHECK(sim.task().m_GapForStopWalking.m_IsSet);
    CHECK(sim.human().GetContextBundle().GetGround().GetMoveState() != scimitar::ac2::HumanGround::State::MoveCycle);
}

TEST(low_profile_walks_off_a_drop_of_2m_or_less) {
    // The controller clamps a low-profile walk only above 2 m (PC 0xDF98B7).
    BoxWorld world = Roof(1.5f);
    Sim sim(world, {.position = {0, 3, 1.5f}});
    sim.run(8.0f, [](float) { return Toward({0, 1, 0}); });
    CHECK(SawInAir(sim));
}

TEST(high_profile_runs_off_a_drop_of_5m_or_less) {
    BoxWorld world = Roof(4);
    Sim sim(world, {.position = {0, 0, 4}});
    sim.run(4.0f, [](float) { return Toward({0, 1, 0}, kHighProfile); });
    CHECK(SawInAir(sim));
}

TEST(high_profile_head_on_at_a_deep_edge_is_not_clamped) {
    // Above 5 m the clamp is on, but edges whose normal is within 60 degrees of the stick are filtered out
    // (FilterSegmentListWithNormalAngle). LIVE: RE/20 row 12.
    BoxWorld world = Roof(6);
    Sim sim(world, {.position = {0, 0, 6}});
    sim.run(4.0f, [](float) { return Toward({0, 1, 0}, kHighProfile); });
    CHECK(SawInAir(sim));
}

TEST(high_profile_grazing_a_deep_edge_is_clamped) {
    BoxWorld world = Roof(6);
    Sim sim(world, {.position = {-4, 3.5f, 6}});
    // 70 degrees off the edge's normal: the edge stays in the list and the run slides along it.
    sim.run(2.5f, [](float) { return Toward({std::sin(1.2217305f), std::cos(1.2217305f), 0}, kHighProfile); });
    CHECK(!SawInAir(sim));
    CHECK(sim.human().m_Position.y <= 4.8f + 1e-3f);
}
