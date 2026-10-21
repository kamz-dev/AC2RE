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

    State GetMoveState() const { return m_MoveState; }
    float GetSpeedRatio() const { return m_SpeedRatio; }
    int GetGaitLevel() const { return m_GaitLevel; }
    int GetMovementDivision() const { return m_MovementDivision; }
    float GetTurnAngle() const { return m_TurnAngle; }
    bool IsRunning() const { return m_Running != 0; }
    const ResponseCurve& GetSlowDownCurve() const { return m_SlowDownCurve; }

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
    static constexpr float kPi = 3.14159265f;

    void UpdateMoveMain(float dt);         // PC 0x604990, Mac 0xFCA184
    void UpdateMovementIntention();        // PC 0x5E03E0, Mac 0xF98C54
    void UpdateTurnAngle();                // PC 0x5FE6A0, Mac 0xFB34C4
    void UpdateOrientation(float dt);      // PC 0x5FFC40, Mac 0xFB4E32
    void UpdateMoveCycleMain(float dt);    // PC 0x61BB60, Mac 0xFCD20A
    bool TestWaitToMoveCycleMain() const;  // PC 0x5F4E70, Mac 0xF9EAE6
    bool TestMoveCycleToWaitMain() const;  // PC 0x5EAB90, Mac 0xFB5D7A
    float PortSpeedFromRatio() const;

    // Mirrors of HumanGround's fields (PC offsets).
    State m_MoveState = State::Wait;     // the Move record's current child (+0x94)
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
    float m_TurnOnMovementMinAngle = kPi / 2;  // +0x990 (ctor: pi/2)
    int m_SpeedRestriction = 1;               // +0x994 (ctor: 1)
};

}  // namespace scimitar::ac2
