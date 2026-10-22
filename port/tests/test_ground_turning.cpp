// HumanGround: movement intention, the slow-down curve and turning (RE/11_ground.md 5-8).

#include <cmath>

#include "check.h"
#include "drive.h"
#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/response_curve.h"

using scimitar::ResponseCurve;
using scimitar::Vec3;
using scimitar::ac2::Human;
using scimitar::ac2::HumanGround;
using scimitar::ac2::HumanGroundData;
using State = HumanGround::State;

namespace {
constexpr float kHalfPi = 1.5707963f;
}

TEST(response_curve_edges) {
    ResponseCurve c;
    CHECK_NEAR(c.GetOutput(0.5f), 0.0f, 1e-6f);  // no keys
    c.AddKey(0, 2);
    CHECK_NEAR(c.GetOutput(0.0f), 0.0f, 1e-6f);  // one key
    c.AddKey(1, 4);
    CHECK_NEAR(c.GetOutput(0.5f), 3.0f, 1e-6f);
    CHECK_NEAR(c.GetOutput(-0.1f), 0.0f, 1e-6f);  // below the first key
    CHECK_NEAR(c.GetOutput(1.1f), 0.0f, 1e-6f);   // above the last
}

TEST(ground_slow_down_curve_from_local_init) {
    Human human;
    scimitar::ac2::AssassinTask task;
    const ResponseCurve& c = human.GetContextBundle().GetGround().GetSlowDownCurve();
    CHECK(c.GetKeyCount() == 5);
    CHECK_NEAR(c.GetOutput(0.0f), 1.0f, 1e-6f);
    CHECK_NEAR(c.GetOutput(0.2f), 1.0f, 1e-6f);
    CHECK_NEAR(c.GetOutput(0.3665f), 0.65f, 1e-3f);
    CHECK_NEAR(c.GetOutput(0.4f), 0.3f, 1e-5f);
    CHECK_NEAR(c.GetOutput(0.533f), 0.25f, 1e-3f);
    CHECK_NEAR(c.GetOutput(1.0f), 1.0f, 1e-5f);
}

TEST(ground_movement_division_table) {
    HumanGroundData d;  // LocalInit's tables: {0, 0.9, 1} for both
    for (bool hp : {false, true}) {
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 0.0f) == 0);
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 0.01f) == 1);
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 0.9f) == 1);
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 0.95f) == 2);
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 1.0f) == 2);
        CHECK(d.GetMovementDivisionFromSpeedRatio(hp, 1.5f) == 0);  // past every threshold
    }
}

TEST(rotate_vector_constant_speed) {
    // +Y to +X is a quarter turn clockwise; at 1 rad/s for 0.1 s the yaw goes to -0.1.
    Vec3 r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, {1, 0, 0}, 1, 1, 0, 0, 0, 0.1f);
    CHECK_NEAR(scimitar::yawFromDirection(r), -0.1f, 1e-5f);
}

TEST(rotate_vector_speed_blend) {
    // 0.6 rad left between min angle 0.2 and max angle 1.0: speed = 1 + (0.4 / 0.8) * (3 - 1) = 2 rad/s.
    Vec3 to = scimitar::forwardFromYaw(0.6f);
    Vec3 r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, to, 1, 3, 0.2f, 1.0f, 0, 0.1f);
    CHECK_NEAR(scimitar::yawFromDirection(r), 0.2f, 1e-5f);
    // Below the min angle: min speed. Above the max angle: max speed.
    r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, scimitar::forwardFromYaw(0.15f), 1, 3, 0.2f,
                                                            1.0f, 0, 0.1f);
    CHECK_NEAR(scimitar::yawFromDirection(r), 0.1f, 1e-5f);
    r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, scimitar::forwardFromYaw(1.5f), 1, 3, 0.2f,
                                                            1.0f, 0, 0.1f);
    CHECK_NEAR(scimitar::yawFromDirection(r), 0.3f, 1e-5f);
}

