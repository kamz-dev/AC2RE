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

TEST(rotate_vector_turns_right_handed) {
    // +y turned -50 degrees about y x K (= +x) points 50 degrees below the horizon: the edge probe's ray.
    const Vec3 r = scimitar::RotateVector({0, 2, 0}, scimitar::cross({0, 1, 0}, {0, 0, 1}), -0.87266463f);
    CHECK_NEAR(r.y, std::cos(0.87266463f), 1e-5f);
    CHECK_NEAR(r.z, -std::sin(0.87266463f), 1e-5f);
    // A null axis leaves the (normalised) vector.
    const Vec3 same = scimitar::RotateVector({0, 0, -3}, {}, 1.0f);
    CHECK_NEAR(same.z, -1.0f, 1e-6f);
}

// GetDepthInFrontOfEdge on the 6 m roof's north edge (y = 5), as GetGapList calls it from 0.5 m back.
float RoofEdgeDepth(const BoxWorld& world) {
    scimitar::ac2::Human human;
    human.SetPortWorld(&world);
    return human.GetDepthInFrontOfEdge({0, 4.5f, 6}, {0, 5, 6}, {0, 1, 0}, 7.0f, 0.02f, 0.0f);
}

TEST(edge_depth_is_the_drop_to_the_floor) {
    CHECK_NEAR(RoofEdgeDepth(Roof(6)), 6.0f, 1e-4f);
    // Deeper than maxDepth: maxDepth.
    BoxWorld high(false);
    high.add({{-5, -5, 0}, {5, 5, 6}});
    CHECK_NEAR(RoofEdgeDepth(high), 7.0f, 1e-6f);
}

TEST(edge_depth_an_obstacle_at_the_edge_is_no_drop) {
    // A taller wall 0.01 m past the edge: the 50-degree probe hits it just above the edge's height.
    BoxWorld world = Roof(6);
    world.add({{-5, 5.01f, 0}, {5, 7, 6.5f}});
    // It starts 0.07 * tan(50) above the edge and drops 0.06 * tan(50) before the wall.
    CHECK_NEAR(RoofEdgeDepth(world), -0.01f * std::tan(0.87266463f), 1e-4f);
}

TEST(edge_depth_a_narrow_gap_lands_on_the_far_wall) {
    // A building 0.4 m across: the 45-degree ray from 0.02 past the edge hits its wall 0.38 m down, with no slope
    // test on the first ray.
    BoxWorld world = Roof(6);
    world.add({{-5, 5.4f, 0}, {5, 7, 8}});
    CHECK_NEAR(RoofEdgeDepth(world), 0.38f, 1e-4f);
}

TEST(edge_depth_adds_the_deepest_gap_where_it_lands) {
    // A 3 m ledge from y = 5 to 6.1: the 10-degree ray lands 0.14 m from its edge, inside the 0.2 m search, so the
    // ledge's own 3 m drop is added.
    BoxWorld world = Roof(6);
    world.add({{-5, 5, 0}, {5, 6.1f, 3}});
    CHECK_NEAR(RoofEdgeDepth(world), 6.0f, 1e-4f);
    // A wider ledge: nothing within the search, the drop to the ledge.
    BoxWorld wide = Roof(6);
    wide.add({{-5, 5, 0}, {5, 6.5f, 3}});
    CHECK_NEAR(RoofEdgeDepth(wide), 3.0f, 1e-4f);
}

TEST(edge_depth_slides_off_a_steep_contact) {
    // A wall 1 m out: the 10-degree ray hits its face (90 degrees), slides 45 degrees back out from it, then casts on
    // to the floor.
    BoxWorld world = Roof(6);
    world.add({{-5, 6, 0}, {5, 7, 8}});
    CHECK_NEAR(RoofEdgeDepth(world), 6.0f, 1e-4f);
}

