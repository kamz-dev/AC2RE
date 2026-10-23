#pragma once
// The contexts' data. In the game each context's data is a reflected class that also holds the context's runtime
// state (RE/11_ground.md 4 and 15): HumanGround reads its inputs from HumanGroundData (PC: HumanGround +0x10). The
// reflected members keep their reflected names (RE/tools/refl.py); the rest are named after what they do, with
// their PC offset. The layout is in sdk/include/ac2sdk/human_ground.h.

#include <cstdint>

#include "scimitar/ac2/edge_clamping.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

// scimitar::ac2::TransitionToMotionType: how a move cycle may blend in from a transition. It is also HumanGround's
// speed restriction (m_TransitionToMotionTypeInternal, PC +0x994; RestrictTargetSpeedRatio). Names from the
// Revelations PDB (RE/02); the caps they get in AC2 match them (RE/11_ground.md 5).
enum TransitionToMotionType : int {
    TransitionToMotionType_JogOnly = 0,
    TransitionToMotionType_BlendWalkToJog = 1,
    TransitionToMotionType_BlendWalkToJogWithRotation = 2,
    TransitionToMotionType_BlendWalkVerySlowToWalkToJogWithRotation = 3,
    TransitionToMotionType_BlendWalkToJogFromCollide = 4,
    TransitionToMotionType_BlendWalkToJogToSprint = 5,
    TransitionToMotionType_BlendWalkToJogToSprintWithFixedLayer = 6,
    TransitionToMotionType_BlendWalkVerySlowToWalkToJogToSprint = 7,
};

// Reflected class HumanGroundData (descriptor PC 0x1D1C528, size 0x3A0, parent HumanGroundDatav2). The unreflected
// members after +0xF3 take their names from the Revelations PDB, where they sit 0x20 later (RE/11_ground.md 4).
struct HumanGroundData {
    // HumanGroundData::MovementState (Revelations PDB names). AC2 numbering: 0-7 as in Revelations, 10-13 are
    // Revelations' 11-14 (EnterMove PC 0x60C610), 14 is MAX (ResetTransitionData). 8 is Collide (the beam's _Collide
    // exit, Mac 0xF211E8); 9 is CarryObject or CustomAction (not decoded). EnterMove has no case for 8 or 9.
    enum MovementState : int {
        MovementState_WaitLow = 0,
        MovementState_WaitHigh = 1,
        MovementState_WalkVerySlow = 2,
        MovementState_Walk = 3,
        MovementState_Jog = 4,
        MovementState_Run = 5,
        MovementState_Sprint = 6,
        MovementState_Crouch = 7,
        MovementState_Collide = 8,
        MovementState_Landing = 10,
        MovementState_LandingHard = 11,
        MovementState_LandingBalanceFall = 12,
        MovementState_HandPassOverReception = 13,
        MovementState_MAX = 14,
    };
    enum HumanGroundSubState {  // HumanGroundData::HumanGroundSubState
        SubState_Movement = 0,
        SubState_Fight = 1,
        SubState_FreeRun = 2,
        SubState_OrientedMove = 3,
        SubState_Hurt = 4,
        SubState_ObstacleCollision = 5,
    };
    enum MvtDivisions {  // HumanGroundData::MvtDivisions (the movement-division tables' indices)
        MvtDivision_WaitLow = 0,
        MvtDivision_WaitHigh = 1,
        MvtDivision_WalkVerySlow = 2,
        MvtDivision_Walk = 3,
        MvtDivision_Jog = 4,
        MvtDivision_Run = 5,
        MvtDivision_Crouch = 6,
        MvtDivision_MAX = 7,
        MvtDivision_Invalid = 8,
    };
    enum CombatStyle { CombatStyle_Normal = 0, CombatStyle_Assassin = 1, CombatStyle_MAX = 2 };
    enum ObstacleLeanType { ObstacleLeanType_Hands = 0, ObstacleLeanType_Feet = 1 };

