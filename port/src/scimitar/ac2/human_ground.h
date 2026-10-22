#pragma once
// scimitar::ac2::HumanGround: everything on foot (RE/11_ground.md).
// PC: vftable 0x19BFCD8, ctor 0x6317A0, LocalInit 0x61F920, Update 0x63B2C0, LinkFSM 0x602270. Mac: LinkFSM
// 0xFA4130, Update(long) 0x1544B74. Bases in the game: scimitar::ActorContext, DataContext<Human, HumanData,
// HumanGroundData>, and IHumanGround (at +0x14).
//
// The game's FSM has 147 states (ac2sdk::HumanGroundState). The port runs the Move branch's Wait and MoveCycle
// states so far; the rest of Move's children come one by one with their RE.

#include "ac2sdk/human_ground.h"
#include "scimitar/ac2/edge_clamping.h"
#include "scimitar/ac2/human_data.h"
#include "scimitar/ac2/human_context.h"
#include "scimitar/math.h"
#include "scimitar/response_curve.h"

namespace scimitar::ac2 {

// Mac: scimitar::ac2::IHumanGround (PC vftable 0x19BF390, at HumanGround +0x14). Controllers (the player's, the
// AI's) and other systems talk to the ground context through it. PORT: only the members the port uses so far.
class IHumanGround {
public:
    virtual ~IHumanGround() = default;
    // The movement requests (RE/21_input.md; the game's slots in the Mac secondary vftable 0x281AC70 in brackets).
    virtual void SetDirection(const Vec3& direction) = 0;  // [+0x00] Mac 0xF98EE6
    virtual void SetSpeedRatio(float ratio) = 0;           // [+0x04] Mac 0xF9B710
    virtual void SetMovementType(int type) = 0;            // [+0x0C] Mac 0xFBA59A; IHumanGroundAccess::MovementType
    virtual bool GetSprint() const = 0;                    // [+0x84]
    virtual bool GetForceTurnAround() const = 0;           // [+0x540] Mac 0x153B8AC (thunk)
    // Queries for contexts and systems the port doesn't have (carrying, crowds, praying, obstacles). PORT: always
    // false until those are ported.
    virtual bool IsInObstacleCollision() const { return false; }  // [+0x420] Mac 0x153CD96 (thunk)
    virtual bool IsCarryingObject() const { return false; }       // [+0x7C0] Mac 0x153DAC8 (thunk)
    virtual bool IsPickPocketing() const { return false; }        // [+0x61C] Mac 0x153BD80 (thunk)
    virtual bool IsPushing() const { return false; }              // [+0x584] Mac 0x153BC90 (thunk)
    virtual bool GetGentlePushIntention() const { return false; } // [+0x580] Mac 0x153BA1E (thunk)
    virtual bool IsInCrowdShove() const { return false; }         // [+0x588] Mac 0x153BD8A (thunk)
    virtual bool IsPraying() const { return false; }              // [+0x674] Mac 0x153C3AA (thunk)
    virtual bool CanRoll() { return false; }  // Mac 0xEFE534 (not decoded)
    virtual void SetHeadingMinRotationSpeed(float radPerSecond) = 0;  // Mac 0xF9771C
    virtual void SetHeadingMaxRotationSpeed(float radPerSecond) = 0;  // Mac 0xF9772E
    virtual void SetHeadingMinRotationAngle(float radians) = 0;       // Mac 0xF97740
    virtual void SetHeadingMaxRotationAngle(float radians) = 0;       // Mac 0xF97752
    // IHumanGround::RotationDirection: 0 Shortest, -1 Left, 1 Right (Revelations PDB).
    virtual void SetHeadingRotationDirection(int direction) = 0;      // Mac 0xF97764
    virtual void SetTurnOnMovementMinAngle(float radians) = 0;        // Mac 0xF9825E
    virtual void SetForceTurnAround(bool force) = 0;                  // Mac 0x153B89A
    virtual void SetBreakHard(bool breakHard) = 0;                    // PC 0x632E20, Mac 0x153B958
    virtual void SetSprint(bool sprint) = 0;                          // PC 0x632E80, Mac 0x153B9D0
    virtual void SetCrouch(bool crouch) = 0;                          // Mac 0x153B994
    // IHumanGround::TurnMode: 0 MainCharacter, 1 NPC (Revelations PDB); TurnOnSpot needs NPC. Sets m_TurnMode (+0x464).
    virtual void SetTurnMode(int mode) = 0;                           // Mac 0x153B87E
    // Turning on the spot on request (AI, look-at). CanTurnOnSpot / DoTurnOnSpot test and run interface command 155
    // (TurnOnSpot, from Wait) with the direction to face; RE/11_ground.md 13.
    virtual bool CanTurnOnSpot(const Vec3& direction, float minAngle) = 0;  // Mac 0xFDB938
    virtual void DoTurnOnSpot(const Vec3& direction) = 0;                   // Mac 0xFEBEDE
    virtual bool IsInTurnOnSpotTurn() const = 0;                            // Mac 0xF9BC7A
    virtual void SetUseTurnOnSpotCycle(bool use) = 0;                       // Mac 0xF979FE
    // The edge commands the controller sends with a gap (RE/11_ground.md 17). Can* run the command's transition test
    // from the current state; Do* queue the command, which the FSM runs in its update.
    virtual bool IsInJumpImpulsion() const { return false; }  // [+0x418] PC 0x5E0B20. PORT: no jump states
    virtual bool CanFallOffEdge(const Gap& gap, bool force, bool b) = 0;    // [+0x43C] PC 0x62F0D0, Mac 0xFDA29A
    virtual void DoFallOffEdge(const Gap& gap, float value) = 0;            // [+0x440] PC 0x639E20
    // [+0x444] PC 0x62F140, Mac 0xFDA2FA. `direction` gets IsNarrowEdge's answer (1 = facing out over the edge).
    virtual bool CanUnbalanceOnEdge(const Gap& gap, float maxSquareDistance, int& direction, bool b) = 0;
    virtual void DoUnbalanceOnEdge(const Gap& gap, int direction, bool b) = 0;  // [+0x448] PC 0x639E80, Mac 0xFEA806
    virtual bool IsInEdgeStop() const = 0;                                   // [+0x44C] PC 0x5F4190, Mac 0xF9BC98
    virtual bool CanPullDownOnEdge(const Gap& gap) = 0;                      // [+0x46C] PC 0x62F1B0, Mac 0xFDA358
    virtual void DoPullDownOnEdge(const Gap& gap) = 0;                       // [+0x470] PC 0x639ED0, Mac 0xFEA84C
    virtual bool CanEnterLookDown() = 0;                                     // [+0x7F4] PC 0x62F920, Mac 0xFDAE98
    virtual void DoEnterLookDown(const Gap& gap) = 0;                        // [+0x7F8] PC 0x63A970, Mac 0xFEB656
};

// Mac 0xFB4A3C, PC 0x5FF930. Turns `from` towards `to` (about +Z) by one frame at an angular speed that grows with
// the angle left: minSpeed below minAngle, linear up to maxSpeed at maxAngle, maxSpeed beyond. Returns `to` once a
// step would reach it. `direction` 0 turns the short way; otherwise the side is forced (1 = counter-clockwise seen
// from above, -1 = clockwise; hypothesis for the sign). PORT: horizontal vectors only.
Vec3 RotateVectorToVectorWithAngularSpeed(const Vec3& from, const Vec3& to, float minSpeed, float maxSpeed,
                                          float minAngle, float maxAngle, int direction, float dt);

class HumanGround final : public ActorContext, public IHumanGround {
public:
    using State = ac2sdk::HumanGroundState;

