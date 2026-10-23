// HumanLedge (RE/13_ledge_climb.md): the pull-down from the ground, the hang, the release and the pull-up. Times and
// positions are the live ones the port's stand-ins reproduce (RE/20_live_checks.md row 15).

#include <cmath>

#include "check.h"
#include "port/sim.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ac2/human_ledge.h"

using namespace port;
using scimitar::Vec3;
using scimitar::ac2::HumanLedge;
using scimitar::ac2::HumanLedgeData;

namespace {
constexpr float kFineDt = 1.0f / 120.0f;
using LedgeState = HumanLedge::State;

// A 10 x 10 m roof `height` above the floor; its north edge is y = 5, a wall straight down.
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

MoveInput Press(unsigned buttons) {
    MoveInput in;
    in.buttons = buttons;
    return in;
}

const HumanLedge& Ledge(Sim& sim) { return sim.human().GetContextBundle().GetLedge(); }

// Walks north to the 8 m roof's edge (low profile) and waits for the look-down, as live row 13.
Sim& WalkToLookDown(Sim& sim) {
    sim.run(6.0f, [](float) { return Toward({0, 1, 0}); });
    CHECK(sim.human().GetContextBundle().GetGround().GetWaitState() == scimitar::ac2::HumanGround::State::LookDown);
    return sim;
}

// Runs `sim` until the Ledge state (and Movement child) changes, up to `limit` seconds; returns the time it took.
float UntilLedgeState(Sim& sim, LedgeState state, const MoveInput& in = {}, float limit = 3.0f) {
    const float start = sim.time();
    while (sim.time() - start < limit) {
        if (sim.human().GetCurrentContextID() == HumanContextId::Ledge && Ledge(sim).GetState() == state) break;
        sim.step(in);
    }
    return sim.time() - start;
}
}  // namespace

TEST(grasp_at_the_look_down_pulls_down_into_a_wall_hang) {
    // Live (row 15): PullDownType 1 from the standing grasp; Orientation 0.53 s, Descent 0.13 s, Reception 0.13 s; the
    // hang 0.539 m out from the edge and 1.082 m down, facing the wall.
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    WalkToLookDown(sim);
    sim.step(Press(kHand));
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    const HumanLedgeData& data = sim.human().GetLedgeData();
    CHECK(Ledge(sim).GetState() == LedgeState::PullDown);
    CHECK(data.m_PullDownType == HumanLedgeData::PullDownType_Ground_Soft_Wait);
    CHECK(data.m_SubState == HumanLedgeData::SubState_PullDown);
    CHECK(data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Orientation);
    CHECK_NEAR(data.m_PullDownGap.m_EdgePosition.y, 5.0f, 1e-4f);

    const float pullDown = UntilLedgeState(sim, LedgeState::Movement);
    CHECK_NEAR(pullDown, 0.53f + 0.13f + 0.13f, 0.05f);
    CHECK(Ledge(sim).GetMovementState() == LedgeState::Wait);
    CHECK(data.m_LedgeHangType == HumanLedgeData::LedgeHangType_Wall);
    CHECK(data.m_SubState == HumanLedgeData::SubState_Movement);
    CHECK(data.m_PullDownSubState == HumanLedgeData::PullDownSubState_None);
    CHECK_NEAR(sim.human().m_Position.y, 5.0f + 0.539f, 1e-4f);
    CHECK_NEAR(sim.human().m_Position.z, 8.0f - 1.082f, 1e-4f);
    // Facing the wall (south).
    CHECK_NEAR(scimitar::forwardFromYaw(sim.human().m_Yaw).y, -1.0f, 1e-5f);
    // The press that pulled down doesn't also drop: the hang holds.
    sim.run(1.0f, [](float) { return MoveInput{}; });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
}

TEST(pull_down_steps_through_orientation_descent_reception) {
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    WalkToLookDown(sim);
    sim.step(Press(kHand));
    const HumanLedgeData& data = sim.human().GetLedgeData();
    float t = 0;
    while (data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Orientation && t < 2) {
        sim.step({});
        t += kFineDt;
    }
    CHECK_NEAR(t, 0.53f, 0.02f);
    // The Orientation ends 0.53 m out, 0.05 m down (live).
    CHECK_NEAR(sim.human().m_Position.y, 5.53f, 0.02f);
    CHECK(data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Descent);
    t = 0;
    while (data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Descent && t < 2) {
        sim.step({});
        t += kFineDt;
    }
    CHECK_NEAR(t, 0.13f, 0.02f);
    CHECK(data.m_PullDownSubState == HumanLedgeData::PullDownSubState_Reception);
}

TEST(pull_down_over_an_overhang_hangs_free) {
    // An awning-like slab: nothing to brace the feet on 1.2 m under the edge. Live: the free hang sits on the edge
    // line 2.400 m down, after a 0.33 s reception.
    BoxWorld world;
    world.add({{-5, -5, 0}, {5, 3, 8}});    // the building, 2 m back
    world.add({{-5, 3, 7.7f}, {5, 5, 8}});  // a 0.3 m slab to the edge
    Sim sim(world, {.position = {0, 1, 8}}, kFineDt);
    sim.run(6.0f, [](float) { return Toward({0, 1, 0}); });
    sim.step(Press(kHand));
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    UntilLedgeState(sim, LedgeState::Movement);
    CHECK(sim.human().GetLedgeData().m_LedgeHangType == HumanLedgeData::LedgeHangType_Free);
    CHECK_NEAR(sim.human().m_Position.y, 5.0f, 1e-4f);
    CHECK_NEAR(sim.human().m_Position.z, 8.0f - 2.4f, 1e-4f);
}

