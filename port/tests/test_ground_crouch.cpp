// HumanGround: crouching, TurnOnSpot and GetTurnAnimation (RE/11_ground.md 13).

#include <cmath>

#include "check.h"
#include "drive.h"
#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

using scimitar::ac2::Human;
using scimitar::ac2::HumanGround;
using State = HumanGround::State;

namespace {
constexpr float kDt = 1.0f / 30.0f;

port::MoveInput Dir(float yaw, float mag) {
    port::MoveInput in;
    in.moveDir = scimitar::forwardFromYaw(yaw);
    in.moveMag = mag;
    return in;
}
}  // namespace

TEST(ground_turn_animation_buckets) {
    const struct {
        float angle;
        int anim;
    } rows[] = {{0.2f, 0}, {-0.2f, 1}, {0.8f, 2}, {-0.8f, 3}, {2.0f, 4}, {-2.0f, 5}, {3.0f, 6}, {-3.0f, 7}};
    for (const auto& r : rows) CHECK(HumanGround::GetTurnAnimation(r.angle) == r.anim);
}

TEST(ground_crouch_wait_move_and_stand) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    ground.SetCrouch(true);

    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);  // Wait -> CrouchWait: crouch, no high profile
    CHECK(ground.GetGaitLevel() == 0);

    AiStep(human, Dir(0, 1), world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchMove);  // movement intended
    CHECK(ground.GetGaitLevel() == 1);
    for (int i = 0; i < 30; ++i) AiStep(human, Dir(0, 1), world, kDt);
    CHECK_NEAR(human.m_Position.y, human.GetPortGroundSpeeds().crouchMove * 31 * kDt, 1e-3f);

    ground.SetCrouch(false);
    AiStep(human, Dir(0, 1), world, kDt);
    CHECK(ground.GetMoveState() == State::MoveCycle);  // crouch released, can stand up

    ground.SetCrouch(true);
    AiStep(human, Dir(0, 1), world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchMove);  // MoveCycle -> CrouchMove: crouch while not running

    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);  // no movement intended
}

TEST(ground_crouch_turn_on_a_180_request) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    ground.SetCrouch(true);
    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);

    // A direction behind, no stick: no movement intended, but a 180 intention (|turn angle| > 2pi/3).
    AiStep(human, Dir(2.8f, 0), world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchTurn);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::CrouchTurn; ++i) AiStep(human, Dir(2.8f, 0), world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);
    CHECK(std::fabs(human.m_Yaw - 2.8f) < 0.15f);  // PORT: the stand-in turn applies the angle over its length
}

TEST(ground_turn_on_spot_needs_turn_mode_1) {
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();

    AiStep(human, Dir(-1.0f, 0), world, kDt);  // a direction with no stick
    CHECK(ground.GetMoveState() == State::Wait);  // turn mode 0: no TurnOnSpot

    ground.SetTurnMode(1);
    AiStep(human, Dir(-1.0f, 0), world, kDt);
    CHECK(ground.GetMoveState() == State::TurnOnSpot);
    for (int i = 0; i < 60 && ground.GetMoveState() == State::TurnOnSpot; ++i) AiStep(human, Dir(-1.0f, 0), world, kDt);
    CHECK(ground.GetMoveState() == State::Wait);
    CHECK(std::fabs(human.m_Yaw + 1.0f) < 0.1f);
}

TEST(ground_do_turn_on_spot_turns_to_the_requested_direction) {
    // Interface command 155 (CanTurnOnSpot / DoTurnOnSpot): from Wait, for any turn mode; the dead zone is the angle
    // from the sight (Mac TestTurnOnSpotDeadZone 0xFB3A6C).
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::Wait);
    const scimitar::Vec3 left = scimitar::forwardFromYaw(1.5f);
    CHECK(ground.CanTurnOnSpot(left, 0.5f));
    CHECK(!ground.CanTurnOnSpot(scimitar::forwardFromYaw(0.2f), 0.5f));  // inside the dead zone

    ground.DoTurnOnSpot(left);
    CHECK(ground.GetMoveState() == State::TurnOnSpot);
    CHECK(ground.IsInTurnOnSpotTurn());
    CHECK(!ground.CanTurnOnSpot(left, 0.0f));  // only offered from Wait
    for (int i = 0; i < 90 && ground.GetMoveState() == State::TurnOnSpot; ++i) AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::Wait);
    CHECK(std::fabs(human.m_Yaw - 1.5f) < 0.1f);
}

TEST(ground_turn_on_spot_cycle_holds_the_step_until_cleared) {
    // SetUseTurnOnSpotCycle only takes while turning on the spot (Mac 0xF979FE). With the flag set the step doesn't
    // return to Wait (TestTurnOnSpotStepToWaitMain); with no channel-1 action to time a next step it holds, as the
    // game does, until the flag is cleared.
    port::BoxWorld world;
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    AiStep(human, {}, world, kDt);
    ground.SetUseTurnOnSpotCycle(true);  // ignored: not turning
    ground.DoTurnOnSpot(scimitar::forwardFromYaw(-1.0f));
    ground.SetUseTurnOnSpotCycle(true);
    for (int i = 0; i < 90; ++i) AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::TurnOnSpot);
    ground.SetUseTurnOnSpotCycle(false);
    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::Wait);
}

TEST(ground_crouch_cannot_stand_up_under_a_low_ceiling) {
    // ActorCanPhysicallyStandUp (PC 0x5F72C0): a 0.38 m capsule from the feet + 1.42 m down to + 0.42 m must be clear.
    port::BoxWorld world;
    world.add({{-1, -1, 1.2f}, {1, 1, 2}});  // a ceiling at 1.2 m over the spot
    Human human;
    HumanGround& ground = human.GetContextBundle().GetGround();
    ground.SetCrouch(true);
    AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);
    ground.SetCrouch(false);
    for (int i = 0; i < 10; ++i) AiStep(human, {}, world, kDt);
    CHECK(ground.GetMoveState() == State::CrouchWait);  // released, but no room to stand

    port::BoxWorld high;
    high.add({{-1, -1, 1.9f}, {1, 1, 2.5f}});  // above the capsule's top (1.42 + 0.38 = 1.8)
    AiStep(human, {}, high, kDt);
    CHECK(ground.GetMoveState() == State::Wait);
}
