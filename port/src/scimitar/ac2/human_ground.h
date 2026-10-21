#pragma once
// scimitar::ac2::HumanGround: everything on foot (RE/11_ground.md).
// PC: vftable 0x19BFCD8, ctor 0x6317A0, LocalInit 0x61F920, Update 0x63B2C0, LinkFSM 0x602270. Mac: LinkFSM
// 0xFA4130, Update(long) 0x1544B74. Bases in the game: scimitar::ActorContext, DataContext<Human, HumanData,
// HumanGroundData>, and IHumanGround (at +0x14).
//
// The game's FSM has 147 states (ac2sdk::HumanGroundState). The port runs the Move branch's Wait and MoveCycle
// states so far; the rest of Move's children come one by one with their RE.

#include "ac2sdk/human_ground.h"
#include "scimitar/ac2/human_context.h"
#include "scimitar/math.h"
#include "scimitar/response_curve.h"

namespace scimitar::ac2 {

// Mac: scimitar::ac2::IHumanGround (PC vftable 0x19BF390, at HumanGround +0x14). Controllers (the player's, the
// AI's) and other systems talk to the ground context through it. PORT: only the members the port uses so far.
class IHumanGround {
public:
    virtual ~IHumanGround() = default;
    virtual bool CanRoll() { return false; }  // Mac 0xEFE534 (not decoded)
    virtual void SetHeadingMinRotationSpeed(float radPerSecond) = 0;  // Mac 0xF9771C
    virtual void SetHeadingMaxRotationSpeed(float radPerSecond) = 0;  // Mac 0xF9772E
    virtual void SetHeadingMinRotationAngle(float radians) = 0;       // Mac 0xF97740
    virtual void SetHeadingMaxRotationAngle(float radians) = 0;       // Mac 0xF97752
    // IHumanGround::RotationDirection in the game; 0 = the short way, -1 / 1 force a side (values' names unknown).
    virtual void SetHeadingRotationDirection(int direction) = 0;      // Mac 0xF97764
    virtual void SetTurnOnMovementMinAngle(float radians) = 0;        // Mac 0xF9825E
    virtual void SetForceTurnAround(bool force) = 0;                  // Mac 0x153B89A
    virtual void SetBreakHard(bool breakHard) = 0;                    // PC 0x632E20, Mac 0x153B958
    virtual void SetSprint(bool sprint) = 0;                          // PC 0x632E80, Mac 0x153B9D0
    virtual void SetCrouch(bool crouch) = 0;                          // Mac 0x153B994
    // IHumanGround::TurnMode in the game; TurnOnSpot needs 1 (value names unknown). Target hypothesis: +0x464.
    virtual void SetTurnMode(int mode) = 0;                           // Mac 0x153B87E
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
    void Update(const port::MoveInput& input, const port::World& world, float dt) override;

    // IHumanGround
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

    // Mac 0xFB3550, PC 0x5FDF00: which of the 8 turn-on-spot animations turns the sight towards `target`; the
    // signed angle sight -> target about +Z picks the size (25 / 70 / 135 degrees) and the side (even = positive).
    static int GetTurnAnimation(float signedAngle);

    State GetMoveState() const { return m_MoveState; }
    float GetSpeedRatio() const { return m_SpeedRatio; }
    int GetGaitLevel() const { return m_GaitLevel; }
    int GetMovementDivision() const { return m_MovementDivision; }
    float GetTurnAngle() const { return m_TurnAngle; }
    bool IsRunning() const { return m_Running != 0; }
    const ResponseCurve& GetSlowDownCurve() const { return m_SlowDownCurve; }

    // --- Decoded rules of UpdateMoveCycleMain (PC 0x61BB60, Mac 0xFCD20A), RE/11_ground.md 5 ---