    explicit HumanGround(Human& human);
    HumanContextId GetID() const override { return HumanContextId::Ground; }
    void LocalInit();  // PC 0x61F920 (vftable slot 2)
    void Enter() override;
    void Exit() override;
    void Update(const port::MoveInput& input, const port::World& world, float dt) override;
    void UpdateAfterPhysics(const port::World& world, float dt) override;

    // IHumanGround
    void SetDirection(const Vec3& direction) override;
    void SetSpeedRatio(float ratio) override;
    void SetMovementType(int type) override;
    bool GetSprint() const override;
    bool GetForceTurnAround() const override { return m_ForceTurnAround; }
    void SetHeadingMinRotationSpeed(float v) override { m_HeadingMinRotationSpeed = v; }
    void SetHeadingMaxRotationSpeed(float v) override { m_HeadingMaxRotationSpeed = v; }
    void SetHeadingMinRotationAngle(float v) override { m_HeadingMinRotationAngle = v; }
    void SetHeadingMaxRotationAngle(float v) override { m_HeadingMaxRotationAngle = v; }
    void SetHeadingRotationDirection(int v) override { m_HeadingRotationDirection = v; }
    void SetTurnOnMovementMinAngle(float v) override { m_TurnOnMovementMinAngle = v < kPi ? v : kPi; }
    void SetForceTurnAround(bool v) override { m_ForceTurnAround = v; }
    void SetBreakHard(bool v) override;
    void SetSprint(bool v) override;
    void SetCrouch(bool v) override;
    void SetTurnMode(int v) override { m_TurnMode = v; }
    bool CanTurnOnSpot(const Vec3& direction, float minAngle) override;
    void DoTurnOnSpot(const Vec3& direction) override;
    bool IsInTurnOnSpotTurn() const override { return m_MoveState == State::TurnOnSpot; }  // IsStateActive(TurnOnSpot)
    void SetUseTurnOnSpotCycle(bool use) override;
    bool CanFallOffEdge(const Gap& gap, bool force, bool b) override;
    void DoFallOffEdge(const Gap& gap, float value) override;
    bool CanUnbalanceOnEdge(const Gap& gap, float maxSquareDistance, int& direction, bool b) override;
    void DoUnbalanceOnEdge(const Gap& gap, int direction, bool b) override;
    bool IsInEdgeStop() const override { return m_MoveState == State::EdgeStop; }  // IsStateActive(EdgeStop)
    bool CanPullDownOnEdge(const Gap& gap) override;
    void DoPullDownOnEdge(const Gap& gap) override;
    bool CanEnterLookDown() override;
    void DoEnterLookDown(const Gap& gap) override;

