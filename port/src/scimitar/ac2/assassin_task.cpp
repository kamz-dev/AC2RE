#include "scimitar/ac2/assassin_task.h"

#include <cmath>

#include "port/input.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

namespace scimitar::ac2 {

namespace {
constexpr float kHalfPi = 1.5707964f;
constexpr float kQuarterPi = 0.78539819f;

// The player's stick bands' upper limits: the thresholds at *(AssassinTask +0x7A8), i.e. *(ControlScheme +0x738).
// Read live 2026-10-21 (RE/20 row 11). PORT: the table's source (ControlScheme::SetupStickThresholds) isn't decoded,
// so the values are constants here.
constexpr float kStickThresholds[] = {0.35f, 0.9f, 1.0f};

// JoyThresholdPressed's test, inlined on PC (0xDF8139): band 0 is length <= t0, band n is t(n-1) < length <= tn.
int PortStickBand(float length) {
    for (int band = 0; band < 3; ++band)
        if (length <= kStickThresholds[band]) return band;
    return 3;
}

Vec3 NormalizeAny(const Vec3& v) { return normalized(v); }

// Gear::GetAngle(a, b, BaseK): the signed angle about +Z. PORT: the sign convention isn't checked; the decoded code
// only uses its size.
float GetAngleAboutK(const Vec3& a, const Vec3& b) {
    float angle = yawFromDirection(a) - yawFromDirection(b);
    while (angle > 3.14159265f) angle -= 6.28318531f;
    while (angle <= -3.14159265f) angle += 6.28318531f;
    return angle;
}
}  // namespace

bool AssassinTask::ButtonPressed(int button) const {
    // ControlScheme::ButtonPressed: not held by another control scheme, then Pad::ButtonPressed (PC 0x10B1710).
    // PORT: the port has pad button 0 only (the port's kLegs: A / Space, live RE/20 row 5).
    return button == 0 && m_PortButton0;
}

int AssassinTask::PortReorientDirectionFromWallCollision(const Vec3&, float, float, const Vec3&, Vec3&) { return 0; }

void AssassinTask::Update(const port::MoveInput& input, Human& human, float dt) {
    m_PortClock += dt;
    const Vec3 sight = forwardFromYaw(human.m_Yaw);

    // UpdateGlobal (Mac 0x106BDE2), the parts the ground update reads.
    // High profile: ButtonPressed(13) (12 with the alternative control setting), flipped by the inverse-profile option
    // (PORT: off); only with the HighProfile ability, and only rewritten while all actions are allowed.
    if (!m_PortHighProfile)
        m_IsHighProfile = false;
    else if (m_PortAllActions)
        m_IsHighProfile = input.held(port::kHighProfile);
    // The camera-relative stick, normalised, and its damped version. PORT: the port's input is already the
    // camera-relative direction. ControlScheme::GetDampedRelativeStick's damping isn't decoded; with the keyboard the
    // damped stick equals the stick on every frame (live, RE/20 row 11), which the port keeps for any input. A
    // gamepad's damping is unmeasured.
    m_LastRelativeDirection = input.moveMag > 0 ? NormalizeAny(input.moveDir) : Vec3{};
    m_LastDampedRelativeDirection = m_LastRelativeDirection;
    m_LastInputLength = input.moveMag;  // ControlScheme::GetStickLength
    const bool sightIsNull = std::fabs(sight.x) <= 0.0005f && std::fabs(sight.y) <= 0.0005f && std::fabs(sight.z) <= 0.0005f;
    if (m_LastInputLength > 0.35f && !sightIsNull) {
        m_LastInputAngle = GetAngleAboutK(m_LastRelativeDirection, sight);
        m_LastDampedInputAngle = GetAngleAboutK(m_LastDampedRelativeDirection, sight);
    }
    m_PortPreviousStickBand = m_PortStickBand;
    m_PortStickBand = PortStickBand(m_LastInputLength);
    m_PortButton0 = input.held(port::kLegs);

    // PORT: the task's FSM runs the current context's update; the port has the ground one. The game's UpdateGround
    // (PC 0xE049A0) first runs a chain of handlers (mission interaction, rest, loot, formations, weapons, fight,
    // pick-up, carry, fight interaction; RE/21_input.md); any of them can take the frame. They are out of scope, so
    // the port always reaches UpdateGroundMovement.
    if (human.GetCurrentContextID() == HumanContextId::Ground)
        UpdateGroundMovement(human.GetContextBundle().GetGround(), sight, dt);
}

void AssassinTask::UpdateGroundMovement(IHumanGround& ground, const Vec3& sight, float dt) {
    // ReorientCaracterInFirstPerson (PC 0xDDD960) runs first: first-person view only, not ported.
    if (!m_PortAllActions) {
        ground.SetSpeedRatio(0);
        ground.SetDirection(NormalizeAny(sight));
        return;
    }

    // Break hard (PC 0xDF8139): while the stick rests, and on a reversal: the damped stick more than 135 degrees from
    // the sight, unless the player leads a group formation (IsPlayerNavigatingInAGroupFormation PC 0xDD9520; PORT:
    // no groups).
    if (JoyThresholdPressed(0))
        ground.SetBreakHard(true);
    else if (m_RotationDirection != 0 && !JoyThresholdPressed(1))
        ground.SetBreakHard(false);
    else
        ground.SetBreakHard(dot(m_LastDampedRelativeDirection, sight) < -0.70710677f);

    // The forced turn-around (PC 0xDF81F7): when the stick enters band 1 or 2 more than 120 degrees from the sight,
    // for at most 0.3 s, and only while HumanGround keeps it.
    if (JoyThresholdJustPressed(1) || JoyThresholdJustPressed(2)) {
        if (!m_ForceTurnAround && std::fabs(m_LastDampedInputAngle) > 2.0943952f) {
            m_ForceTurnAround = true;
            m_ForceTurnAroundTimerEnd = m_PortClock + 0.3f;
        }
    } else if (m_PortClock >= m_ForceTurnAroundTimerEnd || !ground.GetForceTurnAround()) {
        m_ForceTurnAround = false;
    }
    ground.SetForceTurnAround(m_ForceTurnAround);

    Vec3 direction = m_LastDampedRelativeDirection;

    // (The static speedMethod switch, PC dword_1E2C2B0, picks a SetMovementSpeed path instead; it's 0 in the game.)
    // The speed ratio from the stick (PC 0xDF834B; PC computes it in double): 0 up to 0.35, then linear to 1 at 1.
    if (m_LastInputLength > 0.35f) {
        const float v = static_cast<float>((static_cast<double>(m_LastInputLength) - static_cast<double>(0.35f)) /
                                           static_cast<double>(0.65f));
        m_StickSpeedRatio = 1.0f >= v ? v : 1.0f;
    } else {
        m_StickSpeedRatio = 0;
    }
    switch (m_PortMaxSpeed) {
        case AssassinAbilitySet::NoMovementAllowed:
            m_StickSpeedRatio = 0;
            break;
        case AssassinAbilitySet::Jog:
            if (m_IsHighProfile) {
                float v = m_StickSpeedRatio;
                if (!(0.0 < static_cast<double>(v))) v = 0;
                if (static_cast<double>(v) >= static_cast<double>(0.3f)) v = 0.3f;
                m_StickSpeedRatio = v;
            }
            break;
        default:
            break;
    }

    // The turn slowdown (PC 0xDF83F3): the damped stick's angle from the sight beyond 45 degrees lowers the target
    // linearly to 0 at 90; the factor falls at 10/s (not below 0.1) while above the target, else rises at 5/s.
    // Only in high profile; otherwise 1.
    {
        float angle = std::fabs(m_LastDampedInputAngle);
        if (!(static_cast<double>(kHalfPi) >= static_cast<double>(angle))) angle = kHalfPi;
        float over = 0;
        if (static_cast<double>(angle) > static_cast<double>(kQuarterPi))
            over = static_cast<float>((static_cast<double>(angle) - static_cast<double>(kQuarterPi)) /
                                      static_cast<double>(kQuarterPi));
        float factor = m_TurnSlowdownFactor;
        if (static_cast<double>(factor) > 1.0 - static_cast<double>(over)) {
            factor = static_cast<float>(static_cast<double>(factor) - static_cast<double>(dt) * 10.0);
            if (!(static_cast<double>(factor) >= static_cast<double>(0.1f))) factor = 0.1f;
        } else {
            factor = static_cast<float>(static_cast<double>(dt) * 5.0 + static_cast<double>(factor));
            if (!(1.0f >= factor)) factor = 1.0f;
        }
        m_TurnSlowdownFactor = factor;
        if (!m_IsHighProfile) m_TurnSlowdownFactor = 1.0f;
    }

    // Walls (PC 0xDF8518): carrying a body, the target leans 0.2 along the stick.
    const bool carrying = ground.IsCarryingObject();
    int wallReorientation = 0;
    if (m_LastInputLength > 0.35f && !ground.IsInObstacleCollision()) {
        const Vec3 target = carrying ? m_LastDampedRelativeDirection * 0.2f : Vec3{};
        wallReorientation = PortReorientDirectionFromWallCollision(direction, 0.0f, kQuarterPi, target, direction);
    }

    // Crowds (PC 0xDF85B8), in low profile moving within 45 degrees of the sight.
    float crowd = 1.0f;
    if (!m_IsHighProfile && m_LastInputLength > 0.35f && dot(sight, m_LastDampedRelativeDirection) > 0.70710677f) {
        float obstruction = 0;
        if (!ground.IsPickPocketing()) obstruction = PortGetCrowdObstructionFactor(m_LastDampedRelativeDirection);
        if (static_cast<double>(0.95f) > static_cast<double>(obstruction)) {
            double scale = 0.5;
            if (ground.IsPushing())
                scale = static_cast<double>(0.1f);
            else if (ground.GetGentlePushIntention())
                scale = static_cast<double>(0.2f);
            crowd = static_cast<float>(1.0 - static_cast<double>(obstruction) * scale);
        } else {
            crowd = 0;
        }
    }
    const float shove = ground.IsInCrowdShove() ? 0.0f : 1.0f;
    const float praying = ground.IsPraying() ? 0.75f : 1.0f;
    float factor = m_TurnSlowdownFactor;
    if (!(crowd >= factor)) factor = crowd;
    if (!(shove >= factor)) factor = shove;
    if (!(praying >= factor)) factor = praying;
    if (wallReorientation == 2) {
        factor = 0;
        ground.SetBreakHard(true);
    }
    // Carrying a body onto a beam stops it (IHumanGround::DetectBeamInDirection, PC 0xDF879B). PORT: not ported;
    // nothing is carried in the port.
    ground.SetSpeedRatio(static_cast<float>(static_cast<double>(m_StickSpeedRatio) * static_cast<double>(factor)));

    // The heading rotation speeds (PC 0xDF8818); live-verified (RE/20 row 6).
    float minSpeed = 1.5f;
    float maxSpeed = 4.0f;
    if (carrying && m_IsHighProfile) {
        minSpeed = 0.75f;
        maxSpeed = 2.0f;
    } else if (ButtonPressed(0)) {
        if (ground.IsPraying()) {
            minSpeed = 0.375f;
            maxSpeed = 3.0f;
        } else {
            minSpeed = 0.975f;
            maxSpeed = 2.6f;
        }
    }
    ground.SetHeadingMinRotationSpeed(minSpeed);
    ground.SetHeadingMaxRotationSpeed(maxSpeed);

    // The rotation direction (PC 0xDF88DD): with IHumanGround::IsOrientable and no wall reorientation, the game picks
    // a side from the damped angle beyond 30 degrees, then clears m_RotationDirection on every path and asks for the
    // short way. The net effect is kept.
    m_RotationDirection = 0;
    ground.SetHeadingRotationDirection(0);

    // The direction (PC 0xDF8B01): the stick's, or the flat sight with no stick.
    if (m_LastInputLength > 0.0f)
        ground.SetDirection(direction);
    else
        ground.SetDirection(NormalizeAny(Vec3{sight.x, sight.y, 0}));

    m_InPreciseTargetingMode = m_IsHighProfile ? false : ButtonPressed(0);
    ground.SetCrouch(false);

    // Sprint: button 0, with the Sprint ability (PC 0xDF8BA9). OnSprintBegin (PC 0xDEB5E0) on a new press isn't
    // decoded.
    if (m_PortMaxSpeed == AssassinAbilitySet::Sprint)
        ground.SetSprint(ButtonPressed(0));
    else
        ground.SetSprint(false);
    // High profile runs when the ability allows more than a walk (PC 0xDF8C08).
    ground.SetMovementType(m_IsHighProfile && m_PortMaxSpeed > AssassinAbilitySet::Walk ? 1 : 0);
}

}  // namespace scimitar::ac2
