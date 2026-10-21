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
    data.m_MovementMode = 0;
    data.m_MoveDirection = {};
    data.m_StickMagnitude = 0;
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
    // PORT: the game enters the FSM's initial states (Vulnerable > Navigation > Move > Wait, from LinkFSM's initial
    // children); the port only tracks Move's child.
    m_MoveState = State::Wait;
}

void HumanGround::UpdateMovementIntention() {
    const HumanGroundData& data = m_Human.GetGroundData();
    if (m_AISpeedOverride) {
        m_MovementDivision = m_AISpeedRatio > 0.0f ? 2 : 0;
    } else {
        int division = data.GetMovementDivisionFromSpeedRatio(data.m_HighProfileRequest, data.m_StickMagnitude);
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
    const Vec3& move = m_Human.GetGroundData().m_MoveDirection;
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
    Vec3 move{data.m_MoveDirection.x, data.m_MoveDirection.y, 0};
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
    float target = ComputeTargetSpeedRatio(data.m_StickMagnitude, data.m_Sprint,
                                           m_HighProfileIntention != 0, m_AISpeedOverride != 0,
                                           m_AISpeedRatio);
    // PORT: no action system yet, so this is always the game's "move cycle is playing" branch; the restriction mode
    // (RestrictTargetSpeedRatio) only applies outside it. The port's Human is the player, so slowing down follows the
    // curve. LIVE row 4: the game's step is the float at entity +0x6C -> +0x5D8, taken to be dt.
    const float curve = m_AISpeedOverride ? 1.0f : m_SlowDownCurve.GetOutput(m_SpeedRatio);
    m_SpeedRatio = StepSpeedRatio(m_SpeedRatio, target, dt, curve);
    GaitFromSpeedRatio(m_SpeedRatio, m_GaitLevel, m_Running);
}

// --- IHumanGround setters that write HumanGroundData ---

void HumanGround::SetBreakHard(bool v) { m_Human.GetGroundData().m_BreakHard = v; }
void HumanGround::SetSprint(bool v) { m_Human.GetGroundData().m_Sprint = v; }
void HumanGround::SetCrouch(bool v) { m_Human.GetGroundData().m_Crouch = v; }

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
    m_GaitLevel = m_MovementDivision;
    m_Running = m_HighProfileIntention;
    m_Human.GetGroundData().m_TurnAngle = 0;
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

void HumanGround::SwitchMoveState(State next) {
    m_MoveState = next;
    m_PortActionTimeScale = 1;
    switch (next) {
        case State::MoveCycle: EnterMoveCycle(); break;
        case State::TurnStart: EnterTurnStart(); break;
        case State::MoveStop: EnterMoveStop(); break;
        case State::RunTurn: EnterRunTurn(); break;
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
        case State::Wait:  // _UpdateWait PC 0x621A80 (TurnOnSpot and CrouchWait not in the port yet)
            if (TestWaitToTurnStartMain())
                SwitchMoveState(State::TurnStart);
            else if (TestWaitToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            break;
        case State::MoveCycle:  // _UpdateMoveCycle PC 0x621B90 (CrouchMove not in the port yet)
            UpdateMoveCycleMain(dt);
            if (TestMoveCycleToMoveStopMain())
                SwitchMoveState(State::MoveStop);
            else if (TestMoveCycleToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestMoveCycleToTurnStartMain())
                SwitchMoveState(State::TurnStart);
            break;
        case State::MoveStop:  // _UpdateMoveStop PC 0x621C80
            if (TestMoveStopToRunTurnMain(dt))
                SwitchMoveState(State::RunTurn);
            else if (TestMoveStopToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestMoveStopToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
            break;
        case State::TurnStart: {  // _UpdateTurnStart PC 0x621EE0
            UpdateTurnStartMain();
            const float scaled = dt * m_PortActionTimeScale;
            if (TestTurnStartToWaitMain(scaled))
                SwitchMoveState(State::Wait);
            else if (TestTurnStartToMoveCycleMain(scaled))
                SwitchMoveState(State::MoveCycle);
            break;
        }
        case State::RunTurn:  // _UpdateRunTurn PC 0x621E60
            if (TestRunTurnToWaitMain())
                SwitchMoveState(State::Wait);
            else if (TestRunTurnToMoveCycleMain())
                SwitchMoveState(State::MoveCycle);
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
        case State::MoveStop:
        case State::RunTurn:
            m_PortSpeed = m_PortEntrySpeed * (1.0f - m_PortAction.Progress());
            break;
        default:
            m_PortSpeed = 0;
            break;
    }
    if ((m_MoveState == State::TurnStart || m_MoveState == State::RunTurn) && m_PortAction.duration > 0) {
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