    // Mac 0xFCBC84: how the character stands at the gap's edge. 0: no edge, a drop of 1.3 m or less, or further
    // than `maxSquareDistance`; otherwise by the flat angle from the sight to the edge's normal: 1 within 60 degrees
    // (120 with `wide`), 3 for (-120, -60], 4 for [60, 120), else 2.
    int IsNarrowEdge(const Gap& gap, float maxSquareDistance, bool wide) const;

    // Mac 0xFB3550, PC 0x5FDF00: which of the 8 turn-on-spot animations turns the sight towards `target`; the
    // signed angle sight -> target about +Z picks the size (25 / 70 / 135 degrees) and the side (even = positive).
    static int GetTurnAnimation(float signedAngle);

    State GetMoveState() const { return m_MoveState; }
    State GetWaitState() const { return m_WaitState; }
    float GetHeadingMinRotationSpeed() const { return m_HeadingMinRotationSpeed; }
    float GetHeadingMaxRotationSpeed() const { return m_HeadingMaxRotationSpeed; }
    float GetSpeedRatio() const { return m_CurrentSpeedRatio; }
    int GetGaitLevel() const { return m_CurrentMvtState; }
    int GetMovementDivision() const { return m_IntentionMvtState; }
    float GetTurnAngle() const { return m_TurnAngle; }
    bool IsRunning() const { return m_CurrentMvtType != 0; }
    const ResponseCurve& GetSlowDownCurve() const { return m_SlowDownCurve; }

    // --- Decoded rules of UpdateMoveCycleMain (PC 0x61BB60, Mac 0xFCD20A), RE/11_ground.md 5 ---

    // Step 1: the target speed ratio.
    static float ComputeTargetSpeedRatio(float stickMagnitude, bool sprint, bool highProfile, bool aiOverride,
                                         float aiSpeedRatio);
    // Step 2: the restriction mode's limits, applied while the current action isn't the move cycle.
    static float RestrictTargetSpeedRatio(float target, TransitionToMotionType restrictionMode, bool isPlayer, bool sprint,
                                          bool highProfile, float currentRatio);
    // Step 3: one frame of the ratio towards the target. `slowDownCurve` is the player's ResponseCurve output when
    // falling (1 for AI-driven humans).
    static float StepSpeedRatio(float current, float target, float step, float slowDownCurve);
    // Step 4: the gait level (PC +0x7A8) and running flag (+0x7B0) of a ratio.
    static void GaitFromSpeedRatio(float ratio, int& gaitLevel, int& running);

private:
    static constexpr float kPi = 3.14159265f;

