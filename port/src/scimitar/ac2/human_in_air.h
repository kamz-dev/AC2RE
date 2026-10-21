#pragma once
// scimitar::ac2::HumanInAir: jumps, falls, catches and landings. Data: HumanInAirData (FastLoad PC 0x90C4D0).
// Mac: HumanContextBundle::GetInAir 0x12C490C.

#include "scimitar/ac2/human_context.h"

namespace scimitar::ac2 {

class HumanInAir final : public HumanContextBase {
public:
    using HumanContextBase::HumanContextBase;
    HumanContextId GetID() const override { return HumanContextId::InAir; }
    void Update(const port::MoveInput& input, const port::World& world, float dt) override;
};

}  // namespace scimitar::ac2
