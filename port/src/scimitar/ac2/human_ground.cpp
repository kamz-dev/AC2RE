#include "scimitar/ac2/human_ground.h"

#include <cmath>

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace scimitar::ac2 {

// --- UpdateMoveCycleMain's rules (PC 0x61BB60, Mac 0xFCD20A; RE/11_ground.md 5) ---

float HumanGround::ComputeTargetSpeedRatio(float stickMagnitude, bool sprint, bool highProfile, bool aiOverride,
                                           float aiSpeedRatio) {
    float target;
    if (aiOverride) {
        target = aiSpeedRatio;  // +0x7C0 when +0x8A8 is set
    } else if (highProfile) {
        target = stickMagnitude * 0.2f + (sprint ? 0.8f : 0.6f);
    } else {
        target = stickMagnitude * 0.2f + (sprint ? 0.2f : 0.0f);
    }
    return target > 1.0f ? 1.0f : target;
}

float HumanGround::RestrictTargetSpeedRatio(float target, TransitionToMotionType restrictionMode, bool isPlayer, bool sprint,
                                            bool highProfile, float currentRatio) {
    float t = target;
    float cap = 0.6f;
    if (!isPlayer) {  // entity type (entity +0xA8 & 7) != 1
        if (t <= 0.2f) t = 0.2f;
        if (t >= 0.6f) t = 0.6f;
    }
    switch (restrictionMode) {  // +0x994
        case TransitionToMotionType_JogOnly:
            t = cap;
            break;
        case TransitionToMotionType_BlendWalkToJog:
        case TransitionToMotionType_BlendWalkToJogFromCollide:
            if (t < 0.2f)
                t = 0.2f;
            else if (cap < t)
                t = cap;
            break;
        case TransitionToMotionType_BlendWalkToJogWithRotation:
            t = currentRatio < 0.6f ? 0.2f : cap;
            break;
        case TransitionToMotionType_BlendWalkVerySlowToWalkToJogWithRotation:
            if (t <= 0.0f) t = 0.0f;
            if (t >= 0.6f) t = cap;
            break;
        case TransitionToMotionType_BlendWalkToJogToSprint:
        case TransitionToMotionType_BlendWalkToJogToSprintWithFixedLayer:
            if (t < 0.2f) t = 0.2f;
            [[fallthrough]];
        case TransitionToMotionType_BlendWalkVerySlowToWalkToJogToSprint:
            if (sprint) cap = 1.0f;
            if (cap < t) t = cap;
            break;
        default:
            break;
    }
    if (!highProfile && t > 0.2f) t = 0.2f;
    return t;
}

float HumanGround::StepSpeedRatio(float current, float target, float step, float slowDownCurve) {
    if (std::fabs(target - current) <= 0.0005f) return current;
    if (current <= target) {
        float r = current + step;
        return target < r ? target : r;
    }
    float r = current - step * slowDownCurve;
    return r < target ? target : r;
}

void HumanGround::GaitFromSpeedRatio(float ratio, int& gaitLevel, int& running) {
    if (ratio <= 0.2f) {
        running = 0;
        gaitLevel = ratio >= 0.1f ? 2 : 1;
    } else if (ratio <= 0.4f) {
        running = 0;
        gaitLevel = 2;
    } else if (ratio <= 0.6f) {
        running = 1;
        gaitLevel = 1;
    } else {
        running = 1;
        gaitLevel = 2;
    }
}

// --- Turning (RE/11_ground.md 7) ---

namespace {
constexpr float kTwoPi = 6.28318531f;

// An angle wrapped to (-pi, pi].
float WrapAngle(float a) {
    while (a > 3.14159265f) a -= kTwoPi;
    while (a <= -3.14159265f) a += kTwoPi;
    return a;
}

bool NullWithEpsilon(const Vec3& v, float eps) {
    return std::fabs(v.x) <= eps && std::fabs(v.y) <= eps && std::fabs(v.z) <= eps;
}
}  // namespace

Vec3 RotateVectorToVectorWithAngularSpeed(const Vec3& from, const Vec3& to, float minSpeed, float maxSpeed,
                                          float minAngle, float maxAngle, int direction, float dt) {
    Vec3 f = normalized(Vec3{from.x, from.y, 0});
    Vec3 t = normalized(Vec3{to.x, to.y, 0});
    if (std::fabs(f.x - t.x) <= 0.001f && std::fabs(f.y - t.y) <= 0.001f) return to;  // already there
    const float yawFrom = yawFromDirection(f);
    const float delta = WrapAngle(yawFromDirection(t) - yawFrom);

    float sign, angle;
    if (direction != 0) {  // forced side: the angle is measured that way round, 0..2pi
        sign = direction > 0 ? 1.0f : -1.0f;
        angle = sign * delta;
        if (angle < 0.0f) angle += kTwoPi;
    } else {  // the short way; PORT: exactly opposite vectors turn counter-clockwise (the game's axis not checked)
        sign = delta >= 0.0f ? 1.0f : -1.0f;
        angle = std::fabs(delta);
    }

    float speed;
    if (angle < maxAngle)
        speed = minAngle <= angle ? (angle - minAngle) / (maxAngle - minAngle) * (maxSpeed - minSpeed) + minSpeed
                                  : minSpeed;
    else
        speed = maxSpeed;

    if (speed * dt <= angle) return forwardFromYaw(yawFrom + sign * speed * dt);
    return to;
}

// --- The context ---

HumanGround::HumanGround(Human& human) : ActorContext(human) {
    // PORT: the game calls LocalInit through the vftable when the actor is initialised; the port does it here.
    LocalInit();
}

void HumanGround::LocalInit() {
    // PC 0x61F920: the parts the port mirrors so far.
    HumanGroundData& data = m_Human.GetGroundData();
    data.m_SubState = HumanGroundData::SubState_Movement;
    data.m_DestHeading = {};
    data.m_ParamFlags = 0;
    data.m_InternalFlags = 0;
    data.m_HeadingLinearRotationSpeed = 0;
    data.m_CurSpeedRatio = 0;
    data.m_DestSpeedRatio = 0;
    for (int h = 0; h < 2; ++h) {
        data.m_NbMvtDivisions[h] = 3;
        data.m_MvtDivThreshold[h][0] = 0.0f;
        data.m_MvtDivThreshold[h][1] = 0.9f;
        data.m_MvtDivThreshold[h][2] = 1.0f;
    }
    data.m_DestMvtType = false;
    m_SlowDownCurve.Clear();
    m_SlowDownCurve.AddKey(0.0f, 1.0f);
    m_SlowDownCurve.AddKey(0.333f, 1.0f);
    m_SlowDownCurve.AddKey(0.4f, 0.3f);
    m_SlowDownCurve.AddKey(0.666f, 0.2f);
    m_SlowDownCurve.AddKey(1.0f, 1.0f);
    data.m_BreakHard = false;
    data.m_Crouch = false;
    data.m_Sprint = false;
    m_TurnAngle = 0;
}

void HumanGround::Enter() {
    // PORT: the game enters the FSM's initial states (Vulnerable > Navigation > Move); the port enters Move directly.
    EnterMove();
}

void HumanGround::EnterMove() {
    // PC 0x60C610, Mac 0xFA3D06: HumanGroundData's MovementState (left by the context handing over, through
    // TransitionSetupDataToMovement) picks Move's first child and seeds the gait fields.
    HumanGroundData& data = m_Human.GetGroundData();
    data.m_SubState = HumanGroundData::SubState_Movement;
    data.m_DestHeading = forwardFromYaw(m_Human.m_Yaw);  // DestHeading := the sight
    data.m_CurSpeedRatio = 0;
    data.m_DestSpeedRatio = 0;
    data.m_ParamFlags |= 1;
    data.m_InternalFlags |= 1;
    State first = State::Wait;  // PORT: for other values the game keeps Move's previous initial child
    auto seed = [this](int running, int gait, float ratio) {
        m_CurrentMvtType = running;
        m_CurrentMvtState = gait;
        m_CurrentSpeedRatio = ratio;
    };
    switch (data.m_DestMovementState) {
        case HumanGroundData::MovementState_MAX: first = State::Wait; seed(0, 0, 0.0f); break;  // (+0x224 := 1, not mirrored)
        case HumanGroundData::MovementState_HandPassOverReception: first = State::HandPassOverReception; seed(1, 2, 0.6f); break;
        case HumanGroundData::MovementState_Landing: first = State::Landing; seed(0, 0, 0.0f); break;
        case HumanGroundData::MovementState_LandingHard: first = State::LandingHard; seed(0, 0, 0.0f); break;
        case HumanGroundData::MovementState_LandingBalanceFall: first = State::LandingBalanceFall; seed(1, 0, 0.0f); break;
        case HumanGroundData::MovementState_WaitLow: first = State::Wait; seed(0, 0, 0.0f); break;
        case HumanGroundData::MovementState_WaitHigh: first = State::Wait; seed(1, 0, 0.0f); break;
        case HumanGroundData::MovementState_WalkVerySlow: first = State::MoveCycle; seed(0, 1, 0.0f); break;
        case HumanGroundData::MovementState_Walk: first = State::MoveCycle; seed(0, 2, 0.2f); break;
        case HumanGroundData::MovementState_Jog:
        case HumanGroundData::MovementState_Run: first = State::MoveCycle; seed(1, 2, 0.6f); break;
        case HumanGroundData::MovementState_Sprint:
            first = State::MoveCycle;
            seed(1, 2, 1.0f);
            if (data.m_TransitionToMotionType == TransitionToMotionType_BlendWalkToJog)
                data.m_TransitionToMotionType = TransitionToMotionType_BlendWalkToJogToSprint;
            break;
        case HumanGroundData::MovementState_Crouch: first = State::CrouchWait; seed(0, 0, 0.0f); break;
        default: break;
    }
    m_IntentionMvtType = m_CurrentMvtType;
    m_IntentionMvtState = m_CurrentMvtState;
    SwitchMoveState(first);
}

void HumanGround::UpdateMovementIntention() {
    const HumanGroundData& data = m_Human.GetGroundData();
    if (m_SpeedControlType) {
        m_IntentionMvtState = m_IntentionSpeedRatio > 0.0f ? 2 : 0;
    } else {
        int division = data.GetMovementDivisionFromSpeedRatio(data.m_DestMvtType, data.m_DestSpeedRatio);
        if (division == 0)
            m_IntentionMvtState = 0;
        else if (division == 1 || division == 2)
            m_IntentionMvtState = 2;
    }
    m_IntentionMvtType = data.m_DestMvtType ? 1 : 0;
}

void HumanGround::UpdateTurnAngle() {
    // PC 0x5FE6A0: Gear::GetAngle(DestHeading, sight, BaseK), positive when the sight is counter-clockwise from the
    // move direction, i.e. when the move direction is to the right.
    const Vec3& move = m_Human.GetGroundData().m_DestHeading;
    if (NullWithEpsilon(Vec3{move.x, move.y, 0}, 0.0005f)) {
        m_TurnAngle = 0;
        return;
    }
    m_TurnAngle = Gear::GetAngle(move, forwardFromYaw(m_Human.m_Yaw), Gear::kBaseK);
}

void HumanGround::UpdateOrientation(float dt) {
    HumanGroundData& data = m_Human.GetGroundData();
    float ratio = data.m_RotationRatio + dt * data.m_RotationRatioStep;
    if (ratio > 1.0f) ratio = 1.0f;
    data.SetRotationRatio(ratio);

    // PORT: no action system, so the current action item's 0x10 flag (no turning) is never set.
    Vec3 sight = forwardFromYaw(m_Human.m_Yaw);
    Vec3 move{data.m_DestHeading.x, data.m_DestHeading.y, 0};
    if (NullWithEpsilon(sight, 0.0005f) || NullWithEpsilon(move, 0.0005f)) return;
    Vec3 next = RotateVectorToVectorWithAngularSpeed(sight, move, m_HeadingMinRotationSpeed,
                                                     m_HeadingMaxRotationSpeed, m_HeadingMinRotationAngle * ratio,
                                                     m_HeadingMaxRotationAngle * ratio, m_HeadingRotationDirection,
                                                     dt);
    if (std::fabs(next.x - sight.x) > 0.001f || std::fabs(next.y - sight.y) > 0.001f)
        m_Human.m_Yaw = yawFromDirection(next);
}

void HumanGround::UpdateMoveMain(float dt) {
    // PreprocessCharge and EmitPresentationMovementType (combat / presentation) are left out; so is the push handler.
    UpdateMovementIntention();
    UpdateTurnAngle();
    if (m_CurrentMvtState) UpdateOrientation(dt);
}

void HumanGround::UpdateMoveCycleMain(float dt) {
    const HumanGroundData& data = m_Human.GetGroundData();
    float target = ComputeTargetSpeedRatio(data.m_DestSpeedRatio, data.m_Sprint,
                                           m_IntentionMvtType != 0, m_SpeedControlType != 0,
                                           m_IntentionSpeedRatio);
    // PC 0x61BD2D: while the current action isn't the move cycle (93469659) and won't finish this frame, the target is
    // restricted (mode +0x994, then <= 0.2 without high profile), the ratio steps by plain dt, and the bands / gait
    // aren't updated (the branch returns). The step is the frame dt (RE/20 row 4).
    if (!PortIsMoveCycleActionCurrent(dt)) {
        const float t = RestrictTargetSpeedRatio(target, m_TransitionToMotionTypeInternal, /*isPlayer=*/true, data.m_Sprint,
                                                 m_IntentionMvtType != 0, m_CurrentSpeedRatio);
        m_CurrentSpeedRatio = StepSpeedRatio(m_CurrentSpeedRatio, t, dt, 1.0f);
        // (UpdateMoveDefaultBlendFactors(1, 1): animation layer)
        if (m_DoubleTransitionToMotionSpeed & 1) m_PortActionTimeScale = 2.0f - m_PortAction.Progress();
        return;
    }
    // The move cycle is current. The port's Human is the player; slowing down follows the curve unless the AI
    // override is on.
    const float curve = m_SpeedControlType ? 1.0f : m_SlowDownCurve.GetOutput(m_CurrentSpeedRatio);
    m_CurrentSpeedRatio = StepSpeedRatio(m_CurrentSpeedRatio, target, dt, curve);
    GaitFromSpeedRatio(m_CurrentSpeedRatio, m_CurrentMvtState, m_CurrentMvtType);
}

// --- IHumanGround setters that write HumanGroundData ---

void HumanGround::SetDirection(const Vec3& direction) {
    // Mac 0xF98EE6.
    m_Human.GetGroundData().m_DestHeading = direction;
}

void HumanGround::SetSpeedRatio(float ratio) {
    // Mac 0xF9B710. PORT: the game also sets m_IntentionSpeed to GetMovementSpeedFromRatio(ratio) (0 when |ratio| <=
    // 0.0005); the port has no speed table field yet.
    m_SpeedControlType = 0;  // SpeedControlType_MvtTypeAndMvtState
    m_Human.GetGroundData().m_DestSpeedRatio = ratio;
    m_IntentionSpeedRatio = ratio;
}

void HumanGround::SetMovementType(int type) {
    // Mac 0xFBA59A. The ProfileChangeEvent it sends when the type changes (presentation) isn't ported.
    m_Human.GetGroundData().m_DestMvtType = static_cast<unsigned char>(type);
}

bool HumanGround::GetSprint() const { return m_Human.GetGroundData().m_Sprint; }

void HumanGround::SetBreakHard(bool v) { m_Human.GetGroundData().m_BreakHard = v; }
void HumanGround::SetSprint(bool v) { m_Human.GetGroundData().m_Sprint = v; }
void HumanGround::SetCrouch(bool v) { m_Human.GetGroundData().m_Crouch = v; }

int HumanGround::GetTurnAnimation(float a) {
    if (a <= 0.0f) {
        if (a > -0.43633232f) return 1;
        if (a > -1.2217305f) return 3;
        if (a > -2.3561945f) return 5;
        return 7;
    }
    if (a < 0.43633232f) return 0;
    if (a < 1.2217305f) return 2;
    if (a < 2.3561945f) return 4;
    return 6;
}

// --- Crouch and TurnOnSpot (RE/11_ground.md 13) ---

bool HumanGround::PortIsActionInterruptibleByMovement() const {
    // PORT: the game reads the current action item's flags; the port treats a finished (or absent) stand-in action as
    // interruptible and a playing one as not.
    return m_PortAction.HasFinished();
}

bool HumanGround::ActorCanPhysicallyStandUp(const port::World& world) const {
    // PC 0x5F72C0: Human::CheckClearanceCapsuleGlobal (PC 0x934800) with a 0.38 m capsule from the feet + 1.42 m down to
    // + 0.42 m. PORT: the world's capsule query stands in for the physics one.
    Vec3 top = m_Human.m_Position;
    top.z += 1.42f;
    Vec3 bottom = top;
    bottom.z -= 1.0f;
    return world.capsuleIsClear(top, bottom, 0.38f);
}

void HumanGround::SetCrouched(bool crouched) {
    // Mac 0xF97E58: presentation event 31 (enter / leave) when the value changes; not mirrored.
    m_IsCrouched = crouched;
}

void HumanGround::ExecWaitToCrouchWaitMain() {
    // Mac 0xFB9206: the crouch wait action (g_CrouchWaitActionMap[2 * foot + GetUseHideStance()]) with a 0.4 s
    // transition (through a hide-stance transition action when the hide stance is on); then the flag that holds
    // CrouchWait -> CrouchMove until that transition is 75 % done. (The actions are left to the animation layer.)
    m_IsCrouchTransitionFromExternal = true;
}

void HumanGround::ExecCrouchWaitToCrouchMoveMain() {
    // Mac 0xFB6D70: the crouch move action through g_CrouchWaitToMoveActionMap[foot]; the flag is cleared.
    m_IsCrouchTransitionFromExternal = false;
}

bool HumanGround::TestWaitToTurnOnSpotMain() const {
    // NPC turn mode; not in a custom action that orients the character; the current action finishing or
    // interruptible (PORT: one stand-in check); no movement intent; at least 1 degree to turn.
    if (m_TurnMode != 1) return false;
    if (IsInCustomAction() && GetCurrentCustomActionOrientation() != 2) return false;  // 2 = CustomActionOrientation_None
    return PortIsActionInterruptibleByMovement() && m_IntentionMvtState == 0 && std::fabs(m_TurnAngle) >= 0.017453292f;
}

bool HumanGround::IsInCustomAction() const {
    // The game: the CustomAction state is active, or the upper body (+0x790) plays a custom action (its +0x24 == 11).
    // PORT: the port has neither.
    return false;
}

int HumanGround::GetCurrentCustomActionOrientation() const {
    // The game: 2 (None) unless the upper body plays a custom action, then that action's orientation (0 Aggressive,
    // 1 Progressive, 2 None; WaitCustomActionUnit::CustomActionOrientation). PORT: no upper body.
    return 2;
}

bool HumanGround::TestTurnOnSpotDeadZone(const Vec3& direction, float minAngle) const {
    // The angle about +Z from the sight to `direction` (flattened; the sight itself if that is null) is >= minAngle.
    const Vec3 sight = forwardFromYaw(m_Human.m_Yaw);
    Vec3 dir{direction.x, direction.y, 0};
    if (std::fabs(dir.x) + std::fabs(dir.y) < 1e-6f) dir = sight;
    const float angle = yawFromDirection(dir) - yawFromDirection(sight);
    return minAngle <= std::fabs(std::remainder(angle, 2.0f * kPi));
}

bool HumanGround::CanTurnOnSpot(const Vec3& direction, float minAngle) {
    // Interface command 155 is offered by Wait only (TestWaitToTurnOnSpotOnHumanGroundInterfaceCommand_TurnOnSpot
    // Mac 0xFB3B88: 3 - TestTurnOnSpotDeadZone, i.e. 2 = allowed). PORT: the FSM's interface-command dispatch is
    // reduced to that one transition.
    return m_MoveState == State::Wait && TestTurnOnSpotDeadZone(direction, minAngle);
}

void HumanGround::DoTurnOnSpot(const Vec3& direction) {
    // _TransitFromWaitToTurnOnSpotOnHumanGroundInterfaceCommand_TurnOnSpot (Mac 0xFBB7C2): ExecWaitToTurnOnSpot...
    // (Mac 0xF98D70) takes the direction as the target, then TurnOnSpot is entered. PORT: as CanTurnOnSpot.
    if (m_MoveState != State::Wait) return;
    m_TurnOnSpotDir = direction;
    SwitchMoveState(State::TurnOnSpot);
}

void HumanGround::SetUseTurnOnSpotCycle(bool use) {
    if (IsInTurnOnSpotTurn()) m_TurnOnSpotCycle = use;  // Mac 0xF979FE: only while turning on the spot
}

bool HumanGround::TestTurnOnSpotStepToTurnOnSpotStepMain() const {
    // PC 0x5FE0A0: with the cycle flag set and the step's action finished, chain another step when channel 1 is busy
    // and its action has more time left than the next turn animation lasts (else the game clears the flag).
    // PORT: no channel 1 (the action system isn't ported), so it never chains; like the game when channel 1 is idle,
    // the flag is left set and the step holds until SetUseTurnOnSpotCycle(false).
    if (!m_TurnOnSpotCycle || !m_PortAction.HasFinished()) return false;
    const bool channel1Busy = false;
    return channel1Busy;
}

bool HumanGround::TestTurnOnSpotStepToWaitMain() const {
    return !m_TurnOnSpotCycle && m_PortAction.HasFinished();
}

void HumanGround::StartTurnOnSpotAnim(const Vec3& target) {
    // PC 0x606B70: the turn animation for the angle from the sight to `target` (GetTurnAnimation), weighted by the
    // angle over its size; PORT: the stand-in action spreads that angle over the animation's stand-in length.
    const float angle =
        std::remainder(yawFromDirection({target.x, target.y, 0}) - m_Human.m_Yaw, 2.0f * kPi);
    const int anim = GetTurnAnimation(angle);
    m_PortAction.Start(m_Human.GetPortActionLengths().turnOnSpot[anim / 2]);
    m_PortTurnToApply = angle;
}

void HumanGround::ExitTurnOnSpot() {
    // PC 0x60D7A0: DestHeading := the sight; the cycle flag is cleared. (The presentation event isn't mirrored.)
    m_Human.GetGroundData().m_DestHeading = forwardFromYaw(m_Human.m_Yaw);
    m_TurnOnSpotCycle = false;
}

void HumanGround::ExecTurnOnSpotStepToWaitMain() {
    // PC 0x5EA930: DestHeading and HeadingBeforeRotationUpdate := the sight; movement state Wait, ratio 0; the
    // intended movement type keeps the current one. (HeadingBeforeRotationUpdate isn't mirrored.)
    m_Human.GetGroundData().m_DestHeading = forwardFromYaw(m_Human.m_Yaw);
    m_CurrentMvtState = 0;
    m_CurrentSpeedRatio = 0;
    m_IntentionMvtType = m_CurrentMvtType;
    m_IntentionMvtState = 0;
}

bool HumanGround::TestWaitToCrouchWaitMain() const {
    return m_Human.GetGroundData().m_Crouch && m_IntentionMvtType == 0 && PortIsActionInterruptibleByMovement();
}

bool HumanGround::TestMoveCycleToCrouchMoveMain() const {
    return m_Human.GetGroundData().m_Crouch && m_CurrentMvtType == 0;
}

bool HumanGround::TestCrouchWaitToWaitMain(const port::World& world) const {
    return !m_Human.GetGroundData().m_Crouch && PortIsActionInterruptibleByMovement() && ActorCanPhysicallyStandUp(world);
}

bool HumanGround::TestCrouchWaitToCrouchMoveMain() const {
    // Mac 0xF9DF6A: movement intended and, after Wait -> CrouchWait, the stand -> crouch transition more than 75 %
    // done (the transition action's ratio, or the current blend's progress; done when there is no blend). PORT: no
    // action system, so like the game with no blend the transition counts as done.
    if (m_IntentionMvtState == 0) return false;
    if (m_IsCrouchTransitionFromExternal && PortCrouchTransitionProgress() <= 0.75f) return false;
    return true;
}

float HumanGround::PortCrouchTransitionProgress() const {
    // PORT: the stand -> crouch transition's progress (the transition action's ratio, else the blend's); no action
    // system, so it is done, as in the game when there is no blend.
    return 1.0f;
}

bool HumanGround::TestCrouchMoveToMoveCycleMain(const port::World& world) const {
    return (!m_Human.GetGroundData().m_Crouch || m_IntentionMvtType == 1) && ActorCanPhysicallyStandUp(world);
}

// --- Locomotion transitions (RE/11_ground.md 6 and 10) ---
// PORT: without an action system, "the current action can be cut by movement" (IsActionInterruptibleByMovement) is
// never true; WillActionFinish / HasActionFinished read the stand-in action; in MoveCycle the current action is
// always the move cycle; and Wait plays no action, so it is always free.

bool HumanGround::Has180TurnIntention() const {
    return m_ForceTurnAround || std::fabs(m_TurnAngle) > 2.0943952f;  // 2pi/3
}

bool HumanGround::TestWaitToTurnStartMain() const {
    return std::fabs(m_TurnAngle) > m_TurnOnMovementMinAngle && !m_OnlyUseTurnOnSpot;
}

bool HumanGround::TestWaitToMoveCycleMain() const {
    return m_IntentionMvtState == 1 || m_IntentionMvtState == 2;
}

bool HumanGround::TestMoveCycleToMoveStopMain() const {
    return m_CurrentMvtType == 1 && m_CurrentMvtState != 0 && m_Human.GetGroundData().m_BreakHard;
}

bool HumanGround::TestMoveCycleToWaitMain() const {
    return m_IntentionMvtState == 0 && m_CurrentMvtType == 0;
}

bool HumanGround::TestMoveCycleToTurnStartMain() const {
    return m_CurrentMvtType == 0 && std::fabs(m_TurnAngle) > m_TurnOnMovementMinAngle;
}

bool HumanGround::TestMoveStopToRunTurnMain(float dt) const {
    return Has180TurnIntention() && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestMoveStopToWaitMain() const {
    return m_PortAction.HasFinished() && m_IntentionMvtState == 0;
}

bool HumanGround::TestMoveStopToMoveCycleMain() const {
    return m_PortAction.HasFinished() && (m_IntentionMvtState == 1 || m_IntentionMvtState == 2);
}

bool HumanGround::TestTurnStartToWaitMain(float dt) const {
    return m_IntentionMvtState == 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestTurnStartToMoveCycleMain(float dt) const {
    return m_IntentionMvtState != 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestRunTurnToWaitMain() const {
    return m_IntentionMvtState == 0 && m_PortAction.HasFinished();
}

bool HumanGround::TestRunTurnToMoveCycleMain() const {
    return m_IntentionMvtState != 0 && (m_PortAction.HasFinished() || !Has180TurnIntention());
}

// --- Entering the states ---

void HumanGround::EnterMoveCycle() {
    // Mac 0xFBE706: the gait level takes the movement division and running takes the high-profile intention; the
    // blend weights and HumanGroundData's turn angle are reset. (The action set-up is left to the animation layer.)
    HumanGroundData& data = m_Human.GetGroundData();
    m_CurrentMvtState = m_IntentionMvtState;
    m_CurrentMvtType = m_IntentionMvtType;
    m_TransitionToMotionTypeInternal = data.m_TransitionToMotionType;  // this[605] = HumanGroundData +0x228
    TransitionSetupDataToMovement::ResetTransitionData(data);
    data.m_CurrentBodyAngle = 0;
}

void HumanGround::ExecTurnStartToWaitMain() {
    // PC 0x614F40, Mac 0xFB664C. The wait action (g_GroundWaitActionMap[12 * side + 6 * running + 3 * foot]) and
    // its blend by the turn angle are left to the animation layer. Live (RE/20 row 8): the ratio is 0 afterwards.
    m_IntentionMvtType = m_CurrentMvtType;
    m_CurrentSpeedRatio = 0;
}

void HumanGround::ExecTurnStartToMoveCycleMain() {
    // PC 0x615120, Mac 0xFB7474: seeds the speed ratio and TransitionToMotionType, which EnterMoveCycle then copies
    // into the speed restriction. The move cycle action (0x5923BDB) and its blend factors are left to the animation
    // layer. PORT: the +0x998 |= 1 of the high-profile branch isn't mirrored (the field's use isn't decoded).
    HumanGroundData& data = m_Human.GetGroundData();
    const bool isPlayer = true;  // entity type (entity +0xA8 & 7) == 1; the port's Human is the player
    if (m_CurrentMvtType) {
        m_CurrentSpeedRatio = 0.6f;
        data.m_TransitionToMotionType = TransitionToMotionType_BlendWalkToJogWithRotation;
    } else {
        if (m_IntentionMvtType) {
            m_DoubleTransitionToMotionSpeed |= 1;
            m_CurrentSpeedRatio = 0.6f;
        } else if (isPlayer && m_IntentionMvtState == 1)
            m_CurrentSpeedRatio = 0;
        else
            m_CurrentSpeedRatio = 0.2f;
        data.m_TransitionToMotionType = TransitionToMotionType_BlendWalkVerySlowToWalkToJogWithRotation;
    }
    // PORT: the move cycle action is targeted here (0x5923BDB) but isn't current at once (RE/20 row 8).
    m_PortAction.Start(m_Human.GetPortActionLengths().moveCycleEntry);
    m_PortMoveCycleEntryPending = true;
}

bool HumanGround::PortIsMoveCycleActionCurrent(float dt) {
    // PORT: the game asks the action component whether the current action is the move cycle or will finish. The port
    // only knows the one case seen live: after ExecTurnStartToMoveCycleMain, the previous action stays current for
    // PortActionLengths::moveCycleEntry. Other entries into MoveCycle count as current at once.
    if (m_PortMoveCycleEntryPending && m_PortAction.WillFinish(dt * m_PortActionTimeScale))
        m_PortMoveCycleEntryPending = false;
    return !m_PortMoveCycleEntryPending;
}

void HumanGround::ExitMoveCycle() {
    // PC 0x60DB70, Mac 0xF9FD94. (+0x8B4, the +0x398 block and the animation flags aren't mirrored.)
    HumanGroundData& data = m_Human.GetGroundData();
    data.m_CurrentBodyAngle = 0;
    data.m_CurrentBodyBankingAngle = 0;
    m_DoubleTransitionToMotionSpeed &= ~1;
}

void HumanGround::ExecMoveStopToWaitMain() {
    // Mac 0xF9748A, inlined in PC _TransitFromMoveStopToWaitMain 0x61F200. Live (RE/20 row 8): ratio 0 afterwards,
    // +0x7B4 at 1 for one frame (EnterWait's children reset it).
    m_IntentionMvtType = 1;
    m_CurrentSpeedRatio = 0;
}

void HumanGround::EnterTurnStart() {
    // PC 0x614BB0. The action is g_TurnStartActionMap[6 * side + 3 * running + foot] (big turns) or a 3-way blend
    // (small turns); only the field changes are mirrored here.
    const float a = m_TurnAngle;
    if (std::fabs(a) >= m_TurnOnMovementMinAngle) {
        m_TurnIntention = a >= 0.0f ? 1 : 0;
        m_IntentionMvtType = m_CurrentMvtType;
    } else {
        m_TurnIntention = a >= 0.0f ? 1 : 0;
    }
    m_IntendedTurnAngle = a;
    m_CurrentMvtState = 0;
    m_CurrentMvtType = m_IntentionMvtType;
    // PORT: the turn animation's length and rotation.
    m_PortAction.Start(m_Human.GetPortActionLengths().turnStart);
    m_PortTurnToApply = -a;  // the turn angle is positive to the right; the port's yaw counter-clockwise
}

void HumanGround::EnterMoveStop() {
    // PC 0x614370. The stop animation is g_MoveStopActionMap[foot], blended by the speed ratio it starts from.
    m_PortAction.Start(m_Human.GetPortActionLengths().moveStop);
    m_PortEntrySpeed = m_PortSpeed;
}

void HumanGround::EnterRunTurn() {
    // Mac 0xFB76DE. The action is g_RunTurnActionMap[foot]; the entity's banking is set.
    m_PortAction.Start(m_Human.GetPortActionLengths().runTurn);
    m_PortEntrySpeed = m_PortSpeed;
    m_PortTurnToApply = -m_TurnAngle;
}

// --- Landings and Roll (RE/11_ground.md 11). InAir picks the landing action (HumanInAir::ExecMainToGroundMain);
// TestLandingToWaitMain accepts the three standing ones. ---

bool HumanGround::IsStandingLanding() const {
    const std::uint32_t id = m_Human.m_PortCurrentActionId;
    return id == landing_action::StandingRightFoot || id == landing_action::StandingLeftFoot ||
           id == landing_action::StandingVertical;
}

bool HumanGround::TestLandingToWaitMain() const {
    // PC 0x5EB330: the landing action has finished and it is one of the three standing landings.
    return m_PortAction.HasFinished() && IsStandingLanding();
}

bool HumanGround::TestLandingToMoveCycleMain() const {
    // PC 0x5EB3A0: the landing action has finished and it is any other landing.
    return m_PortAction.HasFinished() && !IsStandingLanding();
}

void HumanGround::ExecLandingToMoveCycleMain() {
    HumanGroundData& data = m_Human.GetGroundData();
    HumanGroundData::MovementState state;
    if (m_IntentionMvtType) {
        m_CurrentSpeedRatio = data.m_Sprint ? 1.0f : 0.6f;
        state = data.m_Sprint ? HumanGroundData::MovementState_Sprint : HumanGroundData::MovementState_Jog;
        m_IntentionMvtType = 1;
    } else {
        m_CurrentSpeedRatio = 0.2f;
        state = HumanGroundData::MovementState_Walk;
        m_IntentionMvtType = 0;
    }
    m_IntentionMvtState = 2;
    TransitionSetupDataToMovement{state, 0.0f, 0.5f, TransitionToMotionType_BlendWalkToJog}.SetupTransitionData(data);
    data.m_TransitionToMotionType = TransitionToMotionType_BlendWalkToJogToSprintWithFixedLayer;
}

bool HumanGround::TestLandingHardToWaitMain(float dt) const {
    return m_IntentionMvtState == 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestLandingHardToMoveCycleMain(float dt) const {
    return m_IntentionMvtState != 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestLandingBalanceFallToWaitMain(float dt) const {
    return m_PortAction.WillFinish(dt);
}

bool HumanGround::TestRollToWaitMain(float dt) const {
    return m_PortAction.WillFinish(dt) && m_IntentionMvtState == 0;
}

bool HumanGround::TestRollToMoveCycleMain(float dt) const {
    return m_PortAction.WillFinish(dt) && m_IntentionMvtState != 0;
}

namespace {
// The states whose Enter sets Human::m_IsEdgeClampingActive (+0x991) and whose Exit clears it (Mac EnterWait 0xFBEA66,
// EnterMoveCycle 0xFBE706, EnterMoveStop 0xFB7804, EnterCrouchMove 0xFC3338 and their Exits; PC _EnterWait 0x61DFF2,
// EnterMoveStop 0x614577, EnterEdgeStop Mac 0xFC27A2). ObstacleCollision, Poisoned, CarryObject and Dodge do it too;
// the port doesn't have them.
bool ClampsAtEdges(HumanGround::State state) {
    return state == HumanGround::State::Wait || state == HumanGround::State::MoveCycle ||
           state == HumanGround::State::MoveStop || state == HumanGround::State::CrouchMove ||
           state == HumanGround::State::EdgeStop;
}
}  // namespace

void HumanGround::Exit() {
    // _ExitHumanGround (PC 0x634C00): the active branch's Exits, the leaf's first (RE/10_human_update.md 4). PORT: the
    // Exits of the port's states.
    if (m_MoveState == State::TurnOnSpot) ExitTurnOnSpot();
    if (m_MoveState == State::CrouchWait || m_MoveState == State::CrouchMove) SetCrouched(false);
    if (m_MoveState == State::MoveCycle) ExitMoveCycle();
    m_PortMoveCycleEntryPending = false;
    if (ClampsAtEdges(m_MoveState)) m_Human.m_IsEdgeClampingActive = false;
}

void HumanGround::Update(long phase, const PortUpdateArgs& args) {
    // _UpdateHumanGround(phase), PC 0x6381D0.
    if (phase == 0)
        UpdateMainPhase(args.world, args.dt);
    else
        UpdateAfterPhysicsPhase(args.world, args.dt);
}

void HumanGround::SwitchMoveState(State next) {
    if (ClampsAtEdges(m_MoveState)) m_Human.m_IsEdgeClampingActive = false;
    if (m_MoveState == State::TurnOnSpot && next != State::TurnOnSpot) ExitTurnOnSpot();
    // ExitCrouchWait / ExitCrouchMove (Mac 0xFC19D0 / 0xFC1A62): capsule height back to 1.8 (PORT: no capsule), not
    // crouched.
    if ((m_MoveState == State::CrouchWait || m_MoveState == State::CrouchMove) && next != m_MoveState) SetCrouched(false);
    if (next != State::MoveCycle) {
        if (m_MoveState == State::MoveCycle) ExitMoveCycle();
        m_PortMoveCycleEntryPending = false;
    }
    m_MoveState = next;
    m_PortActionTimeScale = 1;
    switch (next) {
        case State::MoveCycle: EnterMoveCycle(); break;
        case State::TurnStart: EnterTurnStart(); break;
        case State::MoveStop: EnterMoveStop(); break;
        case State::RunTurn: EnterRunTurn(); break;
        case State::Landing: m_PortAction.Start(m_Human.GetPortActionLengths().landing); break;
        case State::LandingHard: m_PortAction.Start(m_Human.GetPortActionLengths().landingHard); break;
        case State::LandingBalanceFall:
            m_PortAction.Start(m_Human.GetPortActionLengths().landingBalanceFall);
            break;
        case State::Roll:
            m_PortAction.Start(m_Human.GetPortActionLengths().roll);
            m_PortEntrySpeed = m_PortSpeed;
            break;
        case State::CrouchWait:  // Mac 0xFC316C: capsule height 1.0 (PORT: no capsule), running and gait 0
            m_CurrentMvtType = 0;
            m_CurrentMvtState = 0;
            SetCrouched(true);
            break;
        case State::CrouchMove:  // Mac 0xFC3338: capsule height 1.0, running 0, gait 1
            m_CurrentMvtType = 0;
            m_CurrentMvtState = 1;
            SetCrouched(true);
            break;
        case State::CrouchTurn:  // the game turns by the character controller's displacement (progression ratio)
            m_PortAction.Start(m_Human.GetPortActionLengths().crouchTurn);
            m_PortTurnToApply = -m_TurnAngle;
            break;
        case State::TurnOnSpot:  // _EnterTurnOnSpot: TurnOnSpotStep's StartTurnOnSpotAnim towards the target
            StartTurnOnSpotAnim(m_TurnOnSpotDir);
            break;
        case State::Wait:  // Wait's initial child is Idle
            m_WaitState = State::Idle;
            break;
        case State::EdgeStop:  // EnterEdgeStop (Mac 0xFC27A2): the unbalance action 115916159
            m_PortAction.Start(m_Human.GetPortActionLengths().edgeStop);
            break;
        default: break;
    }
    if (ClampsAtEdges(next)) m_Human.m_IsEdgeClampingActive = true;
}

void HumanGround::UpdateTurnStartMain() {
    // PC 0x5EB6F0: twice the speed in high profile, unless the action is one of the running turns (the
    // g_TurnStartActionMap entries with running = 1).
    const bool runningTurn = std::fabs(m_IntendedTurnAngle) >= m_TurnOnMovementMinAngle && m_CurrentMvtType == 1;
    m_PortActionTimeScale = (m_IntentionMvtType == 1 && !runningTurn) ? 2.0f : 1.0f;
}

float HumanGround::PortSpeedFromRatio() const {
    const float* s = m_Human.GetPortGroundSpeeds().atRatio;
    float x = m_CurrentSpeedRatio / 0.2f;
    int i = static_cast<int>(x);
    if (i >= 5) return s[5];
    if (i < 0) return s[0];
    float f = x - static_cast<float>(i);
    return s[i] + (s[i + 1] - s[i]) * f;
}

void HumanGround::UpdateMainPhase(const port::World& world, float dt) {
    Human& h = m_Human;

    // _UpdateMove (PC 0x634D20): UpdateMoveMain, then the current child's update and its transitions, in the
    // game's order.
    // PORT: EdgeStop is Navigation's child, not Move's, so Move's update doesn't run in it.
    if (m_MoveState != State::EdgeStop) UpdateMoveMain(dt);
    switch (m_MoveState) {
        case State::Wait:  // _UpdateWait PC 0x621A80
            if (TestWaitToTurnStartMain())
                SwitchMoveState(State::TurnStart);
            else if (TestWaitToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            else if (TestWaitToTurnOnSpotMain()) {
                m_TurnOnSpotDir = m_Human.GetGroundData().m_DestHeading;  // ExecWaitToTurnOnSpotMain, inlined in PC 0x61F0D0
                SwitchMoveState(State::TurnOnSpot);
            }
            else if (TestWaitToCrouchWaitMain()) {
                ExecWaitToCrouchWaitMain();
                SwitchMoveState(State::CrouchWait);
            }
            break;
        case State::EdgeStop:  // _UpdateEdgeStop PC 0x624150: UpdateEdgeStopMain loops 115916157 (PORT: no actions)
            if (TestEdgeStopToMoveMain())
                SwitchMoveState(State::Wait);  // PORT: ExecEdgeStopToMoveMain (Mac 0xFA9BFC) isn't decoded; Move's Wait
            break;
        case State::MoveCycle:  // _UpdateMoveCycle PC 0x621B90 (CrouchMove not in the port yet)
            UpdateMoveCycleMain(dt);
            if (TestMoveCycleToMoveStopMain())
                SwitchMoveState(State::MoveStop);
            else if (TestMoveCycleToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestMoveCycleToTurnStartMain())
                SwitchMoveState(State::TurnStart);
            else if (TestMoveCycleToCrouchMoveMain())
                SwitchMoveState(State::CrouchMove);
            break;
        case State::MoveStop:  // _UpdateMoveStop PC 0x621C80
            if (TestMoveStopToRunTurnMain(dt))
                SwitchMoveState(State::RunTurn);
            else if (TestMoveStopToWaitMain()) {  // _TransitFromMoveStopToWaitMain PC 0x61F200
                ExecMoveStopToWaitMain();
                SwitchMoveState(State::Wait);
            }
            else if (TestMoveStopToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            break;
        case State::TurnStart: {  // _UpdateTurnStart PC 0x621EE0
            UpdateTurnStartMain();
            const float scaled = dt * m_PortActionTimeScale;
            if (TestTurnStartToWaitMain(scaled)) {
                ExecTurnStartToWaitMain();
                SwitchMoveState(State::Wait);
            } else if (TestTurnStartToMoveCycleMain(scaled)) {
                ExecTurnStartToMoveCycleMain();
                SwitchMoveState(State::MoveCycle);
            }
            break;
        }
        case State::RunTurn:  // _UpdateRunTurn PC 0x621E60
            if (TestRunTurnToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestRunTurnToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            break;
        case State::Landing:  // _UpdateLanding PC 0x630800 (the Poisoned exit is left out)
            if (TestLandingToWaitMain()) {
                SwitchMoveState(State::Wait);
            } else if (TestLandingToMoveCycleMain()) {
                ExecLandingToMoveCycleMain();
                SwitchMoveState(State::MoveCycle);
            }
            break;
        case State::LandingHard:  // _UpdateLandingHard PC 0x630890
            if (TestLandingHardToWaitMain(dt)) {
                SwitchMoveState(State::Wait);
            } else if (TestLandingHardToMoveCycleMain(dt)) {
                // ExecLandingHardToMoveCycleMain (Mac 0xFA911E)
                m_CurrentSpeedRatio = m_IntentionMvtType ? 0.6f : 0.2f;
                TransitionSetupDataToMovement{m_IntentionMvtType ? HumanGroundData::MovementState_Jog : HumanGroundData::MovementState_Walk,
                                              0.0f, 0.5f, TransitionToMotionType_BlendWalkToJog}
                    .SetupTransitionData(m_Human.GetGroundData());
                SwitchMoveState(State::MoveCycle);
            }
            break;
        case State::LandingBalanceFall:  // _UpdateLandingBalanceFall PC 0x630940
            if (TestLandingBalanceFallToWaitMain(dt)) SwitchMoveState(State::Wait);
            break;
        case State::TurnOnSpot:  // _UpdateTurnOnSpotStep PC 0x621F90 (TurnOnSpot's only child, TurnOnSpotStep)
            if (TestTurnOnSpotStepToTurnOnSpotStepMain()) {
                // _TransitFrom...ToTurnOnSpotStepMain PC 0x60B960: the target becomes the upper body's custom-action
                // direction (HumanUpperBody::GetCustomActionTargetDirection, PC 0x6408E0), then the next animation.
                StartTurnOnSpotAnim(m_TurnOnSpotDir);
            } else if (TestTurnOnSpotStepToWaitMain()) {
                // _TransitFromTurnOnSpotStepToWaitMain PC 0x61F3F0: ExitTurnOnSpot (in SwitchMoveState), the Exec,
                // then Wait.
                SwitchMoveState(State::Wait);
                ExecTurnOnSpotStepToWaitMain();
            }
            break;
        case State::CrouchWait:  // _UpdateCrouchWait PC 0x623FA0
            if (TestCrouchWaitToWaitMain(world)) {
                SwitchMoveState(State::Wait);
            } else if (TestCrouchWaitToCrouchMoveMain()) {
                ExecCrouchWaitToCrouchMoveMain();
                SwitchMoveState(State::CrouchMove);
            }
            // (high profile -> JumpImpulsion: not in the port, JumpImpulsion needs the free-run system)
            else if (Has180TurnIntention())
                SwitchMoveState(State::CrouchTurn);
            break;
        case State::CrouchMove:  // _UpdateCrouchMove PC 0x61E620
            if (m_IntentionMvtState == 0)
                SwitchMoveState(State::CrouchWait);
            else if (TestCrouchMoveToMoveCycleMain(world))
                SwitchMoveState(State::MoveCycle);
            else if (Has180TurnIntention())
                SwitchMoveState(State::CrouchTurn);
            break;
        case State::CrouchTurn:  // _UpdateCrouchTurn PC 0x5FA510
            if (m_PortAction.HasFinished()) SwitchMoveState(State::CrouchWait);
            break;
        case State::Roll:  // _UpdateRoll PC 0x621D10
            if (TestRollToWaitMain(dt)) {
                SwitchMoveState(State::Wait);
            } else if (TestRollToMoveCycleMain(dt)) {
                // ExecRollToMoveCycleMain (Mac 0xFAE818)
                m_CurrentSpeedRatio = 0.6f;
                TransitionSetupDataToMovement{HumanGroundData::MovementState_Jog, 0.0f, 0.5f, TransitionToMotionType_BlendWalkToJog}.SetupTransitionData(
                    m_Human.GetGroundData());
                SwitchMoveState(State::MoveCycle);
            }
            break;
        default:
            break;
    }

    // PORT: the game moves and turns by the animations' root motion. The port moves along the facing at a stand-in
    // speed (the ratio's in MoveCycle, fading out through MoveStop / RunTurn) and spreads TurnStart's / RunTurn's
    // turn over their stand-in length.
    const float scaledDt = dt * m_PortActionTimeScale;
    switch (m_MoveState) {
        case State::MoveCycle:
            m_PortSpeed = PortSpeedFromRatio();
            break;
        case State::CrouchMove:
            m_PortSpeed = h.GetPortGroundSpeeds().crouchMove;
            break;
        case State::MoveStop:
        case State::RunTurn:
        case State::Roll:
            m_PortSpeed = m_PortEntrySpeed * (1.0f - m_PortAction.Progress());
            break;
        case State::EdgeStop:
            // Live (session 6): still during the unbalance, then backwards at a steady ~0.75 m/s until EdgeStop ends.
            m_PortSpeed = m_PortAction.elapsed < h.GetPortActionLengths().edgeStopUnbalance
                              ? 0.0f
                              : -h.GetPortGroundSpeeds().edgeStopStepBack;
            break;
        default:
            m_PortSpeed = 0;
            break;
    }
    const bool turning = m_MoveState == State::TurnStart || m_MoveState == State::RunTurn ||
                         m_MoveState == State::TurnOnSpot || m_MoveState == State::CrouchTurn;
    if (turning && m_PortAction.duration > 0) {
        float step = m_PortTurnToApply * (scaledDt / m_PortAction.duration);
        h.m_Yaw += step;
    }
    m_PortAction.elapsed += scaledDt;

    // The frame's root motion; Human::Update may clamp it at an edge (EdgeClampingComputation) before it applies.
    h.m_PortRootMotion = forwardFromYaw(h.m_Yaw) * (m_PortSpeed * dt);
}

void HumanGround::UpdateAfterPhysicsPhase(const port::World& world, float dt) {
    // PORT: the character component moves by the (possibly clamped) root motion; then the ground test.
    Human& h = m_Human;
    h.m_Velocity = dt > 0 ? h.m_PortRootMotion * (1.0f / dt) : Vec3{};
    Vec3 next = h.m_Position + h.m_PortRootMotion;
    std::optional<float> ground = world.groundBelow(next.x, next.y, h.m_Position.z + 0.01f);
    // PORT: the character controller's capsule rests on an edge until the centre is a radius past it; the radius
    // stands in with the stand-up capsule's 0.38 (ActorCanPhysicallyStandUp). The game leaves ground it walks off
    // through FallOffEdge (the controller, 0.1 m past the edge) before that.
    if (!ground || *ground < next.z - h.GetPortGroundSpeeds().stepDownHeight) {
        constexpr float r = 0.38f;
        for (const Vec3& o : {Vec3{r, 0, 0}, Vec3{-r, 0, 0}, Vec3{0, r, 0}, Vec3{0, -r, 0}}) {
            std::optional<float> g = world.groundBelow(next.x + o.x, next.y + o.y, h.m_Position.z + 0.01f);
            if (g && *g >= next.z - h.GetPortGroundSpeeds().stepDownHeight && (!ground || *g > *ground)) ground = g;
        }
    }
    h.m_Position = next;
    if (!ground || *ground < next.z - h.GetPortGroundSpeeds().stepDownHeight) {
        // The game leaves the ground through _TransitFromMoveToInAirMain / _TransitFromNavigationToInAirMain etc.
        // (PC 0x5F14C0 / 0x5FA7D0: TransitToContext(InAir) with their Exec). PORT: the port switches when the ground
        // drops away, without their tests and Execs.
        h.TransitToContext(HumanContextId::InAir);
        return;
    }
    h.m_Position.z = *ground;
}

// --- The edge commands (RE/11_ground.md 17) ---

int HumanGround::IsNarrowEdge(const Gap& gap, float maxSquareDistance, bool wide) const {
    if (gap.m_NoEdge || gap.m_Depth <= 1.3f || gap.m_SquareDistance > maxSquareDistance) return 0;
    // PORT: the angle's argument order (sight, normal) is a hypothesis; only its size matters for 1 and 2.
    const Vec3 sight = forwardFromYaw(m_Human.m_Yaw);
    const float a = Gear::GetAngle(sight, Vec3{gap.m_EdgeNormal.x, gap.m_EdgeNormal.y, 0}, Gear::kBaseK);
    const float limit = wide ? 2.0943952f : 1.0471976f;
    if (std::fabs(a) < limit) return 1;
    if (a > -2.0943952f && a <= -1.0471976f) return 3;
    if (a >= 1.0471976f && a < 2.0943952f) return 4;
    return 2;
}

bool HumanGround::TestMoveToEdgeStopOnUnbalanceOnEdge(const Gap& gap, float maxSquareDistance, int& direction,
                                                      bool b) const {
    // Mac 0xFCBF64: no hurt fall (PORT: none), a narrow edge facing out, deeper than 2 m, room to pull down.
    if (IsNarrowEdge(gap, maxSquareDistance, b) != 1) return false;
    if (!(gap.m_Depth > 2.0f)) return false;
    if (!PortTestPullDownToLedgeClearance(gap, true)) return false;
    direction = 1;
    return true;
}

bool HumanGround::CanUnbalanceOnEdge(const Gap& gap, float maxSquareDistance, int& direction, bool b) {
    // Command 87 is offered by Move (to EdgeStop; to the NarrowObject context for directions 3 / 4, which the port
    // leaves out: PORT, no NarrowObject context).
    if (m_MoveState == State::EdgeStop) return false;
    return TestMoveToEdgeStopOnUnbalanceOnEdge(gap, maxSquareDistance, direction, b);
}

void HumanGround::DoUnbalanceOnEdge(const Gap& gap, int direction, bool b) {
    // ExecMoveToEdgeStop...UnbalanceOnEdge (Mac 0xFA0646) only sets the body banking when `b` is set. PORT: runs at
    // once, as DoTurnOnSpot does.
    (void)gap, (void)b;
    if (direction != 1 || m_MoveState == State::EdgeStop) return;
    SwitchMoveState(State::EdgeStop);
}

bool HumanGround::TestEdgeStopToMoveMain() const {
    return m_PortAction.HasFinished();  // Mac 0xF9D2EA: the target action has finished
}

bool HumanGround::TestMoveToLedgeOnLedgePullDown(const Gap& gap) const {
    // Mac 0xFCBB42. PORT: no action items, so the current item's flag 0x20 is never set.
    if (gap.m_NoEdge) return false;
    const Vec3 sight = forwardFromYaw(m_Human.m_Yaw);
    if (dot(gap.m_EdgeNormal, sight) <= 0.0f) return false;
    if (gap.m_Depth <= 2.0f) return false;
    return PortTestPullDownToLedgeClearance(gap, false);
}

bool HumanGround::TestEdgeStopToLedgeOnLedgePullDown(const Gap& gap) const {
    // Mac 0xFCB974: once the unbalance action (115916159) has finished, the same checks. PORT: the stand-in action
    // covers the whole EdgeStop; the unbalance is its first part.
    if (m_PortAction.elapsed < m_Human.GetPortActionLengths().edgeStopUnbalance) return false;
    return TestMoveToLedgeOnLedgePullDown(gap);
}

bool HumanGround::CanPullDownOnEdge(const Gap& gap) {
    return m_MoveState == State::EdgeStop ? TestEdgeStopToLedgeOnLedgePullDown(gap) : TestMoveToLedgeOnLedgePullDown(gap);
}

void HumanGround::DoPullDownOnEdge(const Gap& gap) {
    // _TransitFrom{Move,EdgeStop}ToLedge...LedgePullDown (Mac 0xF99D34 / 0xF9A45A): TransitToContext(Ledge) at once;
    // the Exec (Mac 0xF98DA6 / 0xF9B884) copies the gap into the Ledge data. PORT: the port has no Ledge context yet,
    // so TransitToContext does nothing and the Human stays on the ground.
    if (!CanPullDownOnEdge(gap)) return;
    m_Human.TransitToContext(HumanContextId::Ledge);
}

bool HumanGround::TestIsEdgeValidForFall(const Gap& gap) const {
    // Mac 0xFCAB92: an edge, no hurt fall (PORT: none), already past the edge, room to fall.
    if (gap.m_NoEdge) return false;
    return gap.m_SquareDistance < -0.01f && PortTestFallOffEdgeClearance(gap);
}

bool HumanGround::TestGroundToInAirOnFallOffEdge(const Gap& gap, bool force) const {
    // Mac 0xFCB406. PORT: its timer, +0x455 bit 0, the bumping no-fall zone and the ragdoll aren't ported (they
    // never block).
    if (force) return true;
    return gap.m_Depth >= 0.53f && TestIsEdgeValidForFall(gap);
}

bool HumanGround::CanFallOffEdge(const Gap& gap, bool force, bool b) {
    (void)b;
    return TestGroundToInAirOnFallOffEdge(gap, force);
}

void HumanGround::DoFallOffEdge(const Gap& gap, float value) {
    // _TransitFromHumanGroundToInAirOnHumanGroundInterfaceCommand_FallOffEdge (PC 0x60BA30): TransitToContext(InAir)
    // at once, with the Exec PC 0x6056F0 -> ExecuteFallOffEdge PC 0x5FC890 (the gap and the float: high profile 1,
    // else 0). PORT: the Exec isn't decoded.
    (void)value;
    if (!CanFallOffEdge(gap, false, false)) return;
    m_Human.TransitToContext(HumanContextId::InAir);
}

bool HumanGround::CanEnterLookDown() {
    // Command 136 is offered by Wait's Idle (Mac 0xF9832A) while the UpperBody context (HumanGround +0x790, Mac
    // +0x770; RE/10_human_update.md 1) has 1 at +0x24. PORT: no UpperBody context; taken as 1.
    return m_MoveState == State::Wait && m_WaitState == State::Idle;
}

void HumanGround::DoEnterLookDown(const Gap& gap) {
    // Mac 0xFEB656 (after an unidentified Human vfunc +0x4C(50) -> +0x30(0)); ExecIdleToLookDown (Mac 0xF98D8A) keeps
    // the gap; EnterLookDown (Mac 0xFD1248) plays the look-down animation. PORT: runs at once, no animation. LookDown
    // goes back to Idle when the upper body's +0x24 isn't 1 (never in the port), or ends when Wait is left.
    if (!CanEnterLookDown()) return;
    m_LookDownGap = gap;
    m_WaitState = State::LookDown;
}

}  // namespace scimitar::ac2
