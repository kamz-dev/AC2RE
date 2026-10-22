#pragma once
// Player input as the port hands it to the Human. PORT: the game reads it through its pad proxy and action
// interpreter (AC1: GoAssassinActionInterpreter::ReadInput turns the stick into a world move direction through the
// camera); the port takes the world direction directly, as the lab recordings store it.

#include <cstdint>

#include "scimitar/math.h"

namespace port {

// AC-style "puppeteer" buttons. AssassinTask reads Pad::PadButton 0 (legs) and 13 (high profile; RE/21_input.md);
// LIVE row 3: the pad proxy fields.
enum Button : std::uint32_t {
    kHighProfile = 1u << 0,  // RT / right mouse: the run; with legs, the sprint
    kLegs = 1u << 1,         // A / space: the fast walk; jump, free-run
    kHand = 1u << 2,         // B / Shift (empty hand): Pad::PadButton 3, the grasp (live RE/20 row 13)
    kHead = 1u << 3,         // Y / eagle vision
};

struct MoveInput {
    scimitar::Vec3 moveDir;  // world-space move direction, unit length or zero
    float moveMag = 0;       // stick magnitude 0..1
    // The camera's heading (yaw of its forward, scimitar::forwardFromYaw). PadInput's stick-space rules (the snap to
    // forward, the angle curve) work relative to it. PORT: stands in for CameraProxy::RemapPadInput's camera.
    float cameraYaw = 0;
    std::uint32_t buttons = 0;

    bool held(Button b) const { return (buttons & b) != 0; }
};

}  // namespace port
