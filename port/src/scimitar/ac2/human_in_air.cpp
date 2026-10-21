#include "scimitar/ac2/human_in_air.h"

#include "port/world.h"
#include "scimitar/ac2/human.h"

namespace scimitar::ac2 {

void HumanInAir::Update(const port::MoveInput&, const port::World& world, float dt) {
    Human& h = m_Human;
    // PORT: plain ballistic fall with no air control, no catches and a single landing.
    h.m_Velocity.z -= h.GetInAirData().gravity * dt;
    Vec3 next = h.m_Position + h.m_Velocity * dt;
    std::optional<float> ground = world.groundBelow(next.x, next.y, h.m_Position.z);
    if (ground && next.z <= *ground) {
        next.z = *ground;
        h.m_Position = next;
        h.m_Velocity = {};
        h.RequestContext(HumanContextId::Ground);
        return;
    }
    h.m_Position = next;
}

}  // namespace scimitar::ac2
