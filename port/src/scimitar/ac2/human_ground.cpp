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

float HumanGround::RestrictTargetSpeedRatio(float target, int restrictionMode, bool isPlayer, bool sprint,
                                            bool highProfile, float currentRatio) {
    float t = target;
    float cap = 0.6f;
    if (!isPlayer) {  // entity type (entity +0xA8 & 7) != 1
        if (t <= 0.2f) t = 0.2f;
        if (t >= 0.6f) t = 0.6f;
    }
    switch (restrictionMode) {  // +0x994
        case 0:
            t = cap;
            break;
        case 1:
        case 4:
            if (t < 0.2f)
                t = 0.2f;
            else if (cap < t)
                t = cap;
            break;
        case 2:
            t = currentRatio < 0.6f ? 0.2f : cap;
            break;
        case 3:
            if (t <= 0.0f) t = 0.0f;
            if (t >= 0.6f) t = cap;
            break;
        case 5:
        case 6:
            if (t < 0.2f) t = 0.2f;
            [[fallthrough]];
        case 7:
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
        data.m_MovementDivisionCount[h] = 3;
        data.m_MovementDivisionThresholds[h][0] = 0.0f;
        data.m_MovementDivisionThresholds[h][1] = 0.9f;
        data.m_MovementDivisionThresholds[h][2] = 1.0f;
    }
    data.m_HighProfileRequest = false;
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
        m_Running = running;
        m_GaitLevel = gait;
        m_SpeedRatio = ratio;
    };
    switch (data.m_MovementState) {
        case movement_state::Default: first = State::Wait; seed(0, 0, 0.0f); break;  // (+0x224 := 1, not mirrored)
        case movement_state::HandPassOverReception: first = State::HandPassOverReception; seed(1, 2, 0.6f); break;
        case movement_state::Landing: first = State::Landing; seed(0, 0, 0.0f); break;
        case movement_state::LandingHard: first = State::LandingHard; seed(0, 0, 0.0f); break;
        case movement_state::LandingBalanceFall: first = State::LandingBalanceFall; seed(1, 0, 0.0f); break;
        case movement_state::WaitLow: first = State::Wait; seed(0, 0, 0.0f); break;
        case movement_state::WaitHigh: first = State::Wait; seed(1, 0, 0.0f); break;
        case movement_state::WalkVerySlow: first = State::MoveCycle; seed(0, 1, 0.0f); break;
        case movement_state::Walk: first = State::MoveCycle; seed(0, 2, 0.2f); break;
        case movement_state::Jog:
        case movement_state::Run: first = State::MoveCycle; seed(1, 2, 0.6f); break;
        case movement_state::Sprint:
            first = State::MoveCycle;
            seed(1, 2, 1.0f);
            if (data.m_TransitionToMotionType == 1) data.m_TransitionToMotionType = 5;
            break;
        case movement_state::Crouch: first = State::CrouchWait; seed(0, 0, 0.0f); break;
        default: break;
    }
    m_HighProfileIntention = m_Running;
    m_MovementDivision = m_GaitLevel;
    SwitchMoveState(first);
}

void HumanGround::UpdateMovementIntention() {
    const HumanGroundData& data = m_Human.GetGroundData();
    if (m_AISpeedOverride) {
        m_MovementDivision = m_AISpeedRatio > 0.0f ? 2 : 0;
    } else {
        int division = data.GetMovementDivisionFromSpeedRatio(data.m_HighProfileRequest, data.m_DestSpeedRatio);
        if (division == 0)
            m_MovementDivision = 0;
        else if (division == 1 || division == 2)
            m_MovementDivision = 2;
    }
    m_HighProfileIntention = data.m_HighProfileRequest ? 1 : 0;
}

void HumanGround::UpdateTurnAngle() {
    // The signed angle about +Z between the desired move direction and the sight. PORT: the sign convention of
    // Gear::GetAngle isn't checked (taken as sight to move direction, counter-clockwise positive).
    const Vec3& move = m_Human.GetGroundData().m_DestHeading;
    if (NullWithEpsilon(Vec3{move.x, move.y, 0}, 0.0005f)) {
        m_TurnAngle = 0;
        return;
    }
    m_TurnAngle = WrapAngle(yawFromDirection(move) - m_Human.m_Yaw);
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
    if (m_GaitLevel) UpdateOrientation(dt);
}