    void UpdateMoveMain(float dt);         // PC 0x604990, Mac 0xFCA184
    void UpdateMovementIntention();        // PC 0x5E03E0, Mac 0xF98C54
    void UpdateTurnAngle();                // PC 0x5FE6A0, Mac 0xFB34C4
    void UpdateOrientation(float dt);      // PC 0x5FFC40, Mac 0xFB4E32
    void UpdateMoveCycleMain(float dt);    // PC 0x61BB60, Mac 0xFCD20A
    void UpdateTurnStartMain();            // PC 0x5EB6F0, Mac 0xFB65B6
    bool Has180TurnIntention() const;      // Mac 0xF9916C (inlined on PC)

    bool TestWaitToTurnStartMain() const;       // PC 0x5F4EC0, Mac 0xF9EA82
    bool TestWaitToMoveCycleMain() const;       // PC 0x5F4E70, Mac 0xF9EAE6
    bool TestMoveCycleToMoveStopMain() const;   // PC 0x5EAAF0, Mac 0xFB5CAE
    bool TestMoveCycleToWaitMain() const;       // PC 0x5EAB90, Mac 0xFB5D7A
    bool TestMoveCycleToTurnStartMain() const;  // PC 0x5F5B00, Mac 0xF9E8DA
    bool TestMoveStopToRunTurnMain(float dt) const;  // inlined in PC 0x621C80, Mac 0xF9DA6C
    bool TestMoveStopToWaitMain() const;        // PC 0x5F5B70, Mac 0xF9E88E
    bool TestMoveStopToMoveCycleMain() const;   // PC 0x5F5BB0, Mac 0xF9E840
    bool TestTurnStartToWaitMain(float dt) const;       // inlined in PC 0x621EE0, Mac 0xF9DDD6
    bool TestTurnStartToMoveCycleMain(float dt) const;  // inlined in PC 0x621EE0, Mac 0xF9DE14
    bool TestRunTurnToWaitMain() const;         // inlined in PC 0x621E60, Mac 0xF9DD4A
    bool TestRunTurnToMoveCycleMain() const;    // PC 0x5EB690, Mac 0xF9DD88

    bool TestLandingToWaitMain() const;               // PC 0x5EB330, Mac 0xFB5FA6
    bool IsStandingLanding() const;                   // the current action check in PC 0x5EB330
    bool TestLandingToMoveCycleMain() const;          // PC 0x5EB3A0, Mac 0xFB602A
    void ExecLandingToMoveCycleMain();                // Mac 0xFA920A
    void ExecTurnStartToWaitMain();                   // PC 0x614F40, Mac 0xFB664C
    void ExecTurnStartToMoveCycleMain();              // PC 0x615120, Mac 0xFB7474
    void ExecMoveStopToWaitMain();                    // inlined in PC 0x61F200, Mac 0xF9748A
    bool TestLandingHardToWaitMain(float dt) const;   // inlined in PC 0x630890, Mac 0xF9DC58
    bool TestLandingHardToMoveCycleMain(float dt) const;  // PC 0x5EB4C0, Mac 0xF9DC96
    bool TestLandingBalanceFallToWaitMain(float dt) const;  // Mac 0xF9DD20
    bool TestRollToWaitMain(float dt) const;          // Mac 0xF9DAB2
    bool TestRollToMoveCycleMain(float dt) const;     // Mac 0xF9DAF6

