// HumanGround: movement intention, the slow-down curve and turning (RE/11_ground.md 5-8).

#include <cmath>

#include "check.h"
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
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput in;
    in.moveDir = scimitar::forwardFromYaw(-1.0472f);  // 60 degrees to the right
    in.moveMag = 1;

    human.Update(in, world, dt);
    CHECK(ground.GetMovementDivision() == 2);
    CHECK_NEAR(ground.GetTurnAngle(), -1.0472f, 1e-4f);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // under TurnOnMovementMinAngle (pi/2): no TurnStart
    CHECK_NEAR(human.m_Yaw, 0.0f, 1e-6f);               // the gait level was 0 during this frame's UpdateMoveMain

    for (int i = 0; i < 30; ++i) human.Update(in, world, dt);
    CHECK_NEAR(human.m_Yaw, -1.0472f, 1e-4f);  // UpdateOrientation turned it within the second
    CHECK(human.m_Position.x > 0.0f);
}

TEST(ground_turn_start_from_wait) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput back;
    back.moveDir = scimitar::forwardFromYaw(2.6f);  // about 150 degrees to the left
    back.moveMag = 1;

    human.Update(back, world, dt);
    CHECK(ground.GetMoveState() == State::TurnStart);  // Wait tests TurnStart before MoveCycle
    CHECK(ground.GetGaitLevel() == 0);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::TurnStart; ++i) human.Update(back, world, dt);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // the turn's action finished with movement intended
    CHECK(std::fabs(human.m_Yaw - 2.6f) < 0.2f);        // PORT: the stand-in turn spreads the angle over the action
}

TEST(ground_slowing_from_a_run_follows_the_curve) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput run;
    run.moveDir = {0, 1, 0};
    run.moveMag = 1;
    run.buttons = port::kHighProfile;
    for (int i = 0; i < 40; ++i) human.Update(run, world, dt);
    CHECK_NEAR(ground.GetSpeedRatio(), 0.8f, 1e-5f);

    // High profile released, still walking: the target drops to 0.2 and the ratio falls at dt x curve(ratio). The
    // curve is low (0.2..0.3) between 0.4 and 0.666, so it takes over a second to get under 0.4.
    port::MoveInput walk = run;
    walk.buttons = 0;
    int frames = 0;
    while (ground.GetSpeedRatio() > 0.4f && frames < 300) {
        human.Update(walk, world, dt);
        ++frames;
    }
    CHECK(ground.GetMoveState() == State::MoveCycle);
    CHECK(frames > 30);
    CHECK(frames < 90);
}

TEST(ground_move_stop_and_run_turn) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    const float dt = 1.0f / 30.0f;
    port::MoveInput run;
    run.moveDir = {0, 1, 0};
    run.moveMag = 1;
    run.buttons = port::kHighProfile;
    for (int i = 0; i < 40; ++i) human.Update(run, world, dt);

    human.Update({}, world, dt);  // released while running: BreakHard -> MoveStop
    CHECK(ground.GetMoveState() == State::MoveStop);
    const float y0 = human.m_Position.y;
    human.Update({}, world, dt);
    const float step1 = human.m_Position.y - y0;
    human.Update({}, world, dt);
    const float step2 = human.m_Position.y - y0 - step1;
    CHECK(step1 > 0.0f);
    CHECK(step2 < step1);  // PORT: the stand-in stop slows down over the action

    ground.SetForceTurnAround(true);  // Has180TurnIntention -> RunTurn when the stop's action is about to finish
    for (int i = 0; i < 60 && ground.GetMoveState() == State::MoveStop; ++i) human.Update({}, world, dt);
    CHECK(ground.GetMoveState() == State::RunTurn);
    ground.SetForceTurnAround(false);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::RunTurn; ++i) human.Update({}, world, dt);
    CHECK(ground.GetMoveState() == State::Wait);  // division 0 and the run turn's action finished
}