    // Reflected members (PC offset). Handles (EntityToAvoid +0x9C, CollideEntity +0xC4, IdleBreakerActionBlock +0xE8)
    // and CarryObjectData (+0xA4) aren't mirrored yet.
    Vec3 m_DestGlobalBodyOrientation;              // +0x10
    Vec3 m_DestHeading;                            // +0x20 the direction the controller asks to move in
    Vec3 m_CollidePosition;                        // +0x30
    Vec3 m_CollideNormal;                          // +0x40
    Vec3 m_LCOActorSight;                          // +0x50
    Vec3 m_LCOPelvisSight;                         // +0x60
    Vec3 m_LCOFootSight;                           // +0x70
    HumanGroundSubState m_SubState = SubState_Movement;  // +0x80
    float m_CurSpeedRatio = 0;                     // +0x84
    float m_DestSpeedRatio = 0;                    // +0x88 the speed ratio the controller asks for (0..1)
    float m_HeadingLinearRotationSpeed = 0;        // +0x8C
    float m_HeadingEasedOutRotationSpeedRatio = 0; // +0x90
    std::uint32_t m_ParamFlags = 0;                // +0x94
    std::uint32_t m_InternalFlags = 0;             // +0x98
    CombatStyle m_CombatStyle = CombatStyle_Normal;  // +0xA0
    float m_CurrentBodyAngle = 0;                  // +0xB4
    float m_CurrentBodyBankingAngle = 0;           // +0xB8
    ObstacleLeanType m_ObstacleLeanType = ObstacleLeanType_Hands;  // +0xBC
    float m_CollideHeight = 0;                     // +0xC0
    float m_BlockCylinderRadius = 0;               // +0xC8
    float m_BlockCylinderLength = 0;               // +0xCC
    Vec3 m_BlockCylinderOffset;                    // +0xD0
    std::uint32_t m_CustomActionControlChannel = 0;    // +0xE0
    std::uint32_t m_CustomActionSecondaryChannel = 0;  // +0xE4
    bool m_IsSet = false;                          // +0xEC
    bool m_BreakHard = false;                      // +0xED IHumanGround::SetBreakHard; while running -> MoveStop (the player: AssassinTask, RE/21)
    bool m_Sprint = false;                         // +0xEE IHumanGround::SetSprint; +0.2 to the target ratio (LIVE row 5)
    bool m_ForceFallFromGrabThrow = false;         // +0xEF
    bool m_ForceRemainStandingOnAnyWallImpact = false;  // +0xF0
    bool m_ActingPlaying = false;                  // +0xF1
    bool m_ActingInterrupted = false;              // +0xF2
    bool m_Crouch = false;                         // +0xF3 IHumanGround::SetCrouch

    // Unreflected members (Revelations PDB names; PC offsets). m_HumanGroundHurtData etc. aren't mirrored.
    int m_HumanGroundStage = 0;         // +0x150 IHumanGround::HumanGroundStage; EnterMove: 1 (Navigation)
    // +0x154..+0x158: the movement-division tables, [0] for movement type Low and [1] for High. HumanGround::LocalInit
    // (PC 0x61F920) fills them; these defaults are its values.
    unsigned char m_NbMvtTypes = 2;     // +0x154
    unsigned char m_NbMvtDivisions[2] = {3, 3};  // +0x155
    float m_MvtDivThreshold[2][5] = {{0.0f, 0.9f, 1.0f}, {0.0f, 0.9f, 1.0f}};  // +0x158
    unsigned char m_CurMvtType = 0;     // +0x180 IHumanGroundAccess::MovementType; cleared by LocalInit
    unsigned char m_DestMvtType = 0;    // +0x181 the requested movement type: 1 (High) while high profile is held
    // +0x1A0: how HumanGround::EnterMove starts (RE/11_ground.md 11), left by TransitionSetupDataToMovement.
    MovementState m_DestMovementState = MovementState_MAX;
    float m_RotationRatio = 1.0f;       // +0x21C scales the heading rotation angles; PORT: initial value not seen
    float m_RotationRatioStep = 0.5f;   // +0x220 per second; PORT: initial value not seen (ExitMove sets 1 / 0.5)
    bool m_SkipActionOnStateEntry = false;  // +0x224 EnterMove sets it for MovementState_MAX (not used by the port)
    // +0x228: EnterMoveCycle copies it into HumanGround::m_TransitionToMotionTypeInternal.
    TransitionToMotionType m_TransitionToMotionType = TransitionToMotionType_BlendWalkToJog;

