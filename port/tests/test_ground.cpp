// HumanGround's decoded rules (RE/11_ground.md 5; UpdateMoveCycleMain PC 0x61BB60, Mac 0xFCD20A).

#include "check.h"
#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

using scimitar::ac2::Human;
using scimitar::ac2::HumanGround;
using State = HumanGround::State;

TEST(ground_target_ratio_bases) {
    // stick * 0.2 + {0, 0.2 band-up, 0.6 high profile, 0.8 both}, clamped to 1
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(1, false, false, false, 0), 0.2f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(1, true, false, false, 0), 0.4f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(1, false, true, false, 0), 0.8f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(1, true, true, false, 0), 1.0f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(0.5f, false, true, false, 0), 0.7f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(0, false, false, false, 0), 0.0f, 1e-6f);
}

TEST(ground_target_ratio_ai_override) {
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(1, true, true, true, 0.33f), 0.33f, 1e-6f);
    CHECK_NEAR(HumanGround::ComputeTargetSpeedRatio(0, false, false, true, 1.5f), 1.0f, 1e-6f);
}

TEST(ground_restriction_modes) {
    auto r = [](float t, int mode, bool bandUp = false, bool hp = true, float cur = 0, bool player = true) {
        return HumanGround::RestrictTargetSpeedRatio(t, mode, player, bandUp, hp, cur);
    };
    CHECK_NEAR(r(0.9f, 0), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.1f, 0), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.1f, 1), 0.2f, 1e-6f);
    CHECK_NEAR(r(0.9f, 4), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.9f, 2, false, true, 0.5f), 0.2f, 1e-6f);
    CHECK_NEAR(r(0.9f, 2, false, true, 0.7f), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.5f, 3), 0.5f, 1e-6f);
    CHECK_NEAR(r(0.9f, 3), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.1f, 5), 0.2f, 1e-6f);
    CHECK_NEAR(r(0.9f, 6), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.9f, 6, true), 0.9f, 1e-6f);
    CHECK_NEAR(r(0.9f, 7), 0.6f, 1e-6f);
    CHECK_NEAR(r(0.1f, 7), 0.1f, 1e-6f);
    CHECK_NEAR(r(0.9f, 9), 0.9f, 1e-6f);                       // other modes: unchanged
    CHECK_NEAR(r(0.9f, 9, false, false), 0.2f, 1e-6f);         // no high profile: at most 0.2
    CHECK_NEAR(r(0.05f, 9, false, true, 0, false), 0.2f, 1e-6f);  // non-player: at least 0.2
    CHECK_NEAR(r(0.9f, 9, false, true, 0, false), 0.6f, 1e-6f);   // non-player: at most 0.6
}

TEST(ground_ratio_step) {
    CHECK_NEAR(HumanGround::StepSpeedRatio(0.0f, 0.2f, 0.05f, 1), 0.05f, 1e-6f);
    CHECK_NEAR(HumanGround::StepSpeedRatio(0.18f, 0.2f, 0.05f, 1), 0.2f, 1e-6f);   // capped at the target
    CHECK_NEAR(HumanGround::StepSpeedRatio(0.6f, 0.2f, 0.05f, 2), 0.5f, 1e-6f);    // falling, times the curve
    CHECK_NEAR(HumanGround::StepSpeedRatio(0.22f, 0.2f, 0.05f, 1), 0.2f, 1e-6f);   // not below the target
    CHECK_NEAR(HumanGround::StepSpeedRatio(0.2004f, 0.2f, 0.05f, 1), 0.2004f, 1e-7f);  // within 0.0005: unchanged
}

TEST(ground_gait_bands) {
    struct Row {
        float ratio;
        int gait, running;
    } rows[] = {{0.05f, 1, 0}, {0.15f, 2, 0}, {0.3f, 2, 0}, {0.5f, 1, 1}, {0.7f, 2, 1}, {0.95f, 2, 1}};
    for (const Row& row : rows) {
        int gait = -1, running = -1;
        HumanGround::GaitFromSpeedRatio(row.ratio, gait, running);
        CHECK(gait == row.gait);
        CHECK(running == row.running);
    }
}

TEST(ground_wait_movecycle_wait) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput walk;
    walk.moveDir = {0, 1, 0};
    walk.moveMag = 1;

    human.Update(walk, world, dt);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // Wait -> MoveCycle this frame, ratio not yet updated
    CHECK_NEAR(ground.GetSpeedRatio(), 0.0f, 1e-6f);

    for (int i = 0; i < 30; ++i) human.Update(walk, world, dt);
    CHECK_NEAR(ground.GetSpeedRatio(), 0.2f, 1e-5f);  // walk target, reached at dt per frame
    CHECK(human.m_Position.y > 0.0f);

    port::MoveInput run = walk;
    run.buttons = port::kHighProfile | port::kLegs;  // Sprint + high profile: target 1 (live row 5)
    for (int i = 0; i < 30; ++i) human.Update(run, world, dt);
    CHECK_NEAR(ground.GetSpeedRatio(), 1.0f, 1e-5f);
    CHECK(ground.IsRunning());

    // Released while running: BreakHard (the port's stand-in controller) sends MoveCycle to MoveStop, and MoveStop to
    // Wait once its action is over and the division is 0.
    human.Update({}, world, dt);
    CHECK(ground.GetMoveState() == State::MoveStop);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::MoveStop; ++i) human.Update({}, world, dt);
    CHECK(ground.GetMoveState() == State::Wait);
    // ExecMoveStopToWaitMain (PC 0x61F200) zeroes the speed ratio; live 2026-10-21 (RE/20 row 8).
    CHECK_NEAR(ground.GetSpeedRatio(), 0.0f, 1e-6f);
}
