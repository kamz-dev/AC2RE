#pragma once
// scimitar::ac2::AssassinTask: the player's controller (RE/21_input.md). The game runs it as a task with one FSM state
// per Human context (Mac LinkFSM 0x104FFE6); each frame PreUpdate (Mac 0x106C922) runs UpdateGlobal (Mac 0x106BDE2,
// the pad reading) and then the current context's update, which drives the context through its interface. The port
// has the ground part: UpdateGround (PC 0xE049A0, Mac 0x1082F62) -> UpdateGroundMovement.
//
// AI humans are driven through the same interfaces by the navigation services instead.

#include "scimitar/ac2/edge_clamping.h"
#include "scimitar/math.h"
#include "scimitar/pad_input.h"

namespace port {
struct MoveInput;
}

namespace scimitar::ac2 {

class Human;
class HumanInAir;
class IHumanLedge;
class IHuman;
class IHumanGround;

// scimitar::ac2::AssassinAbilitySet (the mission's ability set; AssassinAbilities::GetAbility / GetMaxSpeed read it).
struct AssassinAbilitySet {
    // AssassinAbilitySet::MaxSpeed (Revelations PDB; AssassinAbilities::GetMaxSpeed PC 0x6CCC40).
    enum MaxSpeed : int {
        NoMovementAllowed = 0,
        Walk = 1,
        Jog = 2,
        Run = 3,
        Sprint = 4,
        WalkSlowest = 5,
        WalkSlightlySlow = 6,
        WalkSlow = 7,
    };
};

class AssassinTask {
public:
    // PORT: the task's frame. The game's PreUpdate / UpdateGlobal read the pad and the camera themselves; the port
    // takes the camera-relative stick from MoveInput (port::UpdatePortInput) and runs the ground update while the
    // Human is in its Ground context.
    void Update(const port::MoveInput& input, Human& human, float dt);

    // PC 0xDF7FE0, Mac 0x107F0EC. The decoded parts: break hard, the forced turn-around, the speed ratio, the heading
    // rotation speeds, the direction, sprint and the movement type, then the gaps and edge clamping (PC 0xDF941E;
    // RE/21_input.md). Between them (PC 0xDF8C2D..: ladders, interactions, beams, the spring jump) isn't ported, nor
    // the edge-jump decision after it. `position` is the entity's global position.
    void UpdateGroundMovement(IHuman& human, IHumanGround& ground, const Vec3& position, const Vec3& sight, float dt);

    // PC 0xDEC620, Mac 0x105C244: the InAir context's update. The decoded part the port uses: the landing speed
    // ratio, which picks InAir's landing (RE/21_input.md, RE/11_ground.md 11). The grasp, dive, wall reception and
    // side-step landing requests go to parts of InAir the port doesn't have.
    void UpdateInAir(HumanInAir& inAir, const Vec3& sight);

    // Mac 0x1076B30 (PC not located yet): the Ledge context's update (RE/13_ledge_climb.md 8). The decoded part the
    // port uses: the direction and speed setters, the release on the buffered empty-hand press and the pull-up on the
    // stick pushed up. The assassinations, the climb leap, the rebound (back eject), the hand-pass-over jump, the
    // pass-over, the turn-around and the swings go to parts of Ledge the port doesn't have.
    void UpdateLedge(IHumanLedge& ledge, const Vec3& sight);

    // PORT: the ability set's answers (the game reads the mission's AssassinAbilitySet). Defaults: everything allowed.
    bool m_PortAllActions = true;   // AssassinAbilitySet::GetAllActions (PC 0x6CC7C0)
    bool m_PortHighProfile = true;  // AssassinAbilitySet::GetHighProfile
    AssassinAbilitySet::MaxSpeed m_PortMaxSpeed = AssassinAbilitySet::Sprint;

