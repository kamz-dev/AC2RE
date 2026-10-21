// HumanGround: entering Move from a MovementState, landings (RE/11_ground.md 11).

#include "check.h"
#include "port/input.h"
#include "port/sim.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

using scimitar::ac2::Human;
using scimitar::ac2::HumanGround;
namespace ms = scimitar::ac2::movement_state;
using State = HumanGround::State;

namespace {
struct Row {
    int movementState;
    State first;
    int running, gait;
    float ratio;
};
}  // namespace

TEST(ground_enter_move_table) {
    const Row rows[] = {
        {ms::WaitLow, State::Wait, 0, 0, 0.0f},
        {ms::WaitHigh, State::Wait, 1, 0, 0.0f},
        {ms::WalkVerySlow, State::MoveCycle, 0, 1, 0.0f},
        {ms::Walk, State::MoveCycle, 0, 2, 0.2f},
        {ms::Jog, State::MoveCycle, 1, 2, 0.6f},
        {ms::Run, State::MoveCycle, 1, 2, 0.6f},
        {ms::Sprint, State::MoveCycle, 1, 2, 1.0f},
        {ms::Landing, State::Landing, 0, 0, 0.0f},
        {ms::LandingHard, State::LandingHard, 0, 0, 0.0f},
        {ms::LandingBalanceFall, State::LandingBalanceFall, 1, 0, 0.0f},
        {ms::Default, State::Wait, 0, 0, 0.0f},
    };
    for (const Row& r : rows) {
        Human human;
        HumanGround& ground = human.GetContextBundle().GetGround();
        human.GetGroundData().m_MovementState = r.movementState;
        ground.Enter();
        CHECK(ground.GetMoveState() == r.first);
        CHECK_NEAR(ground.GetSpeedRatio(), r.ratio, 1e-6f);
        if (r.first == State::MoveCycle) {
            // EnterMoveCycle: gait := division (= EnterMove's gait), running := high profile (= EnterMove's running)
            CHECK(ground.GetGaitLevel() == r.gait);
            CHECK(ground.IsRunning() == (r.running != 0));
            // ...and the transition data is reset
            CHECK(human.GetGroundData().m_MovementState == ms::Default);
        } else {
            CHECK(ground.GetGaitLevel() == r.gait);
            CHECK(ground.IsRunning() == (r.running != 0));
            CHECK(ground.GetMovementDivision() == r.gait);
        }
    }
}

TEST(ground_landing_while_moving_starts_a_walk) {
    port::BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 3}});
    port::Sim sim(world, {.position = {0, 0, 3}});
    const HumanGround& ground = sim.human().GetContextBundle().GetGround();
    auto fwd = [](float, std::uint32_t buttons) {
        port::MoveInput in;
        in.moveDir = {0, 1, 0};
        in.moveMag = 1;
        in.buttons = buttons;
        return in;
    };
    // Run off the edge, then walk (no high profile) through the landing.
    sim.run(2.0f, [&](float t) { return fwd(t, port::kHighProfile); });
    bool sawLanding = false;
    for (int i = 0; i < 90; ++i) {
        sim.step(fwd(0, 0));
        sawLanding |= ground.GetMoveState() == State::Landing;
        if (sawLanding && ground.GetMoveState() == State::MoveCycle) break;
    }
    CHECK(sawLanding);
    CHECK(ground.GetMoveState() == State::MoveCycle);
    // ExecLandingToMoveCycleMain without high profile: a walk at 0.2; turning ramps back in from rotation ratio 0.
    CHECK_NEAR(ground.GetSpeedRatio(), 0.2f, 1e-5f);
    CHECK(sim.human().GetGroundData().m_RotationRatio < 0.1f);
    CHECK_NEAR(sim.human().GetGroundData().m_RotationRatioStep, 0.5f, 1e-6f);
}

TEST(ground_standing_landing_goes_to_wait) {
    port::BoxWorld world;
    port::Sim sim(world, {.position = {0, 0, 2}, .context = scimitar::ac2::HumanContextId::InAir});
    const HumanGround& ground = sim.human().GetContextBundle().GetGround();
    bool sawLanding = false;
    for (int i = 0; i < 90; ++i) {
        sim.step({});
        sawLanding |= ground.GetMoveState() == State::Landing;
    }
    CHECK(sawLanding);
    CHECK(sim.human().GetCurrentContextID() == scimitar::ac2::HumanContextId::Ground);
    CHECK(ground.GetMoveState() == State::Wait);
}

TEST(ground_sprint_landing_keeps_the_sprint) {
    port::BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 3}});
    port::Sim sim(world, {.position = {0, 0, 3}});
    const HumanGround& ground = sim.human().GetContextBundle().GetGround();
    port::MoveInput sprint;
    sprint.moveDir = {0, 1, 0};
    sprint.moveMag = 1;
    sprint.buttons = port::kHighProfile | port::kLegs;
    bool sawLanding = false;
    for (int i = 0; i < 150; ++i) {
        sim.step(sprint);
        sawLanding |= ground.GetMoveState() == State::Landing;
        if (sawLanding && ground.GetMoveState() == State::MoveCycle) break;
    }
    CHECK(sawLanding);
    CHECK_NEAR(ground.GetSpeedRatio(), 1.0f, 1e-5f);  // high profile + sprint: MovementState Sprint, ratio 1
}