    // Mac 0x1119F44, PC 0x90E190: the first index below the count whose threshold is >= value, else 0.
    int GetMovementDivisionFromSpeedRatio(bool highProfile, float value) const {
        const int h = highProfile ? 1 : 0;
        for (int i = 0; i < m_NbMvtDivisions[h]; ++i) {
            if (m_MvtDivThreshold[h][i] >= value) return i;
        }
        return 0;
    }
    void SetRotationRatio(float ratio) { m_RotationRatio = ratio; }      // Mac 0x1119F9C, PC 0x90E250
    void SetRotationRatioStep(float step) { m_RotationRatioStep = step; }  // Mac 0x1119FAE
};

// scimitar::ac2::TransitionSetupDataToMovement (ctor Mac 0x150EF4C): what the context handing over to HumanGround
// leaves in HumanGroundData for EnterMove. PORT: the action transition, feet position, blend factors and the two
// bools aren't mirrored (no animation layer yet).
struct TransitionSetupDataToMovement {
    HumanGroundData::MovementState movementState = HumanGroundData::MovementState_MAX;
    float rotationRatio = 0;
    float rotationRatioStep = 0;
    TransitionToMotionType transitionToMotionType = TransitionToMotionType_BlendWalkToJog;

    // Mac 0x111AAC6.
    void SetupTransitionData(HumanGroundData& data) const {
        data.m_DestMovementState = movementState;
        data.m_RotationRatio = rotationRatio;
        data.m_RotationRatioStep = rotationRatioStep;
        data.m_TransitionToMotionType = transitionToMotionType;
    }
    // Mac 0x1119FE2.
    static void ResetTransitionData(HumanGroundData& data) {
        data.m_DestMovementState = HumanGroundData::MovementState_MAX;
        data.m_TransitionToMotionType = TransitionToMotionType_BlendWalkToJog;
    }
};

// PORT: ground speed in the game is the root motion of the MoveCycle animations blended by the speed ratio
// (RE/11_ground.md 5). Until those animations are decoded, the port maps the ratio to m/s through these stand-in
// speeds at ratio 0, 0.2, 0.4, 0.6, 0.8 and 1.0 (linear in between).
struct PortGroundSpeeds {
    float atRatio[6] = {0.0f, 1.2f, 1.8f, 3.0f, 4.5f, 6.0f};
    float crouchMove = 1.0f;  // PORT: CrouchMove's speed (its animation's root motion)
    // PORT: EdgeStop's step back from the edge after the unbalance (its loop's root motion). Live (session 6,
    // RE/11_ground.md 17): 0.50 m in the 0.67 s after the unbalance, steady.
    float edgeStopStepBack = 0.75f;
    // PORT: how far below the feet still counts as ground, metres. Not decoded.
    float stepDownHeight = 0.3f;
};

// PORT: the player's controller sets HumanGround's heading rotation through IHumanGround (RE/11_ground.md 7); its
// values aren't known yet (LIVE row 6). These stand-ins make the port turn briskly: about 170 deg/s for small
// corrections, up to about 520 deg/s for a quarter turn or more.
struct PortHeadingSettings {
    float minRotationSpeed;  // rad/s
    float maxRotationSpeed;  // rad/s
    float minRotationAngle;  // rad
    float maxRotationAngle;  // rad
};
// The player's values, read live from HumanGround +0x8B8..+0x8C4 (RE/20_live_checks.md row 6, 2026-10-21). The speeds
// are set every ground frame by AssassinTask::UpdateGroundMovement (1.5 / 4.0, or 0.975 / 2.6 with pad button 0,
// RE/21_input.md); the angles' setter isn't decoded, so these stay the initial values (PORT).
inline constexpr PortHeadingSettings kPortHeadingNormal{1.5f, 4.0f, 0.17453292f, 1.0471976f};