    // Fields (PC offsets; the same on Mac unless noted). Names from Revelations' AssassinTask where its layout lines
    // up (m_LastInputLength .. m_LastDampedRelativeDirection: the same order and spacing, RE/21_input.md).
    float m_StickSpeedRatio = 0;       // +0x8B0 the stick's speed ratio after the dead zone and the max speed
    float m_TurnSlowdownFactor = 1;    // +0x8B8 slows high-profile moves while the stick points away from the sight
    int m_RotationDirection = 0;       // +0x8C0 IHumanGround::RotationDirection (always 0 after UpdateGroundMovement)
    bool m_IsHighProfile = false;      // +0x94C (Revelations name) the high-profile button, from UpdateGlobal
    bool m_ForceTurnAround = false;    // +0x94E
    // +0x94A: the empty hand (pad button 3) was just pressed; set by UpdateGlobal (Mac 0x106C1B0), cleared the next
    // frame (unless ButtonJustPressedWithDelay(0, 0.3)), by a timer (+0xA64; PORT: not ported) and on entering the
    // Ledge, Climb, Ladder and Main states (EnterLedge Mac 0x1058601 ...). The ledge release reads it.
    bool m_EmptyHandJustPressed = false;
    float m_LastInputLength = 0;       // +0xEC0 the stick length
    float m_LastInputAngle = 0;        // +0xEC4 sight -> m_LastRelativeDirection, kept while the stick is in the dead zone
    Vec3 m_LastRelativeDirection;      // +0xED0 the camera-relative stick, normalised
    bool m_InPreciseTargetingMode = false;  // +0xEE0 written by UpdateGroundMovement: button 0 without high profile
    float m_LastDampedInputAngle = 0;  // +0xEE4 sight -> m_LastDampedRelativeDirection, kept like m_LastInputAngle
    Vec3 m_LastDampedRelativeDirection;  // +0xEF0 the damped camera-relative stick

    // +0x80: the ControlScheme's (+0x70) PadInput (+0x10). PORT: only the stick processing.
    PadInput m_PadInput;

    // AssassinTaskGround::GapForStopWalking (Revelations layout): the gap that stops a low-profile walk at an edge.
    struct GapForStopWalking {
        Gap m_Gap;                  // +0x840
        float m_TimeSinceSet = 0;   // +0x880 a scimitar::Timer restarted when it is set. PORT: the start time
        bool m_IsSet = false;       // +0x8A0
    };
    GapForStopWalking m_GapForStopWalking;

private:
    // ControlScheme::JoyThresholdPressed / JoyThresholdJustPressed (stick 0); PC inlines them (PC 0xDF8139..).
    bool JoyThresholdPressed(int band) const { return m_PortStickBand == band; }
    bool JoyThresholdJustPressed(int band) const { return m_PortStickBand == band && m_PortPreviousStickBand != band; }
    bool ButtonPressed(int button) const;
    // PORT: AssassinTask::ReorientDirectionFromWallCollision (PC 0xDF3590) isn't decoded; it returns 0 (no wall) and
    // leaves the direction.
    int PortReorientDirectionFromWallCollision(const Vec3& direction, float, float, const Vec3& target, Vec3& out);
    // PORT: AssassinTask::GetCrowdObstructionFactor (PC 0xDE1720) isn't decoded; the port has no crowds (0).
    float PortGetCrowdObstructionFactor(const Vec3&) const { return 0; }

    // PORT: the stick band (AssassinTask +0x7D4) and last frame's (+0x7D6), and the pad buttons the port has.
    int m_PortStickBand = 0;
    // PORT: the pad stick (stick space: +y is the pad's up), which UpdateLedge measures its angle on (Mac reads
    // AssassinTask +0x1030; what it holds is a hypothesis).
    Vec3 m_PortPadStick;
    // PORT: the Human context the task's FSM was in last frame (the game switches the task's state through
    // GoAssInterfaceCommand_SetCurrentContext and runs its Enter).
    int m_PortTaskContext = -1;
    int m_PortPreviousStickBand = 0;
    bool m_PortButton0 = false;  // Pad button 0
    bool m_PortButton3 = false;  // Pad button 3 (the empty hand, Shift)
    bool m_PortButton3Before = false;
    // ControlScheme::ButtonJustPressedWithDelay(button, delay). PORT: the delay's meaning isn't decoded; a plain
    // just-pressed test.
    bool PortButtonJustPressedWithDelay(int button, float delay) const;
    // +0x968 (Mac +0x964) a scimitar::Timer: the forced turn-around lasts at most 0.3 s. PORT: the task's own clock.
    float m_ForceTurnAroundTimerEnd = 0;
    float m_PortClock = 0;
};

}  // namespace scimitar::ac2
