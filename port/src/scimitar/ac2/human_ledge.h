#pragma once
// scimitar::ac2::HumanLedge: hanging from a ledge (RE/13_ledge_climb.md). Data: HumanLedgeData. PC vftable 0x19C0010
// (LinkFSM 0x65D2E0, _UpdateHumanLedge 0x6711A0); Mac: HumanContextBundle::GetLedge 0x12C48EA, full symbols.
//
// The port has the pull-down from the ground (PullDown -> HangEntry), the hang (Movement / Wait), the release
// (-> InAir) and the pull-up (Pullup -> Ground). The other 23 states are named but not ported: the context enters
// them when asked (EnterHumanLedge's switch) and then does nothing.

#include <cstdint>

#include "scimitar/ac2/human_context.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

// scimitar::ac2::IHumanLedge: what the controller drives the hang through (48 slots; Mac vftable 0x285C7E0, byte
// offsets in brackets, RE/13 8). The port has the slots it uses.
class IHumanLedge {
public:
    virtual ~IHumanLedge() = default;
    virtual void SetDirection(const Vec3& direction) = 0;          // [+0x00]
    virtual void SetSpeedRatio(float ratio) = 0;                   // [+0x04]
    virtual void SetDirectionType(std::uint32_t type) = 0;         // [+0x10]
    virtual void SetRelativeDirection(const Vec3& direction) = 0;  // [+0x14]
    virtual bool CanRelease() = 0;                                 // [+0x28] Mac 0x1342EE4
    virtual void DoRelease() = 0;                                  // [+0x2C] Mac 0x134731A (FootHold*, LandingIntention)
    virtual bool CanPullup() = 0;                                  // [+0x40] Mac 0x134308C
    virtual void DoPullup() = 0;                                   // [+0x44] Mac 0x134754A
};

class HumanLedge final : public ActorContext, public IHumanLedge {
public:
    // The FSM's state ids (LinkFSM PC 0x65D2E0; records from +0xE2C). Movement holds Wait and HandPlacement;
    // GraspHangFreeOneHandReception holds 22-23; PullupToActorInterraction holds 25-27.
    enum class State : std::uint8_t {
        None = 0,
        Movement = 1,
        Wait = 2,
        HandPlacement = 3,
        ParrallelJumpPreparation = 4,
        ParrallelJump = 5,
        PullDown = 6,
        HangEntry = 7,
        SwingEntryCollision = 8,
        SwingEntryCollisionBeam = 9,
        Pullup = 10,
        HandPassOver = 11,
        ReboundTransition = 12,
        HangWallReception = 13,
        HangFreeReception = 14,
        Swing = 15,
        SwingDownImpulsion = 16,
        SwingDown = 17,
        ReleaseToTarget = 18,
        GraspEntry = 19,
        SwimEntry = 20,
        GraspHangFreeOneHandReception = 21,
        GraspHangFreeOneHandReceptionInertPart = 22,
        GraspHangFreeLastHandPlacement = 23,
        PullupToActorInterraction = 24,
        PullupToActor = 25,
        GrabOnGround = 26,
        GrabOnGroundToLedge = 27,
        VerticalLeapImpulsion = 28,
        Hurt = 29,
    };

    using ActorContext::ActorContext;
    HumanContextId GetID() const override { return HumanContextId::Ledge; }
    void Enter() override;  // PC 0x670A00 -> _EnterHumanLedge (Mac 0x1347BE0)
    void Exit() override {}
    void Update(long phase, const PortUpdateArgs& args) override;  // PC 0x6711A0, Mac 0x1345F86

    // IHumanLedge
    void SetDirection(const Vec3& direction) override;
    void SetSpeedRatio(float ratio) override;
    void SetDirectionType(std::uint32_t type) override;
    void SetRelativeDirection(const Vec3& direction) override;
    bool CanRelease() override;
    void DoRelease() override;
    bool CanPullup() override;
    void DoPullup() override;

    State GetState() const { return m_State; }  // the root's current state (+0xDE2)
    State GetMovementState() const { return m_MovementState; }  // Movement's current child (+0xE2E)

private:
    // EnterHumanLedge (Mac 0x13477BA): the root's initial state from HumanLedgeData::SubState.
    State EntryStateFromSubState() const;
    void EnterState(State state);

    void EnterPullDown();      // Mac 0x133DE9E
    void UpdatePullDownMain(float dt);  // Mac 0x132BBA8
    bool TestPullDownToHangEntryMain() const;  // Mac 0x1325458
    void UpdateHangEntryMain();  // Mac 0x133DB24
    void EnterPullup();        // Mac 0x131C75C
    void UpdatePullupMain(float dt);

    // PORT: Human::GetToesDestAnchors (Mac 0x12DBE14) stand-in: is there a wall under the hands to brace the feet on?
    bool PortHasWallUnderEdge() const;
    // PORT: where the root is at `offset` from the pull-down gap's edge.
    Vec3 PortAtEdge(float out, float up) const;

    State m_State = State::None;          // +0xDE2
    State m_MovementState = State::Wait;  // +0xE2E

    // PORT: the current timed move (the DisplacementManager's interpolation): from, to, its length and the time in it.
    Vec3 m_PortMoveFrom;
    Vec3 m_PortMoveTo;
    float m_PortMoveLength = 0;
    float m_PortMoveTime = 0;
    void PortStartMove(const Vec3& to, float length);
    float PortStepMove(float dt);  // returns the progression ratio (DisplacementManager::GetProgressionRatio)
};

}  // namespace scimitar::ac2