    bool TestWaitToTurnOnSpotMain() const;            // PC 0x5F4F30, Mac 0xF9E9F8
    bool TestTurnOnSpotDeadZone(const Vec3& direction, float minAngle) const;  // Mac 0xFB3A6C
    bool TestTurnOnSpotStepToTurnOnSpotStepMain() const;  // PC 0x5FE0A0, Mac 0xFB3684
    bool TestTurnOnSpotStepToWaitMain() const;        // inlined in PC 0x621F90, Mac 0xF9D8FE
    void StartTurnOnSpotAnim(const Vec3& target);     // PC 0x606B70, Mac 0xFB37D0
    void ExitTurnOnSpot();                            // PC 0x60D7A0, Mac 0xFA0944
    void ExecTurnOnSpotStepToWaitMain();              // PC 0x5EA930, Mac 0xF98EFE
    bool IsInCustomAction() const;                    // Mac 0xF9BC40 (IHumanGround virtual)
    int GetCurrentCustomActionOrientation() const;    // PC 0x5F2320, Mac 0xF9BDE0
    bool TestWaitToCrouchWaitMain() const;            // PC 0x5F4FC0, Mac 0xF9E9C2
    bool TestMoveCycleToCrouchMoveMain() const;       // inlined in Mac 0xFE05A2 (PC 0x5E1AC0)
    bool TestCrouchWaitToWaitMain(const port::World& world) const;  // inlined in PC 0x623FA0, Mac 0xFDEBB8
    bool TestCrouchWaitToCrouchMoveMain() const;      // PC 0x5EBDD0, Mac 0xF9DF6A
    bool TestCrouchMoveToMoveCycleMain(const port::World& world) const;  // inlined in PC 0x61E620, Mac 0xFDEB1A
    bool ActorCanPhysicallyStandUp(const port::World& world) const;  // PC 0x5F72C0, Mac 0xFDEA42
    void ExecWaitToCrouchWaitMain();                  // Mac 0xFB9206
    void ExecCrouchWaitToCrouchMoveMain();            // Mac 0xFB6D70
    void SetCrouched(bool crouched);                  // Mac 0xF97E58 (virtual)
    float PortCrouchTransitionProgress() const;
    bool PortIsActionInterruptibleByMovement() const;

    void EnterMove();        // PC 0x60C610, Mac 0xFA3D06
    void EnterMoveCycle();   // Mac 0xFBE706 (PC _EnterMoveCycle 0x619DD0)
    void ExitMoveCycle();    // PC 0x60DB70, Mac 0xF9FD94
    bool PortIsMoveCycleActionCurrent(float dt);  // PORT: the action check at PC 0x61BD2D
    void EnterTurnStart();   // PC 0x614BB0, Mac 0xFBE3D6
    void EnterMoveStop();    // PC 0x614370, Mac 0xFB7804
    void EnterRunTurn();     // Mac 0xFB76DE
    void SwitchMoveState(State next);

    // The edge commands' transition tests (RE/11_ground.md 17).
    bool TestMoveToEdgeStopOnUnbalanceOnEdge(const Gap& gap, float maxSquareDistance, int& direction, bool b) const;  // Mac 0xFCBF64
    bool TestMoveToLedgeOnLedgePullDown(const Gap& gap) const;      // Mac 0xFCBB42
    bool TestEdgeStopToLedgeOnLedgePullDown(const Gap& gap) const;  // Mac 0xFCB974
    bool TestGroundToInAirOnFallOffEdge(const Gap& gap, bool force) const;  // Mac 0xFCB406
    bool TestIsEdgeValidForFall(const Gap& gap) const;              // Mac 0xFCAB92
    bool TestEdgeStopToMoveMain() const;                            // Mac 0xF9D2EA
    // PORT: Human::TestPullDownToLedgeClearance (Mac 0x12DB8AA; two box casts at the edge) and
    // TestFallOffEdgeClearance aren't decoded: always clear.
    bool PortTestPullDownToLedgeClearance(const Gap&, bool) const { return true; }
    bool PortTestFallOffEdgeClearance(const Gap&) const { return true; }
    bool RunPendingCommand();  // true when the context is being left

    float PortSpeedFromRatio() const;

    // PORT: stands in for the action component (animations are not loaded yet). Each animation-driven state starts
    // one of these with a stand-in length; WillActionFinish / HasActionFinished read it.
    struct PortAction {
        float duration = 0;
        float elapsed = 0;
        void Start(float seconds) { duration = seconds; elapsed = 0; }
        bool WillFinish(float dt) const { return elapsed + dt >= duration; }
        bool HasFinished() const { return elapsed >= duration; }
        float Progress() const { return duration > 0 ? (elapsed < duration ? elapsed / duration : 1.0f) : 1.0f; }
    };

