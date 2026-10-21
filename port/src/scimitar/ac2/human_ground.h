#pragma once
// scimitar::ac2::HumanGround: everything on foot (RE/11_ground.md).
// PC: vftable 0x19BFCD8, ctor 0x6317A0, Update 0x63B2C0, LinkFSM 0x602270. Mac: LinkFSM 0xFA4130,
// Update(long) 0x1544B74; interface IHumanGround (ctor 0x153BBE2, CanRoll 0xEFE534).
// Bases in the game: scimitar::ActorContext and DataContext<Human, HumanData, HumanGroundData>.
//
// The game's FSM has 147 states (ac2sdk::HumanGroundState). The port runs the Move branch's Wait and MoveCycle
// states so far; the rest of Move's children come one by one with their RE.

#include "ac2sdk/human_ground.h"
#include "scimitar/ac2/human_context.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

// Mac: scimitar::ac2::IHumanGround. Other systems (combat clips, ClimbClip::UpdateGroundContext) talk to the ground
// context through it. PORT: only the members the port needs so far.
class IHumanGround {
public:
    virtual ~IHumanGround() = default;
    virtual bool CanRoll() { return false; }  // Mac 0xEFE534 (7 bytes; not decoded)
};

class HumanGround final : public ActorContext, public IHumanGround {
public:
    using State = ac2sdk::HumanGroundState;

    using ActorContext::ActorContext;
    HumanContextId GetID() const override { return HumanContextId::Ground; }
    void Enter() override;
    void Update(const port::MoveInput& input, const port::World& world, float dt) override;

    State GetMoveState() const { return m_MoveState; }
    float GetSpeedRatio() const { return m_SpeedRatio; }
    int GetGaitLevel() const { return m_GaitLevel; }
    bool IsRunning() const { return m_Running != 0; }

    // --- Decoded rules of UpdateMoveCycleMain (PC 0x61BB60, Mac 0xFCD20A), RE/11_ground.md 5 ---

    // Step 1: the target speed ratio.
    static float ComputeTargetSpeedRatio(float stickMagnitude, bool speedBandUp, bool highProfile, bool aiOverride,
                                         float aiSpeedRatio);
    // Step 2: the restriction mode's limits, applied while the current action isn't the move cycle.
    static float RestrictTargetSpeedRatio(float target, int restrictionMode, bool isPlayer, bool speedBandUp,
                                          bool highProfile, float currentRatio);
    // Step 3: one frame of the ratio towards the target. `slowDownCurve` is the player's ResponseCurve output when
    // falling (1 for AI-driven humans).
    static float StepSpeedRatio(float current, float target, float step, float slowDownCurve);
    // Step 4: the gait level (PC +0x7A8) and running flag (+0x7B0) of a ratio.
    static void GaitFromSpeedRatio(float ratio, int& gaitLevel, int& running);

private:
    void UpdateMovementIntention();  // Mac 0xF98C54 (PC not matched yet)
    void UpdateMoveCycleMain(float dt);
    bool TestWaitToMoveCycleMain() const;   // Mac 0xF9EAE6
    bool TestMoveCycleToWaitMain() const;   // PC 0x5EAB90, Mac 0xFB5D7A
    float PortSpeedFromRatio() const;

    // Mirrors of HumanGround's fields (PC offsets).
    State m_MoveState = State::Wait;  // the Move record's current child (+0x94)
    int m_GaitLevel = 0;              // +0x7A8
    int m_Running = 0;                // +0x7B0
    int m_HighProfileIntention = 0;   // +0x7B4
    float m_SpeedRatio = 0;           // +0x7BC
    float m_AISpeedRatio = 0;         // +0x7C0
    int m_AISpeedOverride = 0;        // +0x8A8
    int m_SpeedRestriction = 0;       // +0x994
    Vec3 m_LastMoveDirection{0, 1, 0};  // PORT: keeps moving while the ratio winds down with no stick
};

}  // namespace scimitar::ac2