TEST(rotate_vector_snaps_and_forced_side) {
    Vec3 to = scimitar::forwardFromYaw(0.05f);
    Vec3 r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, to, 1, 1, 0, 0, 0, 0.1f);
    CHECK_NEAR(r.x, to.x, 1e-6f);  // a 0.1 rad step would pass 0.05: snapped to the target
    CHECK_NEAR(r.y, to.y, 1e-6f);
    // Forced counter-clockwise: +Y to +X goes the long way round (yaw increases).
    r = scimitar::ac2::RotateVectorToVectorWithAngularSpeed({0, 1, 0}, {1, 0, 0}, 1, 1, 0, 0, 1, 0.1f);
    CHECK_NEAR(scimitar::yawFromDirection(r), 0.1f, 1e-5f);
}

TEST(ground_turns_towards_move_direction) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput in;
    in.moveDir = scimitar::forwardFromYaw(-1.0472f);  // 60 degrees to the right
    in.moveMag = 1;

    PlayerStep(task, human, in, world, dt);
    CHECK(ground.GetMovementDivision() == 2);
    // Gear::GetAngle(move, sight, K) (UpdateTurnAngle PC 0x5FE6A0): positive with the move direction to the right.
    CHECK_NEAR(ground.GetTurnAngle(), 1.0472f, 1e-4f);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // under TurnOnMovementMinAngle (pi/2): no TurnStart
    CHECK_NEAR(human.m_Yaw, 0.0f, 1e-6f);               // the gait level was 0 during this frame's UpdateMoveMain

    for (int i = 0; i < 30; ++i) PlayerStep(task, human, in, world, dt);
    CHECK_NEAR(human.m_Yaw, -1.0472f, 1e-4f);  // UpdateOrientation turned it within the second
    CHECK(human.m_Position.x > 0.0f);
}

TEST(ground_turn_start_from_wait) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput back;
    back.moveDir = scimitar::forwardFromYaw(2.6f);  // about 150 degrees to the left
    back.moveMag = 1;

    PlayerStep(task, human, back, world, dt);
    CHECK(ground.GetMoveState() == State::TurnStart);  // Wait tests TurnStart before MoveCycle
    CHECK(ground.GetGaitLevel() == 0);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::TurnStart; ++i) PlayerStep(task, human, back, world, dt);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // the turn's action finished with movement intended
    CHECK(std::fabs(human.m_Yaw - 2.6f) < 0.2f);        // PORT: the stand-in turn spreads the angle over the action
    // ExecTurnStartToMoveCycleMain (PC 0x615120): a walk starts at 0.2 with TransitionToMotionType 3, which
    // EnterMoveCycle copies into the speed restriction (live 2026-10-21: 0.2 and restriction 3).
    CHECK_NEAR(ground.GetSpeedRatio(), 0.2f, 1e-5f);
}

TEST(ground_turn_start_released_goes_to_wait_at_ratio_0) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput back;
    back.moveDir = scimitar::forwardFromYaw(2.6f);
    back.moveMag = 1;
    PlayerStep(task, human, back, world, dt);
    CHECK(ground.GetMoveState() == State::TurnStart);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::TurnStart; ++i) PlayerStep(task, human, {}, world, dt);
    CHECK(ground.GetMoveState() == State::Wait);
    // ExecTurnStartToWaitMain (PC 0x614F40) zeroes the speed ratio; live 2026-10-21 (RE/20 row 8).
    CHECK_NEAR(ground.GetSpeedRatio(), 0.0f, 1e-6f);
}

TEST(ground_slowing_from_a_run_follows_the_curve) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput run;
    run.moveDir = {0, 1, 0};
    run.moveMag = 1;
    run.buttons = port::kHighProfile;
    for (int i = 0; i < 40; ++i) PlayerStep(task, human, run, world, dt);
    CHECK_NEAR(ground.GetSpeedRatio(), 0.8f, 1e-5f);

    // High profile released, still walking: the target drops to 0.2 and the ratio falls at dt x curve(ratio). The
    // curve is low (0.2..0.3) between 0.4 and 0.666, so it takes over a second to get under 0.4.
    port::MoveInput walk = run;
    walk.buttons = 0;
    int frames = 0;
    while (ground.GetSpeedRatio() > 0.4f && frames < 300) {
        PlayerStep(task, human, walk, world, dt);
        ++frames;
    }
    CHECK(ground.GetMoveState() == State::MoveCycle);
    CHECK(frames > 30);
    CHECK(frames < 90);
}

