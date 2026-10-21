// AssassinTask::UpdateGroundMovement: the player's stick and buttons -> IHumanGround (RE/21_input.md).

#include <cmath>

#include "check.h"
#include "drive.h"

using scimitar::Vec3;
using scimitar::ac2::AssassinAbilitySet;
using scimitar::ac2::AssassinTask;
using scimitar::ac2::Human;

namespace {
constexpr float kDt = 1.0f / 30.0f;

port::MoveInput Stick(float yaw, float mag, std::uint32_t buttons = 0) {
    port::MoveInput in;
    in.moveDir = scimitar::forwardFromYaw(yaw);
    in.moveMag = mag;
    in.buttons = buttons;
    return in;
}
}  // namespace

TEST(controller_speed_ratio_from_the_stick) {
    // 0 up to the 0.35 dead zone, then (length - 0.35) / 0.65 (PC 0xDF834B).
    Human human;
    AssassinTask task;
    const auto& data = human.GetGroundData();
    task.Update(Stick(0, 0.35f), human, kDt);
    CHECK_NEAR(data.m_DestSpeedRatio, 0.0f, 1e-6f);
    task.Update(Stick(0, 0.675f), human, kDt);
    CHECK_NEAR(data.m_DestSpeedRatio, 0.5f, 1e-6f);
    task.Update(Stick(0, 1.0f), human, kDt);
    CHECK_NEAR(data.m_DestSpeedRatio, 1.0f, 1e-6f);
}

TEST(controller_max_speed_ability) {
    Human human;
    AssassinTask task;
    const auto& data = human.GetGroundData();
    task.m_PortMaxSpeed = AssassinAbilitySet::Jog;  // high profile capped at 0.3
    task.Update(Stick(0, 1, port::kHighProfile), human, kDt);
    CHECK_NEAR(data.m_DestSpeedRatio, 0.3f, 1e-6f);
    CHECK(data.m_DestMvtType == 1);
    CHECK(!data.m_Sprint);  // sprint needs MaxSpeed Sprint
    task.Update(Stick(0, 1, port::kLegs), human, kDt);
    CHECK(!data.m_Sprint);

    task.m_PortMaxSpeed = AssassinAbilitySet::Walk;  // no high-profile movement type
    task.Update(Stick(0, 1, port::kHighProfile), human, kDt);
    CHECK(data.m_DestMvtType == 0);
    CHECK_NEAR(data.m_DestSpeedRatio, 1.0f, 1e-6f);

    task.m_PortMaxSpeed = AssassinAbilitySet::NoMovementAllowed;
    task.Update(Stick(0, 1), human, kDt);
    CHECK_NEAR(data.m_DestSpeedRatio, 0.0f, 1e-6f);
}

TEST(controller_buttons) {
    Human human;
    AssassinTask task;
    const auto& data = human.GetGroundData();
    task.Update(Stick(0, 1, port::kLegs), human, kDt);  // pad button 0: sprint (the fast walk in low profile)
    CHECK(data.m_Sprint);
    CHECK(data.m_DestMvtType == 0);
    CHECK(task.m_InPreciseTargetingMode);
    task.Update(Stick(0, 1, port::kLegs | port::kHighProfile), human, kDt);
    CHECK(data.m_Sprint);
    CHECK(data.m_DestMvtType == 1);
    CHECK(!task.m_InPreciseTargetingMode);
    CHECK(!data.m_Crouch);  // the player's controller never crouches
}

TEST(controller_heading_rotation_speeds) {
    // Live-verified values (RE/20 row 6), now from the controller (PC 0xDF8818).
    port::BoxWorld world;
    Human human;
    AssassinTask task;
    auto& ground = human.GetContextBundle().GetGround();
    PlayerStep(task, human, Stick(0, 1), world, kDt);
    CHECK(ground.GetHeadingMinRotationSpeed() == 1.5f && ground.GetHeadingMaxRotationSpeed() == 4.0f);
    PlayerStep(task, human, Stick(0, 1, port::kLegs), world, kDt);
    CHECK(ground.GetHeadingMinRotationSpeed() == 0.975f && ground.GetHeadingMaxRotationSpeed() == 2.6f);
}

TEST(controller_break_hard) {
    // At rest, and on a reversal of more than 135 degrees from the sight (PC 0xDF8139).
    Human human;  // yaw 0: the sight is +Y
    AssassinTask task;
    const auto& data = human.GetGroundData();
    task.Update({}, human, kDt);
    CHECK(data.m_BreakHard);
    task.Update(Stick(0, 1), human, kDt);
    CHECK(!data.m_BreakHard);
    task.Update(Stick(2.0f, 1), human, kDt);  // about 115 degrees: no
    CHECK(!data.m_BreakHard);
    task.Update(Stick(2.6f, 1), human, kDt);  // about 149 degrees: yes
    CHECK(data.m_BreakHard);
}

TEST(controller_direction_without_stick_is_the_sight) {
    Human human;
    human.m_Yaw = 0.5f;
    AssassinTask task;
    task.Update(Stick(-1.0f, 0), human, kDt);  // a direction with no stick length is ignored
    const Vec3 dir = human.GetGroundData().m_DestHeading;
    const Vec3 sight = scimitar::forwardFromYaw(0.5f);
    CHECK_NEAR(dir.x, sight.x, 1e-6f);
    CHECK_NEAR(dir.y, sight.y, 1e-6f);
}

TEST(controller_turn_slowdown_in_high_profile) {
    // The damped stick 90 degrees from the sight: the factor falls at 10/s to 0.1 (PC 0xDF83F3); without high profile
    // it stays 1.
    Human human;
    AssassinTask task;
    const auto& data = human.GetGroundData();
    task.Update(Stick(1.5707964f, 1), human, kDt);
    CHECK_NEAR(task.m_TurnSlowdownFactor, 1.0f, 1e-6f);
    task.Update(Stick(1.5707964f, 1, port::kHighProfile), human, kDt);
    CHECK_NEAR(task.m_TurnSlowdownFactor, 1.0f - 10.0f * kDt, 1e-5f);
    CHECK_NEAR(data.m_DestSpeedRatio, 1.0f - 10.0f * kDt, 1e-5f);
    for (int i = 0; i < 10; ++i) task.Update(Stick(1.5707964f, 1, port::kHighProfile), human, kDt);
    CHECK_NEAR(task.m_TurnSlowdownFactor, 0.1f, 1e-6f);
    // Back within 45 degrees: rises at 5/s.
    task.Update(Stick(0, 1, port::kHighProfile), human, kDt);
    CHECK_NEAR(task.m_TurnSlowdownFactor, 0.1f + 5.0f * kDt, 1e-5f);
}

TEST(controller_without_all_actions_stops) {
    Human human;
    AssassinTask task;
    task.m_PortAllActions = false;
    task.Update(Stick(1.0f, 1, port::kHighProfile), human, kDt);
    CHECK_NEAR(human.GetGroundData().m_DestSpeedRatio, 0.0f, 1e-6f);
    CHECK_NEAR(human.GetGroundData().m_DestHeading.y, 1.0f, 1e-6f);  // the sight
}