TEST(empty_hand_at_the_hang_drops_to_in_air) {
    // Live (row 15b): the release goes Wait -> InAir at once, from the hang position.
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    WalkToLookDown(sim);
    sim.step(Press(kHand));
    UntilLedgeState(sim, LedgeState::Movement);
    sim.step({});
    const Vec3 hang = sim.human().m_Position;
    sim.step(Press(kHand));
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::InAir);
    CHECK_NEAR(sim.human().GetInAirData().m_JumpOrigin.z, hang.z, 0.05f);
    sim.run(3.0f, [](float) { return MoveInput{}; });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ground);
    CHECK_NEAR(sim.human().m_Position.z, 0.0f, 1e-5f);
    CHECK(sim.human().m_Position.y > 5.0f);  // in the street, below the hang
}

TEST(stick_up_at_the_hang_pulls_up_onto_the_roof) {
    // Live (row 15): 0.73 s of Pullup from a wall hang, then 0.27 s in NarrowObject, then Ground, standing 0.081 m back
    // from the edge. The stick "up" is the pad's up: with the camera facing north and Ezio facing the wall (south),
    // that is the world's north here (the camera's forward).
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    WalkToLookDown(sim);
    sim.step(Press(kHand));
    UntilLedgeState(sim, LedgeState::Movement);
    sim.run(0.5f, [](float) { return MoveInput{}; });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    const float start = sim.time();
    float pullupAt = -1, groundAt = -1;
    // The stick is let go once the pull-up has started: held, it would walk Ezio back into the edge (EdgeStop, then
    // the pull-down again, as the game's rules have it).
    sim.run(2.0f, [&](float) {
        if (pullupAt < 0 && Ledge(sim).GetState() == LedgeState::Pullup) pullupAt = sim.time();
        if (groundAt < 0 && sim.human().GetCurrentContextID() == HumanContextId::Ground) groundAt = sim.time();
        return pullupAt < 0 ? Toward({0, 1, 0}) : MoveInput{};
    });
    CHECK(pullupAt >= start);
    CHECK(sim.human().GetLedgeData().m_SubState == HumanLedgeData::SubState_Pullup);
    CHECK(groundAt > pullupAt);
    CHECK_NEAR(groundAt - pullupAt, 0.73f + 0.27f, 0.03f);
    // On the roof, 0.081 m back from the edge (then free to walk again).
    CHECK(sim.human().m_Position.z >= 8.0f - 1e-4f);
    CHECK(sim.human().m_Position.y < 5.0f);
}

TEST(the_stick_sideways_at_the_hang_does_not_pull_up) {
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    WalkToLookDown(sim);
    sim.step(Press(kHand));
    UntilLedgeState(sim, LedgeState::Movement);
    sim.run(1.0f, [](float) { return Toward({1, 0, 0}); });
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    CHECK(Ledge(sim).GetState() == LedgeState::Movement);
    CHECK(sim.human().GetLedgeData().m_CurSpeedRatio > 0.9f);
}

TEST(high_profile_run_at_a_deep_edge_pulls_down_from_edge_stop) {
    // Live (RE/11_ground.md 17, row 15): EdgeStop at square distance 0.0224, the pull-down 0.27 s later with
    // PullDownType 0, an Orientation of 0.40 s, and the same wall hang.
    BoxWorld world = Roof(8);
    Sim sim(world, {.position = {0, 2, 8}}, kFineDt);
    float edgeStopAt = -1, ledgeAt = -1;
    // The stick is let go at the hang: held up, it would pull Ezio back up.
    sim.run(3.0f, [&](float) {
        if (edgeStopAt < 0 && sim.human().GetCurrentContextID() == HumanContextId::Ground &&
            sim.human().GetContextBundle().GetGround().IsInEdgeStop())
            edgeStopAt = sim.time();
        if (ledgeAt < 0 && sim.human().GetCurrentContextID() == HumanContextId::Ledge) ledgeAt = sim.time();
        return ledgeAt < 0 ? Toward({0, 1, 0}, kHighProfile) : MoveInput{};
    });
    CHECK(edgeStopAt >= 0);
    CHECK(ledgeAt > edgeStopAt);
    CHECK_NEAR(ledgeAt - edgeStopAt, 0.27f, 0.03f);
    CHECK(sim.human().GetLedgeData().m_PullDownType == HumanLedgeData::PullDownType_Ground_Soft_EdgeStop);
    CHECK(sim.human().GetCurrentContextID() == HumanContextId::Ledge);
    CHECK_NEAR(sim.human().m_Position.z, 8.0f - 1.082f, 1e-4f);
}

TEST(ledge_enters_the_state_its_sub_state_asks_for) {
    // EnterHumanLedge (Mac 0x13477BA): SubState picks the root's initial state.
    scimitar::ac2::Human human;
    HumanLedge& ledge = human.GetContextBundle().GetLedge();
    human.GetLedgeData().m_SubState = HumanLedgeData::SubState_Movement;
    ledge.Enter();
    CHECK(ledge.GetState() == LedgeState::Movement);
    CHECK(ledge.GetMovementState() == LedgeState::Wait);
    human.GetLedgeData().m_SubState = HumanLedgeData::SubState_Entry;
    ledge.Enter();
    CHECK(ledge.GetState() == LedgeState::HangEntry);
    human.GetLedgeData().m_SubState = HumanLedgeData::SubState_Grasp;
    ledge.Enter();
    CHECK(ledge.GetState() == LedgeState::GraspEntry);
    human.GetLedgeData().m_SubState = HumanLedgeData::SubState_ParallelJump;  // rewritten to HandPlacement
    ledge.Enter();
    CHECK(ledge.GetState() == LedgeState::Movement);
    CHECK(human.GetLedgeData().m_SubState == HumanLedgeData::SubState_HandPlacement);
}

