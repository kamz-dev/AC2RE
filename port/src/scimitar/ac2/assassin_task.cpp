#include "scimitar/ac2/assassin_task.h"

#include <cmath>

#include "port/input.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ac2/human_in_air.h"

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

// Gear::GetAngle(a, b, BaseK) (scimitar/math.h).
float GetAngleAboutK(const Vec3& a, const Vec3& b) { return Gear::GetAngle(a, b, Gear::kBaseK); }
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
    // The camera-relative stick, normalised, and its damped version: ControlScheme::GetRelativeStick /
    // GetDampedRelativeStick return what PadInput::ComputeRelativeInput (PC 0x1E84A0) stored. PORT: the port's input
    // is a world direction; it goes back to stick space through the camera's heading.
    Vec3 stick;
    if (input.moveMag > 0) {
        const Vec3 world = NormalizeAny(Vec3{input.moveDir.x, input.moveDir.y, 0});
        const float c = std::cos(-input.cameraYaw), s = std::sin(-input.cameraYaw);
        stick = Vec3{world.x * c - world.y * s, world.x * s + world.y * c, 0};
    }
    const PadInput::RelativeInput rel = m_PadInput.ComputeRelativeInput(stick, 0, input.cameraYaw);
    m_LastRelativeDirection = NormalizeAny(rel.relative);
    m_LastDampedRelativeDirection = rel.damped;
    m_LastInputLength = PadInput::CapLength(input.moveMag);  // ControlScheme::GetStickLength
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
        UpdateGroundMovement(human, human.GetContextBundle().GetGround(), human.m_Position, sight, dt);
    else if (human.GetCurrentContextID() == HumanContextId::InAir)
        UpdateInAir(human.GetContextBundle().GetInAir(), sight);
}

void AssassinTask::UpdateInAir(HumanInAir& inAir, const Vec3& sight) {
    // The landing speed ratio (PC 0xDEC804): 0 when the stick points more than 60 degrees away from the sight
    // (the angle counts only above the 0.35 dead zone); otherwise by high profile, the stick length and button 0. The
    // values sit just under InAir's landing bands (0.2 / 0.5 / 0.9): standing, walking, running, sprinting.
    float angle = 0.0f;
    if (m_LastInputLength > 0.35f) angle = GetAngleAboutK(sight, m_LastRelativeDirection);
    float ratio = 0.0f;
    if (std::fabs(angle) <= 1.0471976f) {
        if (m_IsHighProfile)
            ratio = m_LastInputLength <= 0.5f ? 0.19f : (ButtonPressed(0) ? 1.0f : 0.89f);
        else
            ratio = m_LastInputLength <= 0.5f ? 0.09f : 0.49f;
    }
    inAir.SetLandingSpeedRatio(ratio);  // IHumanInAir +0x40
}

