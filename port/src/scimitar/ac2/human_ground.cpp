#include "scimitar/ac2/human_ground.h"

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace scimitar::ac2 {

void HumanGround::Update(const port::MoveInput& input, const port::World& world, float dt) {
    Human& h = m_Human;
    const HumanGroundData& data = h.GetGroundData();

    // PORT: placeholder gait choice with instant acceleration and turning.
    float speed = 0;
    if (input.moveMag > 0) {
        if (input.held(port::kHighProfile))
            speed = data.sprintSpeed;
        else
            speed = input.moveMag > 0.5f ? data.runSpeed : data.walkSpeed;
    }
    Vec3 dir = normalized(Vec3{input.moveDir.x, input.moveDir.y, 0});
    h.m_Velocity = dir * speed;
    if (speed > 0) h.m_Yaw = yawFromDirection(dir);

    Vec3 next = h.m_Position + h.m_Velocity * dt;
    std::optional<float> ground = world.groundBelow(next.x, next.y, h.m_Position.z + 0.01f);
    h.m_Position = next;
    if (!ground || *ground < next.z - data.stepDownHeight) {
        // The game leaves the ground through an interface command (Mac: ...InterfaceCommand_FallOffEdge).
        // PORT: the port switches directly.
        h.RequestContext(HumanContextId::InAir);
        return;
    }
    h.m_Position.z = *ground;
}

}  // namespace scimitar::ac2