    // Mirrors of HumanGround's fields (PC offsets).
    State m_MoveState = State::Wait;     // the Move record's current child (+0x94)
    // The Wait record's current child (+0x97): Idle or LookDown. PORT: EdgeStop (a Navigation child in the game) is
    // kept in m_MoveState with Move's children.
    State m_WaitState = State::Idle;
    Gap m_LookDownGap;                   // the gap ExecIdleToLookDown stores (Mac +0x9B0)
    // PORT: the game switches context inside the Do* call (AIActor::TransitToContext, RE/10_human_update.md 3). Until
    // the port's switch mirrors it, the edge commands that leave the context wait for the ground's next update.
    enum class PortCommand { None, FallOffEdge, LedgePullDown };
    PortCommand m_PortPendingCommand = PortCommand::None;
    Gap m_PortPendingGap;
    // Names from the Revelations PDB's HumanGround (RE/11_ground.md 2); the enums' values are in the comments.
    int m_TurnMode = 0;                  // +0x464 IHumanGround::TurnMode: 0 MainCharacter, 1 NPC (TurnOnSpot needs 1)
    int m_TurnIntention = 0;             // +0x468 HumanGround::TurnIntention: 0 Left, 1 Right, 2 Back, 3 Unknown
    float m_IntendedTurnAngle = 0;       // +0x480 the turn angle when TurnStart was entered
    bool m_OnlyUseTurnOnSpot = false;    // +0x7A0 blocks Wait -> TurnStart
    int m_CurrentMvtState = 0;           // +0x7A8 IHumanGroundAccess::MovementState: 0 Wait, 1 Slow, 2 Fast
    int m_IntentionMvtState = 0;         // +0x7AC the same, from the movement-division tables
    int m_CurrentMvtType = 0;            // +0x7B0 IHumanGroundAccess::MovementType: 0 Low, 1 High (running)
    int m_IntentionMvtType = 0;          // +0x7B4 the same, from HumanGroundData::m_DestMvtType
    float m_CurrentSpeedRatio = 0;       // +0x7BC
    float m_IntentionSpeedRatio = 0;     // +0x7C0 used with SpeedControlType_DirectSpeed
    float m_TurnAngle = 0;               // +0x7D8
    ResponseCurve m_SlowDownCurve;       // +0x81C
    int m_SpeedControlType = 0;          // +0x8A8 IHumanGround::SpeedControlType: 0 MvtTypeAndMvtState, 1 DirectSpeed
    float m_HeadingMinRotationSpeed = 0;  // +0x8B8 (ctor: 0)
    float m_HeadingMaxRotationSpeed = 0;  // +0x8BC
    float m_HeadingMinRotationAngle = 0;  // +0x8C0
    float m_HeadingMaxRotationAngle = 0;  // +0x8C4
    int m_HeadingRotationDirection = 0;   // +0x8D0 IHumanGround::RotationDirection: 0 Shortest, -1 Left, 1 Right
    bool m_ForceTurnAround = false;       // +0x8D4 bit 0 of a bitfield (PDB name); read by Has180TurnIntention
    float m_TurnOnMovementMinAngle = kPi / 2;  // +0x990 (ctor: pi/2)
    // +0x994 the speed restriction (ctor: BlendWalkToJog); EnterMoveCycle copies HumanGroundData's into it.
    TransitionToMotionType m_TransitionToMotionTypeInternal = TransitionToMotionType_BlendWalkToJog;
    // +0x998 bit 0: set by ExecTurnStartToMoveCycleMain's high-profile branch; while the move-cycle action isn't
    // current yet, UpdateMoveCycleMain then plays the current action at 2 - its ratio. Cleared by ExitMoveCycle.
    std::uint8_t m_DoubleTransitionToMotionSpeed = 0;
    // +0x8D5..+0x8D9: bytes that Revelations packs into a bitfield after m_ForceTurnAround (same order).
    bool m_IsCrouchTransitionFromExternal = false;  // +0x8D5 set by ExecWaitToCrouchWaitMain (stand -> crouch playing)
    bool m_IsCrouched = false;           // +0x8D7 SetCrouched (presentation event 31 on change)
    bool m_TurnOnSpotCycle = false;      // +0x8DD SetUseTurnOnSpotCycle: chain turn-on-spot steps while set
    Vec3 m_TurnOnSpotDir;                // +0x8E0 the direction TurnOnSpot turns to (PDB m_TurnOnSpotDir)

    PortAction m_PortAction;
    float m_PortSpeed = 0;           // PORT: the stand-in speed this frame
    float m_PortEntrySpeed = 0;      // PORT: speed when a stop / run turn started, faded out over the action
    float m_PortTurnToApply = 0;     // PORT: the turn a TurnStart / RunTurn animation would make, spread over it
    float m_PortActionTimeScale = 1; // PORT: ActionComponent::SetTimeScale on the stand-in action
    bool m_PortMoveCycleEntryPending = false;  // PORT: the action before the move cycle is still current (RE/20 row 8)
};


}  // namespace scimitar::ac2
