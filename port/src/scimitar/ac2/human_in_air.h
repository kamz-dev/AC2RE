#pragma once
// scimitar::ac2::HumanInAir: jumps, falls, catches and landings. Data: HumanInAirData (FastLoad PC 0x90C4D0).
// Mac: HumanContextBundle::GetInAir 0x12C490C. PC vftable 0x19C05B0 (LocalInit 0x6911A0).

#include "scimitar/ac2/human_context.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

class HumanInAir final : public ActorContext {
public:
    using ActorContext::ActorContext;
    HumanContextId GetID() const override { return HumanContextId::InAir; }
    void Enter() override;
    void Exit() override { m_PortEntered = false; }
    // Phase 0: the fall (PORT: ballistic) and the landing; phase 1: nothing yet.
    void Update(long phase, const PortUpdateArgs& args) override;

    // IHumanInAir::SetLandingSpeedRatio (Mac 0x15FF30A).
    void SetLandingSpeedRatio(float ratio) { m_LandingSpeedRatio = ratio; }
    float GetLandingSpeedRatio() const { return m_LandingSpeedRatio; }

private:
    // PC 0x69B730, Mac 0x1236156: the landing on the ground; picks the landing and hands over to HumanGround.
    void ExecMainToGroundMain(const Vec3& velocity);

    float m_LandingSpeedRatio = 0;  // +0x21C (PDB m_LandingSpeedRatio)
    bool m_PortEntered = false;     // PORT: a Human forced into InAir hasn't run Enter
};

}  // namespace scimitar::ac2
