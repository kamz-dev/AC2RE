#include "scimitar/ac2/human_in_air.h"

#include <cmath>

#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

namespace scimitar::ac2 {

void HumanInAir::Enter() {
    // PORT: the jump / fall start (HumanInAirData::JumpOrigin) and the landing speed ratio are set by code the port
    // doesn't have yet (the jumps; IHumanInAir::SetLandingSpeedRatio's caller, LIVE row 10). Stand-ins: the position
    // and HumanGround's speed ratio when the human leaves the ground.
    m_Human.GetInAirData().m_JumpOrigin = m_Human.m_Position;
    SetLandingSpeedRatio(m_Human.GetContextBundle().GetGround().GetSpeedRatio());
    m_PortEntered = true;
}

void HumanInAir::Update(const port::MoveInput&, const port::World& world, float dt) {
    Human& h = m_Human;
    if (!m_PortEntered) Enter();
    // PORT: plain ballistic fall with no air control and no catches.
    h.m_Velocity.z -= h.GetInAirData().gravity * dt;
    Vec3 next = h.m_Position + h.m_Velocity * dt;
    std::optional<float> ground = world.groundBelow(next.x, next.y, h.m_Position.z);
    if (ground && next.z <= *ground) {
        next.z = *ground;
        h.m_Position = next;
        const Vec3 velocity = h.m_Velocity;
        h.m_Velocity = {};
        ExecMainToGroundMain(velocity);
        m_PortEntered = false;
        h.RequestContext(HumanContextId::Ground);
        return;
    }
    h.m_Position = next;
}

void HumanInAir::ExecMainToGroundMain(const Vec3& velocity) {
    // PC 0x69B730, Mac 0x1236156 (RE/11_ground.md 11). The air assassination / air tackle exits (only HumanGroundData's
    // EntryState := 2 / 10) and the sounds, FX, witness events and fall damage aren't mirrored.
    Human& h = m_Human;
    const float drop = h.GetInAirData().m_JumpOrigin.z - h.m_Position.z;
    // The landing speed band: 0 below 0.2, 1 below 0.5, 2 below 0.9, else 3.
    int band = 0;
    if (m_LandingSpeedRatio >= 0.2f) band = m_LandingSpeedRatio >= 0.5f ? (m_LandingSpeedRatio >= 0.9f ? 3 : 2) : 1;
    const int frontFoot = 1;  // PORT: Human::GetFrontFoot (PC 0x9183D0) needs the skeleton

    // PORT: the spring-jump branch (current action 0xDA3ACAAF) isn't ported; nor is ComputeFallDamage (PC 0x6927A0).
    if (drop > 3.0f) {
        h.m_PortCurrentActionId = m_LandingSpeedRatio <= 0.2f ? landing_action::Hard : landing_action::HardRoll;
        TransitionSetupDataToMovement{HumanGroundData::MovementState_LandingHard, 0.0f, 0.5f,
                                      TransitionToMotionType_BlendWalkToJog}
            .SetupTransitionData(h.GetGroundData());
        return;
    }
    // A steep fall: no horizontal speed, or more than 75 degrees between it and the velocity (z clamped to <= 0).
    const Vec3 horizontal{velocity.x, velocity.y, 0};
    const Vec3 descent{velocity.x, velocity.y, velocity.z > 0 ? 0 : velocity.z};
    bool steep = std::fabs(horizontal.x) <= 0.0005f && std::fabs(horizontal.y) <= 0.0005f;
    if (!steep) {
        const float cosAngle = dot(horizontal, descent) / (length(horizontal) * length(descent));
        steep = std::acos(std::fmin(1.0f, std::fmax(-1.0f, cosAngle))) > 1.3089969f;
    }
    if (steep)
        h.m_PortCurrentActionId = band ? landing_action::MovingVertical : landing_action::StandingVertical;
    else if (band)
        h.m_PortCurrentActionId = landing_action::MovingRightFoot + (frontFoot != 1);
    else
        h.m_PortCurrentActionId = frontFoot != 1 ? landing_action::StandingLeftFoot : landing_action::StandingRightFoot;
    TransitionSetupDataToMovement{HumanGroundData::MovementState_Landing, 0.0f, 0.5f,
                                  TransitionToMotionType_BlendWalkToJog}
        .SetupTransitionData(h.GetGroundData());
}

}  // namespace scimitar::ac2