TEST(ground_move_stop_and_run_turn) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput run;
    run.moveDir = {0, 1, 0};
    run.moveMag = 1;
    run.buttons = port::kHighProfile;
    for (int i = 0; i < 40; ++i) PlayerStep(task, human, run, world, dt);

    PlayerStep(task, human, {}, world, dt);  // released while running: BreakHard -> MoveStop
    CHECK(ground.GetMoveState() == State::MoveStop);
    const float y0 = human.m_Position.y;
    PlayerStep(task, human, {}, world, dt);
    const float step1 = human.m_Position.y - y0;
    PlayerStep(task, human, {}, world, dt);
    const float step2 = human.m_Position.y - y0 - step1;
    CHECK(step1 > 0.0f);
    CHECK(step2 < step1);  // PORT: the stand-in stop slows down over the action

    // The stick pushed backwards: it enters a band more than 120 degrees from the sight, so the controller forces the
    // turn-around (RE/21_input.md), and it breaks hard (more than 135 degrees). Has180TurnIntention -> RunTurn when
    // the stop's action is about to finish.
    port::MoveInput back = run;
    back.moveDir = {0, -1, 0};
    back.buttons = 0;
    PlayerStep(task, human, back, world, dt);
    CHECK(task.m_ForceTurnAround);
    CHECK(ground.GetForceTurnAround());
    CHECK(human.GetGroundData().m_BreakHard);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::MoveStop; ++i) PlayerStep(task, human, back, world, dt);
    CHECK(ground.GetMoveState() == State::RunTurn);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::RunTurn; ++i) PlayerStep(task, human, {}, world, dt);
    CHECK(ground.GetMoveState() == State::Wait);  // division 0 and the run turn's action finished
}

namespace {
// Turns 150 degrees from Wait with `buttons` held, then measures how long the speed ratio stays at its value on
// entering MoveCycle, and where it ends up.
struct EntryHold {
    float entryRatio = -1, holdSeconds = 0, finalRatio = 0;
};
EntryHold MeasureMoveCycleEntryHold(std::uint32_t buttons) {
    port::BoxWorld world;
    Human human;
    scimitar::ac2::AssassinTask task;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 60.0f;
    port::MoveInput in;
    in.moveDir = scimitar::forwardFromYaw(2.6f);
    in.moveMag = 1;
    in.buttons = buttons;
    EntryHold r;
    for (int i = 0; i < 120 && ground.GetMoveState() != State::MoveCycle; ++i) PlayerStep(task, human, in, world, dt);
    r.entryRatio = ground.GetSpeedRatio();
    int held = 0;
    for (int i = 0; i < 300; ++i) {
        PlayerStep(task, human, in, world, dt);
        if (std::fabs(ground.GetSpeedRatio() - r.entryRatio) < 1e-4f) ++held; else break;
    }
    r.holdSeconds = held * dt;
    for (int i = 0; i < 120; ++i) PlayerStep(task, human, in, world, dt);
    r.finalRatio = ground.GetSpeedRatio();
    return r;
}
}  // namespace

TEST(ground_move_cycle_entry_holds_the_ratio_while_walking) {
    // Live session 1 (RE/20 row 8): TurnStart -> MoveCycle with Sprint (legs) held: 0.2 for 0.68 s, then up to 0.4.
    // The restriction (mode 3 from ExecTurnStartToMoveCycleMain) caps the target at 0.2 without high profile.
    const EntryHold r = MeasureMoveCycleEntryHold(port::kLegs);
    CHECK_NEAR(r.entryRatio, 0.2f, 1e-5f);
    CHECK(r.holdSeconds > 0.6f && r.holdSeconds < 0.72f);
    CHECK_NEAR(r.finalRatio, 0.4f, 1e-5f);
}

TEST(ground_move_cycle_entry_hold_is_shorter_in_high_profile) {
    // Live session 1: with high profile, 0.6 for 0.50 s, then up; the action plays at 2 - ratio (+0x998 bit 0).
    const EntryHold r = MeasureMoveCycleEntryHold(port::kHighProfile);
    CHECK_NEAR(r.entryRatio, 0.6f, 1e-5f);
    CHECK(r.holdSeconds > 0.4f && r.holdSeconds < 0.55f);
    CHECK_NEAR(r.finalRatio, 0.8f, 1e-5f);
}