void HumanGround::UpdateMoveCycleMain(float dt) {
    const HumanGroundData& data = m_Human.GetGroundData();
    float target = ComputeTargetSpeedRatio(data.m_DestSpeedRatio, data.m_Sprint,
                                           m_HighProfileIntention != 0, m_AISpeedOverride != 0,
                                           m_AISpeedRatio);
    // PC 0x61BD2D: while the current action isn't the move cycle (93469659) and won't finish this frame, the target is
    // restricted (mode +0x994, then <= 0.2 without high profile), the ratio steps by plain dt, and the bands / gait
    // aren't updated (the branch returns). The step is the frame dt (RE/20 row 4).
    if (!PortIsMoveCycleActionCurrent(dt)) {
        const float t = RestrictTargetSpeedRatio(target, m_SpeedRestriction, /*isPlayer=*/true, data.m_Sprint,
                                                 m_HighProfileIntention != 0, m_SpeedRatio);
        m_SpeedRatio = StepSpeedRatio(m_SpeedRatio, t, dt, 1.0f);
        // (UpdateMoveDefaultBlendFactors(1, 1): animation layer)
        if (m_MoveCycleEntryFlags & 1) m_PortActionTimeScale = 2.0f - m_PortAction.Progress();
        return;
    }
    // The move cycle is current. The port's Human is the player; slowing down follows the curve unless the AI
    // override is on.
    const float curve = m_AISpeedOverride ? 1.0f : m_SlowDownCurve.GetOutput(m_SpeedRatio);
    m_SpeedRatio = StepSpeedRatio(m_SpeedRatio, target, dt, curve);
    GaitFromSpeedRatio(m_SpeedRatio, m_GaitLevel, m_Running);
}

// --- IHumanGround setters that write HumanGroundData ---

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

bool HumanGround::ActorCanPhysicallyStandUp() const {
    // PC 0x5F72C0: a 0.38 m capsule test from 0.42 m to 1.42 m above the feet. PORT: the port has no collision.
    return true;
}

bool HumanGround::TestWaitToTurnOnSpotMain() const {
    // PORT: the custom-action orientation check is left out (no custom actions).
    return m_TurnMode == 1 && PortIsActionInterruptibleByMovement() && m_MovementDivision == 0 &&
           std::fabs(m_TurnAngle) >= 0.017453292f;
}

bool HumanGround::TestWaitToCrouchWaitMain() const {
    return m_Human.GetGroundData().m_Crouch && m_HighProfileIntention == 0 && PortIsActionInterruptibleByMovement();
}

bool HumanGround::TestMoveCycleToCrouchMoveMain() const {
    return m_Human.GetGroundData().m_Crouch && m_Running == 0;
}

bool HumanGround::TestCrouchWaitToWaitMain() const {
    return !m_Human.GetGroundData().m_Crouch && PortIsActionInterruptibleByMovement() && ActorCanPhysicallyStandUp();
}

bool HumanGround::TestCrouchWaitToCrouchMoveMain() const {
    // PORT: the transition-animation check behind the flag at Mac +0x8B5 is left out.
    return m_MovementDivision != 0;
}

bool HumanGround::TestCrouchMoveToMoveCycleMain() const {
    return (!m_Human.GetGroundData().m_Crouch || m_HighProfileIntention == 1) && ActorCanPhysicallyStandUp();
}

// --- Locomotion transitions (RE/11_ground.md 6 and 10) ---
// PORT: without an action system, "the current action can be cut by movement" (IsActionInterruptibleByMovement) is
// never true; WillActionFinish / HasActionFinished read the stand-in action; in MoveCycle the current action is
// always the move cycle; and Wait plays no action, so it is always free.

bool HumanGround::Has180TurnIntention() const {
    return m_ForceTurnAround || std::fabs(m_TurnAngle) > 2.0943952f;  // 2pi/3
}

