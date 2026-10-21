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
    AC2_PAD(0x6F9 - 0x2AD);         // 0x2AD
    std::uint8_t walkingOnHorse;    // 0x6F9  _UpdateMove 0x634D20
    AC2_PAD(0x790 - 0x6FA);         // 0x6FA
    std::uint32_t upperBody;        // 0x790  HumanUpperBody* (hypothesis)
    AC2_PAD(0x7A8 - 0x794);         // 0x794
    std::int32_t gaitLevel;         // 0x7A8  1 / 2, UpdateMoveCycleMain 0x61BB60
    AC2_PAD(0x04);                  // 0x7AC
    std::int32_t running;           // 0x7B0  ratio above 0.4
    std::int32_t highProfile;       // 0x7B4  high-profile intention
    AC2_PAD(0x04);                  // 0x7B8
    float speedRatio;               // 0x7BC  0..1, drives the MoveCycle blend
    float aiSpeedRatio;             // 0x7C0
    AC2_PAD(0x8A8 - 0x7C4);         // 0x7C4
    std::int32_t aiSpeedOverride;   // 0x8A8
    float blendSlot12Weight;        // 0x8AC
    float blendSlot19Weight;        // 0x8B0
    AC2_PAD(0x994 - 0x8B4);         // 0x8B4
    std::int32_t speedRestriction;  // 0x994  mode 0..7
};

AC2_ASSERT_OFFSET(HumanGround, actor, 0x008);
AC2_ASSERT_OFFSET(HumanGround, groundData, 0x010);
AC2_ASSERT_OFFSET(HumanGround, fsmCurrentState, 0x042);
AC2_ASSERT_OFFSET(HumanGround, fsmStates, 0x08C);
AC2_ASSERT_OFFSET(HumanGround, walkingOnHorse, 0x6F9);
AC2_ASSERT_OFFSET(HumanGround, upperBody, 0x790);
AC2_ASSERT_OFFSET(HumanGround, gaitLevel, 0x7A8);
AC2_ASSERT_OFFSET(HumanGround, running, 0x7B0);
AC2_ASSERT_OFFSET(HumanGround, highProfile, 0x7B4);
AC2_ASSERT_OFFSET(HumanGround, speedRatio, 0x7BC);
AC2_ASSERT_OFFSET(HumanGround, aiSpeedRatio, 0x7C0);
AC2_ASSERT_OFFSET(HumanGround, aiSpeedOverride, 0x8A8);
AC2_ASSERT_OFFSET(HumanGround, blendSlot19Weight, 0x8B0);
AC2_ASSERT_OFFSET(HumanGround, speedRestriction, 0x994);

// HumanGround::groundData points here. Offsets read in UpdateMoveCycleMain on both builds (RE/11_ground.md 4).
struct HumanGroundData {
    AC2_PAD(0x10);                 // 0x000
    float direction[4];            // 0x010  zeroed after use
    float moveDirection[4];        // 0x020  (hypothesis)
    AC2_PAD(0x80 - 0x30);          // 0x030
    std::int32_t movementMode;     // 0x080  3 = oriented move
    AC2_PAD(0x04);                 // 0x084
    float stickMagnitude;          // 0x088  (hypothesis)
    AC2_PAD(0xB4 - 0x8C);          // 0x08C
    float turnAngle;               // 0x0B4
    float turnAngle2;              // 0x0B8
    AC2_PAD(0xEE - 0xBC);          // 0x0BC
    std::uint8_t speedBandUp;      // 0x0EE  +0.2 to the target speed ratio
};

AC2_ASSERT_OFFSET(HumanGroundData, direction, 0x010);
AC2_ASSERT_OFFSET(HumanGroundData, movementMode, 0x080);
AC2_ASSERT_OFFSET(HumanGroundData, stickMagnitude, 0x088);
AC2_ASSERT_OFFSET(HumanGroundData, turnAngle, 0x0B4);
AC2_ASSERT_OFFSET(HumanGroundData, speedBandUp, 0x0EE);

}  // namespace ac2sdk