// PORT: the lengths of the animation-driven ground states' actions (TurnStart, MoveStop, RunTurn), which the game
// takes from the animations (g_TurnStartActionMap etc., RE/11_ground.md 10). Stand-ins until they are loaded.
struct PortActionLengths {
    float turnStart = 0.39f;  // live: 0.39 s walking, 0.27..0.30 s in high profile (RE/20 row 8 notes)
    // The action still current after TurnStart -> MoveCycle, before the move cycle's (RE/20 row 8). Live: 0.68 s
    // walking; 0.50 s in high profile, where the 2 - ratio time scale makes it 0.68 x ln 2 = 0.47 s.
    float moveCycleEntry = 0.68f;
    float moveStop = 0.4f;
    float runTurn = 0.6f;
    float landing = 0.5f;
    float landingHard = 0.9f;
    float landingBalanceFall = 1.2f;
    float roll = 0.8f;
    float crouchTurn = 0.6f;
    float turnOnSpot[4] = {0.4f, 0.6f, 0.8f, 1.0f};  // by GetTurnAnimation's size (< 25, 70, 135 degrees, more)
    // EdgeStop (RE/11_ground.md 17): the unbalance action (115916159), then the loop (115916157) until the target
    // action ends. Live (sessions 5-6): the pull-down to Ledge, which waits for the unbalance to finish, came 0.27 s
    // after EdgeStop began; with the stick released, EdgeStop went back to Move after 0.94 s (one sample).
    float edgeStopUnbalance = 0.27f;
    float edgeStop = 0.94f;
};

// Reflected class HumanLedgeData (descriptor PC 0x1D1AF70, size 0x1B0, parent HumanLedgeDatav2; RE/13_ledge_climb.md
// 3). Enum names from the Revelations PDB; AC2's LedgeSubState lacks Revelations' 14 (HookBladeGrasp), so 14-18 are
// Revelations' 15-19 (hypothesis: every case of EnterHumanLedge's switch fits).
struct HumanLedgeData {
    enum PullDownType : int {
        PullDownType_Ground_Soft_EdgeStop = 0,  // HumanGround's Exec from EdgeStop (live: the run)
        PullDownType_Ground_Soft_Wait = 1,      // from Move (live: the standing grasp)
        PullDownType_Ground_Hard = 2,
        PullDownType_Beam_Soft = 3,
        PullDownType_HandPassOver = 4,
        PullDownType_WaterWellSoft = 5,
        PullDownType_Ground_Fast = 6,
    };
    enum PullDownSide : int { PullDownSide_Front = 0, PullDownSide_Back = 1, PullDownSide_Left = 2, PullDownSide_Right = 3 };
    enum LedgeSubState : int {
        SubState_Entry = 0,
        SubState_Movement = 1,
        SubState_TurnCorner = 2,
        SubState_HandPlacement = 3,
        SubState_Pullup = 4,
        SubState_ReboundTransition = 5,
        SubState_HangWallReception = 6,
        SubState_HangFreeReception = 7,
        SubState_SwingReception = 8,
        SubState_TransitionInFromClimb = 9,
        SubState_TransitionInFromLadder = 10,
        SubState_PullDown = 11,
        SubState_HandPassOver = 12,
        SubState_Grasp = 13,
        SubState_ParallelJump = 14,
        SubState_SwimEntry = 15,
        SubState_PullupToActorGrab = 16,
        SubState_PullupToActorAssassinate = 17,
        SubState_JumpToActorAssassinate = 18,
    };
    enum PullDownSubState : int {
        PullDownSubState_None = 0,
        PullDownSubState_Orientation = 1,
        PullDownSubState_Descent = 2,
        PullDownSubState_Reception = 3,
        PullDownSubState_ReleaseToInAir = 4,
    };
    enum LedgeHangType : int { LedgeHangType_Wall = 0, LedgeHangType_Free = 1, LedgeHangType_WallFree = 2 };

