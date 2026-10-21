#pragma once
// scimitar::ac2::AssassinTask: the player's controller (RE/21_input.md). The game runs it as a task with one FSM state
// per Human context (Mac LinkFSM 0x104FFE6); each frame PreUpdate (Mac 0x106C922) runs UpdateGlobal (Mac 0x106BDE2,
// the pad reading) and then the current context's update, which drives the context through its interface. The port
// has the ground part: UpdateGround (PC 0xE049A0, Mac 0x1082F62) -> UpdateGroundMovement.
//
// AI humans are driven through the same interfaces by the navigation services instead.

#include "scimitar/math.h"

namespace port {
struct MoveInput;
}

namespace scimitar::ac2 {

class Human;
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

    // PC 0xDF7FE0, Mac 0x107F0EC. The decoded part: break hard, the forced turn-around, the speed ratio, the heading
    // rotation speeds, the direction, sprint and the movement type (RE/21_input.md). The rest of the function (from
    // PC 0xDF8C2D: ladders and interactions) isn't decoded.
    void UpdateGroundMovement(IHumanGround& ground, const Vec3& sight, float dt);

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
    float m_LastInputLength = 0;       // +0xEC0 the stick length
    float m_LastInputAngle = 0;        // +0xEC4 sight -> m_LastRelativeDirection, kept while the stick is in the dead zone
    Vec3 m_LastRelativeDirection;      // +0xED0 the camera-relative stick, normalised
    bool m_InPreciseTargetingMode = false;  // +0xEE0 written by UpdateGroundMovement: button 0 without high profile
    float m_LastDampedInputAngle = 0;  // +0xEE4 sight -> m_LastDampedRelativeDirection, kept like m_LastInputAngle
    Vec3 m_LastDampedRelativeDirection;  // +0xEF0 the damped camera-relative stick

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

    // PORT: the ControlScheme's stick band (+0x7D4) and last frame's (+0x7D6), and the pad buttons the port has.
    int m_PortStickBand = 0;
    int m_PortPreviousStickBand = 0;
    bool m_PortButton0 = false;  // Pad button 0
    // +0x968 (Mac +0x964) a scimitar::Timer: the forced turn-around lasts at most 0.3 s. PORT: the task's own clock.
    float m_ForceTurnAroundTimerEnd = 0;
    float m_PortClock = 0;
};

}  // namespace scimitar::ac2