void AssassinTask::UpdateGroundMovement(IHuman& human, IHumanGround& ground, const Vec3& position, const Vec3& sight,
                                        float dt) {
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

    // PORT: PC 0xDF8C2D..0xDF941E isn't ported: ladders, object use areas, interactions, the beam probe (the
    // CanEnterBeam out-flag below stays false) and the spring jump (IsRunning, button 0, CanEnterSpringJump(60 deg):
    // DoEnterSpringJump ends the frame).
    const bool beamAhead = false;

    // The gaps (PC 0xDF941E, Mac 0x1080A91). The search direction is the stick above the dead zone, else the sight.
    std::vector<Gap> gaps;
    human.GetGapList(gaps, position, 0.75f, 0.45f, true);
    const Vec3 searchDir = m_LastInputLength > 0.35f ? m_LastRelativeDirection : sight;
    Gap gapA, gapB, gapC;
    const bool foundA = human.GetBestGapInDirectionInList(gapA, gaps, position, searchDir, 0.75f, 1.7453293f);  // 100 deg
    const bool foundB = human.GetBestGapInDirectionInList(gapB, gaps, position, sight, 0.75f, 3.1415927f);
    const bool foundC = human.GetClosestGapInList(gapC, gaps, position, 0.75f);
    const float aboveWater = gapA.m_EdgePosition.z - human.GetWaterLevel();
    human.SetIsGapValid(foundB);
    human.SetNearestGap(gapC);
    human.SetIsNearestGapValid(foundC);

    // Walking up to a deep edge (PC 0xDF96E9). (The targeting settings' downward-target position, +0x7F0 / +0x7E0,
    // is set from gap A here too; not ported.)
    if (foundA && !beamAhead && m_LastInputLength > 0.35f) {
        const float toSight = Gear::GetAngleAbsolute(gapA.m_EdgeNormal, sight);
        if (aboveWater > 0.1f && 0.16f > gapA.m_SquareDistance && gapA.m_Depth > 2.0f && 1.2217305f > toSight) {
            if (!m_IsHighProfile) ground.SetBreakHard(true);
            const float toStick = Gear::GetAngleAbsolute(gapA.m_EdgeNormal, m_LastRelativeDirection);
            if (1.2217305f > toStick) {  // 70 degrees
                if (!m_GapForStopWalking.m_IsSet) m_GapForStopWalking.m_TimeSinceSet = m_PortClock;
                m_GapForStopWalking.m_Gap = gapA;
                m_GapForStopWalking.m_IsSet = true;
            } else {
                m_GapForStopWalking.m_IsSet = false;
            }
        } else {
            m_GapForStopWalking.m_IsSet = false;
        }
    } else {
        m_GapForStopWalking.m_IsSet = false;
    }
    if (m_GapForStopWalking.m_IsSet && (!m_IsHighProfile || ground.IsCarryingObject())) ground.SetSpeedRatio(0);

    // Edge clamping (PC 0xDF9853): carrying, any gap over 0.45 m; in low profile a gap over 2 m; in high profile only
    // a gap over 5 m while the stick is pushed, and then directions within 60 degrees of the stick aren't clamped.
    // Checked live on roofs of 1.2, 2.3 and 7+ m (RE/20 row 12, 2026-10-22).
    EdgeClampingSettings& settings = human.GetEdgeClampingSettings();
    bool clamp = false;
    if (ground.IsCarryingObject()) {
        if (foundA && gapA.m_Depth > 0.45f) {
            clamp = true;
            settings.m_RejectAngle = 0;
            settings.m_DirectionForRejectAngle = m_LastRelativeDirection;
        }
    } else if (aboveWater > 0.1f) {
        if (m_IsHighProfile) {
            if (m_LastInputLength > 0.35f && foundA && gapA.m_Depth > 5.0f) {
                clamp = true;
                settings.m_RejectAngle = 1.0471976f;
                settings.m_DirectionForRejectAngle = m_LastRelativeDirection;
            }
        } else if (foundA && gapA.m_Depth > 2.0f) {
            clamp = true;
            settings.m_RejectAngle = 0;  // the direction isn't written
        }
    }
    human.SetEdgeClamping(clamp);

    // PC 0xDF994D..: a flag of the jump code is cleared by its timer or 0.5 m away (not ported). Then the clamping's
    // gap: gap B, the best gap towards the sight.
    if (foundB) human.SetGap(gapB);

    // The edge reactions (PC 0xDF9997..0xDFA850, RE/21_input.md "Edge reactions"). PORT: the jumps aren't ported
    // (the free-run edge jump at PC 0xDF9D7A, the run-off jump at 0xDFA3DF: no jump targets or jump states), nor
    // beams, cannons, boats and the overhead jump; the abilities they test count as granted.
    // LookDown (PC 0xDFA58B): GapForStopWalking held for more than 0.25 s over a drop deeper than 2 m.
    if (m_GapForStopWalking.m_IsSet && m_GapForStopWalking.m_Gap.m_Depth > 2.0f &&
        m_PortClock - m_GapForStopWalking.m_TimeSinceSet > 0.25f && ground.CanEnterLookDown()) {
        ground.DoEnterLookDown(m_GapForStopWalking.m_Gap);
        return;
    }
    // Unbalance (PC 0xDFA5F7): within 0.15 m (square distance 0.0225), facing out, over a drop deeper than 5 m.
    int edgeDirection = 0;
    if (foundB && ground.CanUnbalanceOnEdge(gapB, 0.0225f, edgeDirection, false) && edgeDirection == 1 &&
        gapB.m_Depth > 5.0f) {
        ground.DoUnbalanceOnEdge(gapB, 1, false);
        return;
    }
    // FallOffEdge (PC 0xDFA657). PORT: +0x951 (set by the run-off jump) is never set; no ground tackle.
    if (!foundB) return;
    if (ground.CanFallOffEdge(gapB, false, false)) {
        ground.DoFallOffEdge(gapB, m_IsHighProfile ? 1.0f : 0.0f);
        return;
    }
    // PullDown (PC 0xDFA6CA): from EdgeStop while the stick still points within 70 degrees of the edge's normal, or
    // on a grasp press (pad button 3, ButtonJustPressedWithDelay(3, 0.2)). PORT: the port has no button 3, and the
    // prompt (ControlTypes +4 = 0x38) isn't ported.
    if (ground.CanPullDownOnEdge(gapB)) {
        if (ground.IsInEdgeStop()) {
            if (m_LastInputLength > 0.35f &&
                Gear::GetAngleAbsolute(gapB.m_EdgeNormal, m_LastRelativeDirection) < 1.2217305f)
                ground.DoPullDownOnEdge(gapB);
        }
    }
}

}  // namespace scimitar::ac2
