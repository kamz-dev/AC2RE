#pragma once
// scimitar::ac2::HumanGround as it sits in the PC game (RE/11_ground.md). PC vftable 0x19BFCD8; Mac symbols in
// IDB/scimitar_sf.i64. Only fields read in the PC decompilation are declared; the rest is padding. The class's full
// size is not known yet, so it is not asserted.

#include <cstdint>

#include "ac2sdk/layout.h"

namespace ac2sdk {

// FSM state ids, as HumanGround::LinkFSM (PC 0x602270, Mac 0xFA4130) assigns them. Only the ids whose dispatcher
// has been identified are listed.
enum class HumanGroundState : std::uint8_t {
    Vulnerable = 1,
    Navigation = 2,
    Move = 3,
    Wait = 4,
    Idle = 5,
    LookDown = 8,
    HumanGroundSearch = 10,  // sub-machine
    Loot = 11,
    MoveCycle = 15,
    MoveCycleDefault = 16,  // MoveCycle's initial child (LinkFSM); name from Mac _UpdateMoveCycleDefault (hypothesis)
    Charge = 17,
    PushSoft = 20,
    PushCrowdShove = 21,
    MoveStop = 22,
    Roll = 23,
    HandPassOverReception = 24,
    Landing = 25,
    LandingHard = 26,
    LandingBalanceFall = 27,
    RunTurn = 28,
    TurnStart = 29,
    TurnOnSpot = 30,
    Shocked = 32,
    CrouchWait = 35,
    CrouchMove = 36,
    CrouchTurn = 37,
    JumpImpulsion = 38,
    JumpImpulsionStraight = 39,
    JumpImpulsionForwardFromIdle = 40,
    WallingImpulsion = 41,
    ObstacleCollision = 42,
    EdgeStop = 43,
    OrientedMove = 44,  // sub-machine OrientedMoveState
    CarryObject = 45,   // sub-machine
    CustomAction = 46,  // sub-machine
    Acting = 47,        // sub-machine
    Frozen = 48,
    HumanGroundDodge = 49,  // sub-machine
    Rest = 50,
    StealedSurprise = 61,
    EdgeLanding = 62,
    GetUp = 63,  // sub-machine
    Fight = 64,  // sub-machine
    HumanGroundHurt = 139,
    HumanGroundHurtFight = 140,
    HumanGroundAgonizing = 141,
    Assassinate = 143,
    GetAssassinated = 144,
    Tackle = 145,
    GetTackled = 146,
};

// A simple FSM state record (3 bytes). Sub-machines are 8-byte FSMClassStateWrapper objects instead.
struct FSMStateRecord {
    std::uint8_t id;
    std::uint8_t initialChild;
    std::uint8_t currentChild;
};
static_assert(sizeof(FSMStateRecord) == 3);

// Byte offsets of the records of the locomotion states (from LinkFSM and the dispatchers).
namespace human_ground_record {
inline constexpr std::uint32_t Vulnerable = 0x8C;
inline constexpr std::uint32_t Navigation = 0x8F;
inline constexpr std::uint32_t Move = 0x92;
inline constexpr std::uint32_t Wait = 0x95;
inline constexpr std::uint32_t MoveCycle = 0xBC;
}  // namespace human_ground_record

struct HumanGround {
    void* vftable;                  // 0x000  0x19BFCD8
    AC2_PAD(0x04);                  // 0x004
    std::uint32_t actor;            // 0x008  scimitar::AIActor*
    std::uint32_t humanData;        // 0x00C  HumanData* (hypothesis)
    std::uint32_t groundData;       // 0x010  HumanGroundData*
    AC2_PAD(0x41 - 0x14);           // 0x014
    std::uint8_t fsmInitialState;   // 0x041
    std::uint8_t fsmCurrentState;   // 0x042  read by _UpdateHumanGround 0x6381D0
    AC2_PAD(0x8C - 0x43);           // 0x043  (+0x44: FSMSuccessfulInterfaceTransitionTracker)
    std::uint8_t fsmStates[0x2AD - 0x8C];  // 0x08C  records and wrappers, ids 1..147 (LinkFSM 0x602270)
    AC2_PAD(0x468 - 0x2AD);         // 0x2AD
    std::int32_t turnIntention;     // 0x468  m_TurnIntention: 0 Left, 1 Right, 2 Back, 3 Unknown; EnterTurnStart 0x614BB0
    AC2_PAD(0x480 - 0x46C);         // 0x46C
    float intendedTurnAngle;        // 0x480  m_IntendedTurnAngle: the turn angle when TurnStart was entered
    AC2_PAD(0x6F9 - 0x484);         // 0x484
    std::uint8_t walkingOnHorse;    // 0x6F9  _UpdateMove 0x634D20
    AC2_PAD(0x790 - 0x6FA);         // 0x6FA
    std::uint32_t upperBody;        // 0x790  HumanUpperBody* (hypothesis)
    AC2_PAD(0x7A0 - 0x794);         // 0x794
    std::uint8_t onlyUseTurnOnSpot; // 0x7A0  m_OnlyUseTurnOnSpot; TestWaitToTurnStartMain 0x5F4EC0
    AC2_PAD(0x7A8 - 0x7A1);         // 0x7A1
    std::int32_t currentMvtState;   // 0x7A8  m_CurrentMvtState (IHumanGroundAccess::MovementState 0 Wait, 1 Slow, 2 Fast)
    std::int32_t intentionMvtState; // 0x7AC  m_IntentionMvtState; UpdateMovementIntention 0x5E03E0
    std::int32_t currentMvtType;    // 0x7B0  m_CurrentMvtType (IHumanGroundAccess::MovementType 0 Low, 1 High)
    std::int32_t intentionMvtType;  // 0x7B4  m_IntentionMvtType (from HumanGroundData::m_DestMvtType)
    AC2_PAD(0x04);                  // 0x7B8
    float currentSpeedRatio;        // 0x7BC  m_CurrentSpeedRatio: 0..1, drives the MoveCycle blend
    float intentionSpeedRatio;      // 0x7C0  m_IntentionSpeedRatio (SpeedControlType_DirectSpeed)
    AC2_PAD(0x7D8 - 0x7C4);         // 0x7C4
    float turnAngle;                // 0x7D8  move direction -> sight about +Z; UpdateTurnAngle 0x5FE6A0
    AC2_PAD(0x04);                  // 0x7DC
    float headingBeforeRotationUpdate[4];  // 0x7E0  the entity's sight before turning; UpdateMoveMain 0x604990
    AC2_PAD(0x81C - 0x7F0);         // 0x7F0
    std::uint8_t slowDownCurve[4];  // 0x81C  ResponseCurve object (size unknown); LocalInit 0x61F920
    AC2_PAD(0x8A8 - 0x820);         // 0x820
    std::int32_t speedControlType;  // 0x8A8  m_SpeedControlType: 0 MvtTypeAndMvtState, 1 DirectSpeed
    float alternateJogBlendFactor;  // 0x8AC  m_AlternateJogBlendFactor (MoveCycle blend slot 12)
    float alternateSprintBlendFactor;  // 0x8B0  m_AlternateSprintBlendFactor (MoveCycle blend slot 19)
    std::uint8_t overrideMoveCyclebySubState;  // 0x8B4  m_OverrideMoveCyclebySubState; UpdateMoveCycleMain returns early
    AC2_PAD(0x03);                  // 0x8B5
    float headingMinRotationSpeed;  // 0x8B8  rad/s; UpdateOrientation 0x5FFC40
    float headingMaxRotationSpeed;  // 0x8BC
    float headingMinRotationAngle;  // 0x8C0  rad, scaled by HumanGroundData rotation ratio
    float headingMaxRotationAngle;  // 0x8C4
    float savedHeadSpeed;           // 0x8C8  m_SavedHeadSpeed: 15.0 at construction
    float savedBodySpeed;           // 0x8CC  m_SavedBodySpeed: 12.5 at construction
    std::int32_t headingRotationDirection;  // 0x8D0  IHumanGround::RotationDirection, -1 / 0 / 1
    std::uint8_t forceTurnAround;   // 0x8D4  bit 0: m_ForceTurnAround (a bitfield); Has180TurnIntention
    AC2_PAD(0x8DD - 0x8D5);         // 0x8D5
    std::uint8_t turnOnSpotCycle;   // 0x8DD  m_TurnOnSpotCycle; SetUseTurnOnSpotCycle, TurnOnSpotStep -> Step
    AC2_PAD(0x8E0 - 0x8DE);         // 0x8DE
    float turnOnSpotDir[4];         // 0x8E0  m_TurnOnSpotDir: the direction TurnOnSpot turns to
    AC2_PAD(0x990 - 0x8F0);         // 0x8F0
    float turnOnMovementMinAngle;   // 0x990  pi/2 at construction; Wait / MoveCycle -> TurnStart
    std::int32_t transitionToMotionTypeInternal;  // 0x994  m_TransitionToMotionTypeInternal (TransitionToMotionType)
    std::uint8_t doubleTransitionToMotionSpeed;   // 0x998  m_DoubleTransitionToMotionSpeed (bit 0)
};

AC2_ASSERT_OFFSET(HumanGround, actor, 0x008);
AC2_ASSERT_OFFSET(HumanGround, groundData, 0x010);
AC2_ASSERT_OFFSET(HumanGround, fsmCurrentState, 0x042);
AC2_ASSERT_OFFSET(HumanGround, fsmStates, 0x08C);
AC2_ASSERT_OFFSET(HumanGround, turnIntention, 0x468);
AC2_ASSERT_OFFSET(HumanGround, intendedTurnAngle, 0x480);
AC2_ASSERT_OFFSET(HumanGround, walkingOnHorse, 0x6F9);
AC2_ASSERT_OFFSET(HumanGround, onlyUseTurnOnSpot, 0x7A0);
AC2_ASSERT_OFFSET(HumanGround, forceTurnAround, 0x8D4);
AC2_ASSERT_OFFSET(HumanGround, turnOnSpotCycle, 0x8DD);
AC2_ASSERT_OFFSET(HumanGround, turnOnSpotDir, 0x8E0);
AC2_ASSERT_OFFSET(HumanGround, upperBody, 0x790);
AC2_ASSERT_OFFSET(HumanGround, currentMvtState, 0x7A8);
AC2_ASSERT_OFFSET(HumanGround, intentionMvtState, 0x7AC);
AC2_ASSERT_OFFSET(HumanGround, currentMvtType, 0x7B0);
AC2_ASSERT_OFFSET(HumanGround, intentionMvtType, 0x7B4);
AC2_ASSERT_OFFSET(HumanGround, currentSpeedRatio, 0x7BC);
AC2_ASSERT_OFFSET(HumanGround, intentionSpeedRatio, 0x7C0);
AC2_ASSERT_OFFSET(HumanGround, turnAngle, 0x7D8);
AC2_ASSERT_OFFSET(HumanGround, headingBeforeRotationUpdate, 0x7E0);
AC2_ASSERT_OFFSET(HumanGround, slowDownCurve, 0x81C);
AC2_ASSERT_OFFSET(HumanGround, speedControlType, 0x8A8);
AC2_ASSERT_OFFSET(HumanGround, alternateSprintBlendFactor, 0x8B0);
AC2_ASSERT_OFFSET(HumanGround, headingMinRotationSpeed, 0x8B8);
AC2_ASSERT_OFFSET(HumanGround, headingMaxRotationAngle, 0x8C4);
AC2_ASSERT_OFFSET(HumanGround, headingRotationDirection, 0x8D0);
AC2_ASSERT_OFFSET(HumanGround, turnOnMovementMinAngle, 0x990);
AC2_ASSERT_OFFSET(HumanGround, transitionToMotionTypeInternal, 0x994);
AC2_ASSERT_OFFSET(HumanGround, doubleTransitionToMotionSpeed, 0x998);
AC2_ASSERT_OFFSET(HumanGround, overrideMoveCyclebySubState, 0x8B4);

// HumanGround::groundData points here: the reflected class HumanGroundData (descriptor PC 0x1D1C528, size 0x3A0,
// parent HumanGroundDatav2; names from RE/tools/refl.py), then the unreflected members found in the code.
struct HumanGroundData {
    enum HumanGroundSubState : std::int32_t {
        SubState_Movement = 0, SubState_Fight = 1, SubState_FreeRun = 2, SubState_OrientedMove = 3, SubState_Hurt = 4,
        SubState_ObstacleCollision = 5,
    };
    enum MvtDivisions : std::int32_t {
        MvtDivision_WaitLow = 0, MvtDivision_WaitHigh = 1, MvtDivision_WalkVerySlow = 2, MvtDivision_Walk = 3,
        MvtDivision_Jog = 4, MvtDivision_Run = 5, MvtDivision_Crouch = 6, MvtDivision_MAX = 7, MvtDivision_Invalid = 8,
    };
    enum CombatStyle : std::int32_t { CombatStyle_Normal = 0, CombatStyle_Assassin = 1, CombatStyle_MAX = 2 };
    enum ObstacleLeanType : std::int32_t { ObstacleLeanType_Hands = 0, ObstacleLeanType_Feet = 1 };

