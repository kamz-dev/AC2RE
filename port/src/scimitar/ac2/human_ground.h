#pragma once
// scimitar::ac2::HumanGround: walking, running and everything else on foot.
// Mac: HumanGround::LinkFSM 0xFA4130, HumanGround::Update(long) 0x1544B74; interface IHumanGround (ctor 0x153BBE2,
// CanRoll 0xEFE534). The FSM has many states (Shocked, Push, Steal, Grabbed..., TurnOnSpotStep, ...): only the
// movement ones are in scope.

#include "scimitar/ac2/human_context.h"

namespace scimitar::ac2 {

// Mac: scimitar::ac2::IHumanGround. Other systems (combat clips, ClimbClip::UpdateGroundContext) talk to the ground
// context through it. PORT: only the members the port needs so far.
class IHumanGround {
public:
    virtual ~IHumanGround() = default;
    virtual bool CanRoll() { return false; }  // Mac 0xEFE534 (7 bytes; not decoded)
};

class HumanGround final : public HumanContextBase, public IHumanGround {
public:
    using HumanContextBase::HumanContextBase;
    HumanContextId GetID() const override { return HumanContextId::Ground; }
    void Update(const port::MoveInput& input, const port::World& world, float dt) override;
};

}  // namespace scimitar::ac2