    // Step 1: the target speed ratio.
    static float ComputeTargetSpeedRatio(float stickMagnitude, bool sprint, bool highProfile, bool aiOverride,
                                         float aiSpeedRatio);
    // Step 2: the restriction mode's limits, applied while the current action isn't the move cycle.
    static float RestrictTargetSpeedRatio(float target, int restrictionMode, bool isPlayer, bool sprint,
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
    bool TestWaitToCrouchWaitMain() const;            // PC 0x5F4FC0, Mac 0xF9E9C2
    bool TestMoveCycleToCrouchMoveMain() const;       // inlined in Mac 0xFE05A2 (PC 0x5E1AC0)
    bool TestCrouchWaitToWaitMain() const;            // inlined in PC 0x623FA0, Mac 0xFDEBB8
    bool TestCrouchWaitToCrouchMoveMain() const;      // PC 0x5EBDD0, Mac 0xF9DF6A
    bool TestCrouchMoveToMoveCycleMain() const;       // inlined in PC 0x61E620, Mac 0xFDEB1A
    bool ActorCanPhysicallyStandUp() const;           // PC 0x5F72C0, Mac 0xFDEA42
    bool PortIsActionInterruptibleByMovement() const;

    void EnterMove();        // PC 0x60C610, Mac 0xFA3D06
    void EnterMoveCycle();   // Mac 0xFBE706 (PC _EnterMoveCycle 0x619DD0)
    void EnterTurnStart();   // PC 0x614BB0, Mac 0xFBE3D6
    void EnterMoveStop();    // PC 0x614370, Mac 0xFB7804
    void EnterRunTurn();     // Mac 0xFB76DE
    void SwitchMoveState(State next);

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
    int m_TurnMode = 0;                  // +0x464 (hypothesis: SetTurnMode's target); TurnOnSpot needs 1
    int m_TurnStartSide = 0;             // +0x468 1 when the turn angle is >= 0
    float m_TurnStartAngle = 0;          // +0x480 the turn angle when TurnStart was entered
    bool m_BlockTurnStartFromWait = false;  // +0x7A0
    int m_GaitLevel = 0;                 // +0x7A8
    int m_MovementDivision = 0;          // +0x7AC
    int m_Running = 0;                   // +0x7B0
    int m_HighProfileIntention = 0;      // +0x7B4
    float m_SpeedRatio = 0;              // +0x7BC
    float m_AISpeedRatio = 0;            // +0x7C0
    float m_TurnAngle = 0;               // +0x7D8
    ResponseCurve m_SlowDownCurve;       // +0x81C
    int m_AISpeedOverride = 0;           // +0x8A8
    float m_HeadingMinRotationSpeed = 0;  // +0x8B8 (ctor: 0)
    float m_HeadingMaxRotationSpeed = 0;  // +0x8BC
    float m_HeadingMinRotationAngle = 0;  // +0x8C0
    float m_HeadingMaxRotationAngle = 0;  // +0x8C4
    int m_HeadingRotationDirection = 0;   // +0x8D0
    bool m_ForceTurnAround = false;       // +0x8D4 (Has180TurnIntention; SetForceTurnAround's target, hypothesis)
    float m_TurnOnMovementMinAngle = kPi / 2;  // +0x990 (ctor: pi/2)
    int m_SpeedRestriction = 1;               // +0x994 (ctor: 1)

    PortAction m_PortAction;
    float m_PortSpeed = 0;           // PORT: the stand-in speed this frame
    float m_PortEntrySpeed = 0;      // PORT: speed when a stop / run turn started, faded out over the action
    float m_PortTurnToApply = 0;     // PORT: the turn a TurnStart / RunTurn animation would make, spread over it
    float m_PortActionTimeScale = 1; // PORT: ActionComponent::SetTimeScale on the stand-in action
};

// HumanGroundData::MovementState values that EnterMove understands (RE/11_ground.md 11; names partly hypothesis).
namespace movement_state {
inline constexpr int WaitLow = 0;
inline constexpr int WaitHigh = 1;
inline constexpr int WalkVerySlow = 2;
inline constexpr int Walk = 3;
inline constexpr int Jog = 4;
inline constexpr int Run = 5;
inline constexpr int Sprint = 6;
inline constexpr int Crouch = 7;
inline constexpr int Landing = 10;
inline constexpr int LandingHard = 11;
inline constexpr int LandingBalanceFall = 12;
inline constexpr int HandPassOverReception = 13;
inline constexpr int Default = 14;  // what ResetTransitionData leaves: Wait
}  // namespace movement_state

}  // namespace scimitar::ac2