TEST(edge_depth_with_a_null_normal_is_the_drop_below) {
    BoxWorld world;
    scimitar::ac2::Human human;
    human.SetPortWorld(&world);
    CHECK_NEAR(human.GetDepthInFrontOfEdge({0, 0, 3}, {0, 0, 3}, {}, 7.0f, 0.02f, 0.0f), 3.0f, 1e-5f);
    // Standing on the ground: the first ray lands at once.
    CHECK_NEAR(human.GetDepthInFrontOfEdge({0, 0, 0}, {0, 0, 0}, {}, 7.0f, 0.02f, 0.0f), 0.0f, 1e-6f);
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

namespace {
// The game ran at ~290 fps live (RE/20 row 2); at 30 Hz a run steps 0.18 m a frame, past the 0.15 m Unbalance
// window. The edge-reaction tests run at 120 Hz.
constexpr float kFineDt = 1.0f / 120.0f;

using GroundState = scimitar::ac2::HumanGround::State;
}  // namespace

TEST(high_profile_run_at_a_deep_edge_unbalances_into_edge_stop) {
    // Above 5 m the clamp is on, but edges whose normal is within 60 degrees of the stick are filtered out
    // (FilterSegmentListWithNormalAngle), so the run reaches the edge; within 0.15 m it unbalances (EdgeStop). Live
    // (RE/11_ground.md 17): EdgeStop at square distance 0.0224, then the pull-down to Ledge (test_ledge.cpp).
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    bool sawEdgeStop = false;
    float edgeStopY = 0;
    sim.run(3.0f, [&](float) {
        const auto& ground = sim.human().GetContextBundle().GetGround();
        if (!sawEdgeStop && ground.IsInEdgeStop()) {
            sawEdgeStop = true;
            edgeStopY = sim.human().m_Position.y;
        }
        return Toward({0, 1, 0}, kHighProfile);
    });
    CHECK(sawEdgeStop);
    CHECK(edgeStopY > 5.0f - 0.15f - 1e-3f);
    CHECK(edgeStopY <= 5.0f);
    CHECK(!SawInAir(sim));
}

TEST(high_profile_run_off_a_mid_drop_falls_off_the_edge) {
    // 2 to 5 m: no clamp, no Unbalance (needs over 5 m); FallOffEdge once 0.1 m past the edge (square distance
    // < -0.01). Live: InAir at square distance -0.0115 on a 2.3 m drop.
    BoxWorld world = Roof(3);
    Sim sim(world, {.position = {0, 2, 3}}, kFineDt);
    float firstAirY = 0;
    sim.run(3.0f, [&](float) {
        if (firstAirY == 0 && sim.human().GetCurrentContextID() == HumanContextId::InAir)
            firstAirY = sim.human().m_Position.y;
        return Toward({0, 1, 0}, kHighProfile);
    });
    CHECK(SawInAir(sim));
    CHECK(firstAirY > 5.1f);
    CHECK(firstAirY < 5.38f);  // before the port's own support radius (0.38) would let go
}

TEST(low_profile_walk_at_a_deep_edge_looks_down) {
    // GapForStopWalking stops the walk 0.4 m short; 0.25 s later the controller enters LookDown (Wait's child).
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    sim.run(6.0f, [](float) { return Toward({0, 1, 0}); });
    const auto& ground = sim.human().GetContextBundle().GetGround();
    CHECK(ground.GetMoveState() == GroundState::Wait);
    CHECK(ground.GetWaitState() == GroundState::LookDown);
    CHECK(sim.human().m_Position.y > 4.5f);
    CHECK(sim.human().m_Position.y <= 4.8f);
}

TEST(high_profile_grazing_a_deep_edge_is_clamped) {
    BoxWorld world = Roof(6);
    Sim sim(world, {.position = {-4, 3.5f, 6}});
    // 70 degrees off the edge's normal: the edge stays in the list and the run slides along it.
    sim.run(2.5f, [](float) { return Toward({std::sin(1.2217305f), std::cos(1.2217305f), 0}, kHighProfile); });
    CHECK(!SawInAir(sim));
    CHECK(sim.human().m_Position.y <= 4.8f + 1e-3f);
}

TEST(released_stick_in_edge_stop_goes_back_to_move) {
    // Live (RE/20 row 13, session 6): the stick released 0.07 s into EdgeStop; no pull-down, and EdgeStop went back to
    // Move (Wait) 0.94 s after it began.
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    float edgeStopStart = -1;
    float waitAgain = -1;
    sim.run(4.0f, [&](float t) {
        const auto& ground = sim.human().GetContextBundle().GetGround();
        if (edgeStopStart < 0 && ground.IsInEdgeStop()) edgeStopStart = t;
        if (edgeStopStart >= 0 && waitAgain < 0 && ground.GetMoveState() == GroundState::Wait) waitAgain = t;
        if (edgeStopStart >= 0) return MoveInput{};
        return Toward({0, 1, 0}, kHighProfile);
    });
    CHECK(edgeStopStart >= 0);
    CHECK(waitAgain > edgeStopStart);
    CHECK_NEAR(waitAgain - edgeStopStart, 0.94f, 0.02f);
    CHECK(!SawInAir(sim));
    // It stepped back about 0.5 m (live: from 0.13 m to 0.63 m from the edge) and doesn't wobble again.
    CHECK(sim.human().m_Position.y < 5.0f - 0.55f);
    CHECK(!sim.human().GetContextBundle().GetGround().IsInEdgeStop());
}

TEST(grasp_at_the_look_down_pulls_down_to_ledge) {
    // Live (RE/20 row 13, session 6): Shift (the empty hand, pad button 3) at the look-down goes Ground -> Ledge
    // directly (ButtonJustPressedWithDelay(3) -> DoPullDownOnEdge, PC 0xDFA6CA); the pull-down itself is in
    // test_ledge.cpp.
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    sim.run(6.0f, [](float) { return Toward({0, 1, 0}); });
    const auto& ground = sim.human().GetContextBundle().GetGround();
    CHECK(ground.GetWaitState() == GroundState::LookDown);
    MoveInput press = Toward({0, 1, 0});
    press.buttons = kHand;
    sim.step(press);
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    CHECK(!ground.IsInEdgeStop());  // the direct path, not the wobble
    CHECK(sim.human().GetPortRefusedContextID() == HumanContextId::None);
}