    AC2_PAD(0x10);                                // 0x000
    float destGlobalBodyOrientation[4];           // 0x010  DestGlobalBodyOrientation
    float destHeading[4];                         // 0x020  DestHeading
    float collidePosition[4];                     // 0x030  CollidePosition
    float collideNormal[4];                       // 0x040  CollideNormal
    float lcoActorSight[4];                       // 0x050  LCOActorSight
    float lcoPelvisSight[4];                      // 0x060  LCOPelvisSight
    float lcoFootSight[4];                        // 0x070  LCOFootSight
    HumanGroundSubState subState;                 // 0x080  SubState
    float curSpeedRatio;                          // 0x084  CurSpeedRatio
    float destSpeedRatio;                         // 0x088  DestSpeedRatio
    float headingLinearRotationSpeed;             // 0x08C  HeadingLinearRotationSpeed
    float headingEasedOutRotationSpeedRatio;      // 0x090  HeadingEasedOutRotationSpeedRatio
    std::uint32_t paramFlags;                     // 0x094  ParamFlags
    std::uint32_t internalFlags;                  // 0x098  InternalFlags
    std::uint32_t entityToAvoid;                  // 0x09C  EntityToAvoid (Handle<Entity>)
    CombatStyle combatStyle;                      // 0x0A0  CombatStyle
    std::uint8_t carryObjectData[0x10];           // 0x0A4  CarryObjectData (HumanGroundCarryObjectData, 0x10)
    float currentBodyAngle;                       // 0x0B4  CurrentBodyAngle
    float currentBodyBankingAngle;                // 0x0B8  CurrentBodyBankingAngle
    ObstacleLeanType obstacleLeanType;            // 0x0BC  ObstacleLeanType
    float collideHeight;                          // 0x0C0  CollideHeight
    std::uint32_t collideEntity;                  // 0x0C4  CollideEntity (Handle<Entity>)
    float blockCylinderRadius;                    // 0x0C8  BlockCylinderRadius
    float blockCylinderLength;                    // 0x0CC  BlockCylinderLength
    float blockCylinderOffset[4];                 // 0x0D0  BlockCylinderOffset
    std::uint32_t customActionControlChannel;     // 0x0E0  CustomActionControlChannel
    std::uint32_t customActionSecondaryChannel;   // 0x0E4  CustomActionSecondaryChannel
    std::uint32_t idleBreakerActionBlock;         // 0x0E8  IdleBreakerActionBlock (Handle<ActionBlock>)
    std::uint8_t isSet;                           // 0x0EC  IsSet
    std::uint8_t breakHard;                       // 0x0ED  BreakHard (IHumanGround::SetBreakHard 0x632E20)
    std::uint8_t sprint;                          // 0x0EE  Sprint (IHumanGround::SetSprint 0x632E80)
    std::uint8_t forceFallFromGrabThrow;          // 0x0EF  ForceFallFromGrabThrow
    std::uint8_t forceRemainStandingOnAnyWallImpact;  // 0x0F0  ForceRemainStandingOnAnyWallImpact
    std::uint8_t actingPlaying;                   // 0x0F1  ActingPlaying
    std::uint8_t actingInterrupted;               // 0x0F2  ActingInterrupted
    std::uint8_t crouch;                          // 0x0F3  Crouch (IHumanGround::SetCrouch)
    // Unreflected members, named after what they do.
    AC2_PAD(0x150 - 0xF4);                        // 0x0F4
    std::int32_t humanGroundStage;                // 0x150  m_HumanGroundStage (IHumanGround::HumanGroundStage)
    std::uint8_t nbMvtTypes;                      // 0x154  m_NbMvtTypes (2)
    std::uint8_t nbMvtDivisions[2];               // 0x155  m_NbMvtDivisions (Low / High)
    AC2_PAD(0x01);                                // 0x157
    float mvtDivThreshold[2][5];                  // 0x158  m_MvtDivThreshold; GetMovementDivisionFromSpeedRatio 0x90E190
    std::uint8_t curMvtType;                      // 0x180  m_CurMvtType
    std::uint8_t destMvtType;                     // 0x181  m_DestMvtType: 1 while high profile is held
    AC2_PAD(0x1A0 - 0x182);                       // 0x182
    std::int32_t destMovementState;               // 0x1A0  m_DestMovementState (MovementState); EnterMove 0x60C610
    AC2_PAD(0x21C - 0x1A4);                       // 0x1A4  (+0x1A8 m_DestTransition, +0x204 m_IsCustomTransitionSet,
                                                  //  +0x20C m_DestTransitionFlags, +0x210 m_DestFeetPos, +0x214 m_DestBlendFactors)
    float rotationRatio;                          // 0x21C  SetRotationRatio 0x90E250
    float rotationRatioStep;                      // 0x220  per second
    std::uint8_t skipActionOnStateEntry;          // 0x224  m_SkipActionOnStateEntry
    AC2_PAD(0x03);                                // 0x225
    std::int32_t transitionToMotionType;          // 0x228  -> HumanGround speed restriction mode in EnterMoveCycle
    std::uint8_t teleportToGroundSetup;           // 0x22C  m_TeleportToGroundSetup
    AC2_PAD(0x250 - 0x22D);                       // 0x22D  (+0x230 m_BlockFallOffEdgeTimer, Revelations)
    std::int32_t entryState;                      // 0x250  m_EntryState (HumanGroundData::EntryState)
    AC2_PAD(0x3A0 - 0x254);                       // 0x254
};

AC2_ASSERT_SIZE(HumanGroundData, 0x3A0);
AC2_ASSERT_OFFSET(HumanGroundData, destGlobalBodyOrientation, 0x010);
AC2_ASSERT_OFFSET(HumanGroundData, destHeading, 0x020);
AC2_ASSERT_OFFSET(HumanGroundData, lcoFootSight, 0x070);
AC2_ASSERT_OFFSET(HumanGroundData, subState, 0x080);
AC2_ASSERT_OFFSET(HumanGroundData, destSpeedRatio, 0x088);
AC2_ASSERT_OFFSET(HumanGroundData, internalFlags, 0x098);
AC2_ASSERT_OFFSET(HumanGroundData, combatStyle, 0x0A0);
AC2_ASSERT_OFFSET(HumanGroundData, currentBodyAngle, 0x0B4);
AC2_ASSERT_OFFSET(HumanGroundData, collideEntity, 0x0C4);
AC2_ASSERT_OFFSET(HumanGroundData, blockCylinderOffset, 0x0D0);
AC2_ASSERT_OFFSET(HumanGroundData, idleBreakerActionBlock, 0x0E8);
AC2_ASSERT_OFFSET(HumanGroundData, breakHard, 0x0ED);
AC2_ASSERT_OFFSET(HumanGroundData, sprint, 0x0EE);
AC2_ASSERT_OFFSET(HumanGroundData, crouch, 0x0F3);
AC2_ASSERT_OFFSET(HumanGroundData, humanGroundStage, 0x150);
AC2_ASSERT_OFFSET(HumanGroundData, nbMvtTypes, 0x154);
AC2_ASSERT_OFFSET(HumanGroundData, nbMvtDivisions, 0x155);
AC2_ASSERT_OFFSET(HumanGroundData, curMvtType, 0x180);
AC2_ASSERT_OFFSET(HumanGroundData, mvtDivThreshold, 0x158);
AC2_ASSERT_OFFSET(HumanGroundData, destMvtType, 0x181);
AC2_ASSERT_OFFSET(HumanGroundData, destMovementState, 0x1A0);
AC2_ASSERT_OFFSET(HumanGroundData, rotationRatio, 0x21C);
AC2_ASSERT_OFFSET(HumanGroundData, transitionToMotionType, 0x228);

}  // namespace ac2sdk
