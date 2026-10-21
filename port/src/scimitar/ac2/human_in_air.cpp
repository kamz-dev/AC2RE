#include "scimitar/ac2/human_in_air.h"

#include "port/world.h"
#include "scimitar/ac2/human.h"
#include "scimitar/ac2/human_ground.h"

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
        // The game hands over through TransitionSetupDataToMovement (HumanGround::EnterMove reads the MovementState).
        // PORT: InAir's choice of landing (LandingType: Safe / SmallDamage / HeavyDamage / Fatal) isn't decoded; the
        // port always lands with a plain Landing.
        TransitionSetupDataToMovement{movement_state::Landing, h.GetGroundData().m_RotationRatio,
                                      h.GetGroundData().m_RotationRatioStep, 1}
            .SetupTransitionData(h.GetGroundData());
        h.RequestContext(HumanContextId::Ground);
        return;
    }
    h.m_Position = next;
}

}  // namespace scimitar::ac2