    Vec3 m_CurDirection;                          // +0x10 IHumanLedge::SetDirection
    Vec3 m_CurRelativeDirection;                  // +0x20 IHumanLedge::SetRelativeDirection
    PullDownType m_PullDownType = PullDownType_Ground_Soft_EdgeStop;  // +0x40
    PullDownSide m_PullDownSide = PullDownSide_Front;                 // +0x44
    LedgeSubState m_SubState = SubState_Entry;                        // +0x4C the state the context enters in
    PullDownSubState m_PullDownSubState = PullDownSubState_None;      // +0x54
    std::uint32_t m_DirectionType = 0;            // +0x58 IHumanLedge::SetDirectionType
    float m_CurSpeedRatio = 0;                    // +0x5C IHumanLedge::SetSpeedRatio
    LedgeHangType m_LedgeHangType = LedgeHangType_Wall;  // +0x68
    Gap m_PullDownGap;                            // +0xF0 (Revelations m_PullDownGap) the gap a pull-down starts from
};

// PORT: the Ledge context's animation-driven moves. The game moves the body along each step's action
// (DisplacementManager / SetupInterpolatorForHumanLedgeMove) towards targets from the hands' anchor points
// (RE/13_ledge_climb.md 5), over the action's length. Until the anchors, the interpolator and the animations are
// ported, the port moves the entity root linearly between the positions measured live, over the measured durations
// (RE/20_live_checks.md row 15, 2026-10-23). Offsets are from the gap's edge: `out` along its flat normal, `up`.
struct PortLedgeMoves {
    struct Offset {
        float out, up;
    };
    // The pull-down's steps (PullDownSubState 1-3).
    float orientation[2] = {0.40f, 0.53f};  // by PullDownType: 0 (from EdgeStop), 1 (from Move)
    float descent = 0.13f;
    float receptionWall = 0.13f;
    float receptionFree = 0.33f;
    Offset orientationEnd{0.53f, -0.05f};
    Offset descentEnd{0.97f, -0.80f};
    Offset hangWall{0.539f, -1.082f};
    Offset hangFree{0.0f, -2.400f};
    // The pull-up (Pullup, then NarrowObject for 0.27 s, then Ground; the port has no NarrowObject context and stays
    // in Pullup for that time), and where the root ends, on the roof.
    float pullupWall = 0.73f;
    float pullupFree = 1.54f;
    float pullupNarrowObject = 0.27f;
    Offset standWall{-0.081f, 0.0f};
    Offset standFree{-0.050f, 0.0f};
    // PORT: Human::GetToesDestAnchors (Mac 0x12DBE14) looks for the toes' anchors on the wall 1.2 m under the hands
    // (0.65); the port casts a ray at the wall from this far out, at that depth.
    float toesDepth = 1.2f;
    float toesReach = 0.65f;
};

struct HumanInAirData {
    // PORT: placeholder gravity (m/s^2), not from the game.
    float gravity = 9.81f;
    // +0x10 JumpOrigin (reflected, a matrix): where the jump or fall started. The port keeps its translation (+0x40),
    // which HumanInAir::ExecMainToGroundMain measures the drop from.
    Vec3 m_JumpOrigin;
};

// Landing action ids (HumanInAir::ExecMainToGroundMain PC 0x69B730; HumanGround::TestLandingToWaitMain PC 0x5EB330).
namespace landing_action {
inline constexpr std::uint32_t StandingRightFoot = 1855747402;  // front foot 1
inline constexpr std::uint32_t StandingLeftFoot = 1855747404;
inline constexpr std::uint32_t StandingVertical = 1855747415;   // a steep fall, no landing speed
inline constexpr std::uint32_t MovingVertical = 1855746090;     // a steep fall with landing speed
inline constexpr std::uint32_t MovingRightFoot = 1830814048;    // + 1 for the other front foot
inline constexpr std::uint32_t Hard = 0x10DD707;                // LandingHard, no speed or fatal damage
inline constexpr std::uint32_t HardRoll = 0x10DD70B;            // LandingHard with speed
}  // namespace landing_action

}  // namespace scimitar::ac2