bool HumanGround::TestWaitToTurnStartMain() const {
    return std::fabs(m_TurnAngle) > m_TurnOnMovementMinAngle && !m_BlockTurnStartFromWait;
}

bool HumanGround::TestWaitToMoveCycleMain() const {
    return m_MovementDivision == 1 || m_MovementDivision == 2;
}

bool HumanGround::TestMoveCycleToMoveStopMain() const {
    return m_Running == 1 && m_GaitLevel != 0 && m_Human.GetGroundData().m_BreakHard;
}

bool HumanGround::TestMoveCycleToWaitMain() const {
    return m_MovementDivision == 0 && m_Running == 0;
}

bool HumanGround::TestMoveCycleToTurnStartMain() const {
    return m_Running == 0 && std::fabs(m_TurnAngle) > m_TurnOnMovementMinAngle;
}

bool HumanGround::TestMoveStopToRunTurnMain(float dt) const {
    return Has180TurnIntention() && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestMoveStopToWaitMain() const {
    return m_PortAction.HasFinished() && m_MovementDivision == 0;
}

bool HumanGround::TestMoveStopToMoveCycleMain() const {
    return m_PortAction.HasFinished() && (m_MovementDivision == 1 || m_MovementDivision == 2);
}

bool HumanGround::TestTurnStartToWaitMain(float dt) const {
    return m_MovementDivision == 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestTurnStartToMoveCycleMain(float dt) const {
    return m_MovementDivision != 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestRunTurnToWaitMain() const {
    return m_MovementDivision == 0 && m_PortAction.HasFinished();
}

bool HumanGround::TestRunTurnToMoveCycleMain() const {
    return m_MovementDivision != 0 && (m_PortAction.HasFinished() || !Has180TurnIntention());
}

// --- Entering the states ---

void HumanGround::EnterMoveCycle() {
    // Mac 0xFBE706: the gait level takes the movement division and running takes the high-profile intention; the
    // blend weights and HumanGroundData's turn angle are reset. (The action set-up is left to the animation layer.)
    HumanGroundData& data = m_Human.GetGroundData();
    m_GaitLevel = m_MovementDivision;
    m_Running = m_HighProfileIntention;
    m_SpeedRestriction = data.m_TransitionToMotionType;  // this[605] = HumanGroundData +0x228
    TransitionSetupDataToMovement::ResetTransitionData(data);
    data.m_CurrentBodyAngle = 0;
}

void HumanGround::ExecTurnStartToWaitMain() {
    // PC 0x614F40, Mac 0xFB664C. The wait action (g_GroundWaitActionMap[12 * side + 6 * running + 3 * foot]) and
    // its blend by the turn angle are left to the animation layer. Live (RE/20 row 8): the ratio is 0 afterwards.
    m_HighProfileIntention = m_Running;
    m_SpeedRatio = 0;
}

void HumanGround::ExecTurnStartToMoveCycleMain() {
    // PC 0x615120, Mac 0xFB7474: seeds the speed ratio and TransitionToMotionType, which EnterMoveCycle then copies
    // into the speed restriction. The move cycle action (0x5923BDB) and its blend factors are left to the animation
    // layer. PORT: the +0x998 |= 1 of the high-profile branch isn't mirrored (the field's use isn't decoded).
    HumanGroundData& data = m_Human.GetGroundData();
    const bool isPlayer = true;  // entity type (entity +0xA8 & 7) == 1; the port's Human is the player
    if (m_Running) {
        m_SpeedRatio = 0.6f;
        data.m_TransitionToMotionType = 2;
    } else {
        if (m_HighProfileIntention) {
            m_MoveCycleEntryFlags |= 1;
            m_SpeedRatio = 0.6f;
        } else if (isPlayer && m_MovementDivision == 1)
            m_SpeedRatio = 0;
        else
            m_SpeedRatio = 0.2f;
        data.m_TransitionToMotionType = 3;
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
    m_MoveCycleEntryFlags &= ~1;
}

void HumanGround::ExecMoveStopToWaitMain() {
    // Mac 0xF9748A, inlined in PC _TransitFromMoveStopToWaitMain 0x61F200. Live (RE/20 row 8): ratio 0 afterwards,
    // +0x7B4 at 1 for one frame (EnterWait's children reset it).
    m_HighProfileIntention = 1;
    m_SpeedRatio = 0;
}

void HumanGround::EnterTurnStart() {
    // PC 0x614BB0. The action is g_TurnStartActionMap[6 * side + 3 * running + foot] (big turns) or a 3-way blend
    // (small turns); only the field changes are mirrored here.
    const float a = m_TurnAngle;
    if (std::fabs(a) >= m_TurnOnMovementMinAngle) {
        m_TurnStartSide = a >= 0.0f ? 1 : 0;
        m_HighProfileIntention = m_Running;
    } else {
        m_TurnStartSide = a >= 0.0f ? 1 : 0;
    }
    m_TurnStartAngle = a;
    m_GaitLevel = 0;
    m_Running = m_HighProfileIntention;
    // PORT: the turn animation's length and rotation.
    m_PortAction.Start(m_Human.GetPortActionLengths().turnStart);
    m_PortTurnToApply = a;
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
    m_PortTurnToApply = m_TurnAngle;
}

// --- Landings and Roll (RE/11_ground.md 11). PORT: the landing actions are chosen by InAir in the game; the
// port's "standing landing" (the three ids TestLandingToWaitMain accepts) is a landing with no movement intended. ---

bool HumanGround::TestLandingToWaitMain() const {
    return m_PortAction.HasFinished() && m_MovementDivision == 0;
}

bool HumanGround::TestLandingToMoveCycleMain() const {
    return m_PortAction.HasFinished() && m_MovementDivision != 0;
}

void HumanGround::ExecLandingToMoveCycleMain() {
    HumanGroundData& data = m_Human.GetGroundData();
    int state;
    if (m_HighProfileIntention) {
        m_SpeedRatio = data.m_Sprint ? 1.0f : 0.6f;
        state = data.m_Sprint ? movement_state::Sprint : movement_state::Jog;
        m_HighProfileIntention = 1;
    } else {
        m_SpeedRatio = 0.2f;
        state = movement_state::Walk;
        m_HighProfileIntention = 0;
    }
    m_MovementDivision = 2;
    TransitionSetupDataToMovement{state, 0.0f, 0.5f, 1}.SetupTransitionData(data);
    data.m_TransitionToMotionType = 6;
}

bool HumanGround::TestLandingHardToWaitMain(float dt) const {
    return m_MovementDivision == 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestLandingHardToMoveCycleMain(float dt) const {
    return m_MovementDivision != 0 && m_PortAction.WillFinish(dt);
}

bool HumanGround::TestLandingBalanceFallToWaitMain(float dt) const {
    return m_PortAction.WillFinish(dt);
}

bool HumanGround::TestRollToWaitMain(float dt) const {
    return m_PortAction.WillFinish(dt) && m_MovementDivision == 0;
}

bool HumanGround::TestRollToMoveCycleMain(float dt) const {
    return m_PortAction.WillFinish(dt) && m_MovementDivision != 0;
}

void HumanGround::SwitchMoveState(State next) {
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
            m_Running = 0;
            m_GaitLevel = 0;
            break;
        case State::CrouchMove:  // Mac 0xFC3338: capsule height 1.0, running 0, gait 1
            m_Running = 0;
            m_GaitLevel = 1;
            break;
        case State::CrouchTurn:  // the game turns by the character controller's displacement (progression ratio)
            m_PortAction.Start(m_Human.GetPortActionLengths().crouchTurn);
            m_PortTurnToApply = m_TurnAngle;
            break;
        case State::TurnOnSpot: {  // StartTurnOnSpotAnim PC 0x606B70: one turn animation towards the target
            const int anim = GetTurnAnimation(m_TurnAngle);
            m_PortAction.Start(m_Human.GetPortActionLengths().turnOnSpot[anim / 2]);
            m_PortTurnToApply = m_TurnAngle;
            break;
        }
        default: break;
    }
}

void HumanGround::UpdateTurnStartMain() {
    // PC 0x5EB6F0: twice the speed in high profile, unless the action is one of the running turns (the
    // g_TurnStartActionMap entries with running = 1).
    const bool runningTurn = std::fabs(m_TurnStartAngle) >= m_TurnOnMovementMinAngle && m_Running == 1;
    m_PortActionTimeScale = (m_HighProfileIntention == 1 && !runningTurn) ? 2.0f : 1.0f;
}

float HumanGround::PortSpeedFromRatio() const {
    const float* s = m_Human.GetPortGroundSpeeds().atRatio;
    float x = m_SpeedRatio / 0.2f;
    int i = static_cast<int>(x);
    if (i >= 5) return s[5];
    if (i < 0) return s[0];
    float f = x - static_cast<float>(i);
    return s[i] + (s[i + 1] - s[i]) * f;
}

void HumanGround::Update(const port::MoveInput&, const port::World& world, float dt) {
    Human& h = m_Human;

    // _UpdateMove (PC 0x634D20): UpdateMoveMain, then the current child's update and its transitions, in the
    // game's order.
    UpdateMoveMain(dt);
    switch (m_MoveState) {
        case State::Wait:  // _UpdateWait PC 0x621A80
            if (TestWaitToTurnStartMain())
                SwitchMoveState(State::TurnStart);
            else if (TestWaitToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            else if (TestWaitToTurnOnSpotMain())
                SwitchMoveState(State::TurnOnSpot);
            else if (TestWaitToCrouchWaitMain())
                SwitchMoveState(State::CrouchWait);
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
                m_SpeedRatio = m_HighProfileIntention ? 0.6f : 0.2f;
                TransitionSetupDataToMovement{m_HighProfileIntention ? movement_state::Jog : movement_state::Walk,
                                              0.0f, 0.5f, 1}
                    .SetupTransitionData(m_Human.GetGroundData());
                SwitchMoveState(State::MoveCycle);
            }
            break;
        case State::LandingBalanceFall:  // _UpdateLandingBalanceFall PC 0x630940
            if (TestLandingBalanceFallToWaitMain(dt)) SwitchMoveState(State::Wait);
            break;
        case State::TurnOnSpot:  // TurnOnSpotStep (PORT: one step; the game chains steps while its cycle flag is set)
            if (m_PortAction.HasFinished()) SwitchMoveState(State::Wait);
            break;
        case State::CrouchWait:  // _UpdateCrouchWait PC 0x623FA0
            if (TestCrouchWaitToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestCrouchWaitToCrouchMoveMain())
                SwitchMoveState(State::CrouchMove);
            // (high profile -> JumpImpulsion: not in the port, JumpImpulsion needs the free-run system)
            else if (Has180TurnIntention())
                SwitchMoveState(State::CrouchTurn);
            break;
        case State::CrouchMove:  // _UpdateCrouchMove PC 0x61E620
            if (m_MovementDivision == 0)
                SwitchMoveState(State::CrouchWait);
            else if (TestCrouchMoveToMoveCycleMain())
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
                m_SpeedRatio = 0.6f;
                TransitionSetupDataToMovement{movement_state::Jog, 0.0f, 0.5f, 1}.SetupTransitionData(
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

    h.m_Velocity = forwardFromYaw(h.m_Yaw) * m_PortSpeed;
    Vec3 next = h.m_Position + h.m_Velocity * dt;
    std::optional<float> ground = world.groundBelow(next.x, next.y, h.m_Position.z + 0.01f);
    h.m_Position = next;
    if (!ground || *ground < next.z - h.GetPortGroundSpeeds().stepDownHeight) {
        // The game leaves the ground through _TransitFromMoveToInAirMain / _TransitFromNavigationToInAirMain etc.
        // PORT: the port switches directly when the ground drops away.
        h.RequestContext(HumanContextId::InAir);
        return;
    }
    h.m_Position.z = *ground;
}

}  // namespace scimitar::ac2
