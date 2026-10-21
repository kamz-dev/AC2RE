#include "scimitar/ac2/human_ground.h"

#include <cmath>

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace scimitar::ac2 {

// --- UpdateMoveCycleMain's rules (PC 0x61BB60, Mac 0xFCD20A; RE/11_ground.md 5) ---

float HumanGround::ComputeTargetSpeedRatio(float stickMagnitude, bool speedBandUp, bool highProfile, bool aiOverride,
                                           float aiSpeedRatio) {
    float target;
    if (aiOverride) {
        target = aiSpeedRatio;  // +0x7C0 when +0x8A8 is set
    } else if (highProfile) {
        target = stickMagnitude * 0.2f + (speedBandUp ? 0.8f : 0.6f);
    } else {
        target = stickMagnitude * 0.2f + (speedBandUp ? 0.2f : 0.0f);
    }
    return target > 1.0f ? 1.0f : target;
}

float HumanGround::RestrictTargetSpeedRatio(float target, int restrictionMode, bool isPlayer, bool speedBandUp,
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
            if (speedBandUp) cap = 1.0f;
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

// --- The context ---

void HumanGround::Enter() {
    // PORT: the game enters the FSM's initial states (Vulnerable > Navigation > Move > Wait, from LinkFSM's initial
    // children); the port only tracks Move's child.
    m_MoveState = State::Wait;
}

void HumanGround::UpdateMovementIntention() {
    // Mac 0xF98C54: the high-profile intention mirrors HumanGroundData +0x181. (It also sets +0x78C [Mac] from
    // HumanGroundData::GetMovementDivisionFromSpeedRatio; not decoded, not used by the port yet.)
    m_HighProfileIntention = m_Human.GetGroundData().m_HighProfileRequest ? 1 : 0;
}

void HumanGround::UpdateMoveCycleMain(float dt) {
    const HumanGroundData& data = m_Human.GetGroundData();
    float target = ComputeTargetSpeedRatio(data.m_StickMagnitude, data.m_SpeedBandUp,
                                           m_HighProfileIntention != 0, m_AISpeedOverride != 0,
                                           m_AISpeedRatio);
    // PORT: no action system yet, so this is always the game's "move cycle is playing" branch: the restriction mode
    // (RestrictTargetSpeedRatio) only applies outside it, and the slow-down ResponseCurve isn't decoded (1).
    // LIVE row 4: the game's step is the float at entity +0x6C -> +0x5D8, taken to be dt.
    m_SpeedRatio = StepSpeedRatio(m_SpeedRatio, target, dt, 1.0f);
    GaitFromSpeedRatio(m_SpeedRatio, m_GaitLevel, m_Running);
}

bool HumanGround::TestWaitToMoveCycleMain() const {
    // PORT: condition not decoded (Mac 0xF9EAE6); any stick input starts the cycle.
    return m_Human.GetGroundData().m_StickMagnitude > 0.0f;
}

bool HumanGround::TestMoveCycleToWaitMain() const {
    // PORT: condition not decoded (PC 0x5EAB90); back to Wait once the stick is released and the ratio is down.
    return m_Human.GetGroundData().m_StickMagnitude <= 0.0f && m_SpeedRatio <= 0.0005f;
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
    const HumanGroundData& data = h.GetGroundData();

    UpdateMovementIntention();

    // Move's children, as _UpdateMove (PC 0x634D20) dispatches them; the port has Wait and MoveCycle so far.
    switch (m_MoveState) {
        case State::Wait:  // _UpdateWait PC 0x621A80: transitions only, no main update
            if (TestWaitToMoveCycleMain()) m_MoveState = State::MoveCycle;
            break;
        case State::MoveCycle:  // _UpdateMoveCycle PC 0x621B90: main update, then the transitions
            UpdateMoveCycleMain(dt);
            if (TestMoveCycleToWaitMain()) m_MoveState = State::Wait;
            break;
        default:
            break;
    }

    // PORT: the game moves by the blended animations' root motion and turns through UpdateTurnAngle /
    // UpdateOrientation (not decoded); the port moves at a stand-in speed for the ratio and turns instantly.
    float speed = m_MoveState == State::MoveCycle ? PortSpeedFromRatio() : 0.0f;
    if (data.m_StickMagnitude > 0.0f) {
        Vec3 dir = normalized(Vec3{data.m_MoveDirection.x, data.m_MoveDirection.y, 0});
        if (dir.x != 0.0f || dir.y != 0.0f) m_LastMoveDirection = dir;
    }
    h.m_Velocity = m_LastMoveDirection * speed;
    if (speed > 0) h.m_Yaw = yawFromDirection(m_LastMoveDirection);

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
