#pragma once
// The contexts' data. In the game each context's data is a reflected class that also holds the context's runtime
// state (RE/11_ground.md 4 and 15): HumanGround reads its inputs from HumanGroundData (PC: HumanGround +0x10). The
// reflected members keep their reflected names (RE/tools/refl.py); the rest are named after what they do, with
// their PC offset. The layout is in sdk/include/ac2sdk/human_ground.h.

#include <cstdint>

#include "scimitar/math.h"

namespace scimitar::ac2 {

// Reflected class HumanGroundData (descriptor PC 0x1D1C528, size 0x3A0, parent HumanGroundDatav2).
struct HumanGroundData {
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
    bool m_BreakHard = false;                      // +0xED IHumanGround::SetBreakHard; while running -> MoveStop (LIVE row 7)
    bool m_Sprint = false;                         // +0xEE IHumanGround::SetSprint; +0.2 to the target ratio (LIVE row 5)
    bool m_ForceFallFromGrabThrow = false;         // +0xEF
    bool m_ForceRemainStandingOnAnyWallImpact = false;  // +0xF0
    bool m_ActingPlaying = false;                  // +0xF1
    bool m_ActingInterrupted = false;              // +0xF2
    bool m_Crouch = false;                         // +0xF3 IHumanGround::SetCrouch

    // Unreflected members (named after what they do).
    // +0x155 / +0x158: the movement-division tables, [0] without and [1] with high profile. HumanGround::LocalInit
    // (PC 0x61F920) fills them; these defaults are its values.
    unsigned char m_MovementDivisionCount[2] = {3, 3};
    float m_MovementDivisionThresholds[2][5] = {{0.0f, 0.9f, 1.0f}, {0.0f, 0.9f, 1.0f}};
    bool m_HighProfileRequest = false;  // +0x181 (LIVE row 5)
    // +0x1A0 HumanGroundData::MovementState: how HumanGround::EnterMove starts (RE/11_ground.md 11). Value names
    // (hypothesis, from ActingClip::ExitMovementState): 0 WaitLow, 1 WaitHigh, 2 WalkVerySlow, 3 Walk, 4/5 Jog/Run,
    // 6 Sprint; 7 crouch, 10 Landing, 11 LandingHard, 12 LandingBalanceFall, 13 HandPassOverReception, 14 Wait (the
    // value ResetTransitionData leaves).
    int m_MovementState = 14;
    float m_RotationRatio = 1.0f;       // +0x21C scales the heading rotation angles; PORT: initial value not seen
    float m_RotationRatioStep = 0.5f;   // +0x220 per second; PORT: initial value not seen (ExitMove sets 1 / 0.5)
    int m_TransitionToMotionType = 1;   // +0x228 EnterMoveCycle copies it into the speed restriction mode

    // Mac 0x1119F44, PC 0x90E190: the first index below the count whose threshold is >= value, else 0.
    int GetMovementDivisionFromSpeedRatio(bool highProfile, float value) const {
        const int h = highProfile ? 1 : 0;
        for (int i = 0; i < m_MovementDivisionCount[h]; ++i) {
            if (m_MovementDivisionThresholds[h][i] >= value) return i;
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
    int movementState = 14;
    float rotationRatio = 0;
    float rotationRatioStep = 0;
    int transitionToMotionType = 1;

    // Mac 0x111AAC6.
    void SetupTransitionData(HumanGroundData& data) const {
        data.m_MovementState = movementState;
        data.m_RotationRatio = rotationRatio;
        data.m_RotationRatioStep = rotationRatioStep;
        data.m_TransitionToMotionType = transitionToMotionType;
    }
    // Mac 0x1119FE2.
    static void ResetTransitionData(HumanGroundData& data) {
        data.m_MovementState = 14;
        data.m_TransitionToMotionType = 1;
    }
};

// PORT: ground speed in the game is the root motion of the MoveCycle animations blended by the speed ratio
// (RE/11_ground.md 5). Until those animations are decoded, the port maps the ratio to m/s through these stand-in
// speeds at ratio 0, 0.2, 0.4, 0.6, 0.8 and 1.0 (linear in between).
struct PortGroundSpeeds {
    float atRatio[6] = {0.0f, 1.2f, 1.8f, 3.0f, 4.5f, 6.0f};
    float crouchMove = 1.0f;  // PORT: CrouchMove's speed (its animation's root motion)
    // PORT: how far below the feet still counts as ground, metres. Not decoded.
    float stepDownHeight = 0.3f;
};

// PORT: the player's controller sets HumanGround's heading rotation through IHumanGround (RE/11_ground.md 7); its
// values aren't known yet (LIVE row 6). These stand-ins make the port turn briskly: about 170 deg/s for small
// corrections, up to about 520 deg/s for a quarter turn or more.
struct PortHeadingSettings {
    float minRotationSpeed = 3.0f;    // rad/s
    float maxRotationSpeed = 9.0f;    // rad/s
    float minRotationAngle = 0.3f;    // rad
    float maxRotationAngle = 1.5708f; // rad
};

// PORT: the lengths of the animation-driven ground states' actions (TurnStart, MoveStop, RunTurn), which the game
// takes from the animations (g_TurnStartActionMap etc., RE/11_ground.md 10). Stand-ins until they are loaded.
struct PortActionLengths {
    float turnStart = 0.5f;
    float moveStop = 0.4f;
    float runTurn = 0.6f;
    float landing = 0.5f;
    float landingHard = 0.9f;
    float landingBalanceFall = 1.2f;
    float roll = 0.8f;
    float crouchTurn = 0.6f;
    float turnOnSpot[4] = {0.4f, 0.6f, 0.8f, 1.0f};  // by GetTurnAnimation's size (< 25, 70, 135 degrees, more)
};

struct HumanInAirData {
    // PORT: placeholder gravity (m/s^2), not from the game.
    float gravity = 9.81f;
};

}  // namespace scimitar::ac2
