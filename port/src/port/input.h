#pragma once
// Player input as the port hands it to the Human. PORT: the game reads it through its pad proxy and action
// interpreter (AC1: GoAssassinActionInterpreter::ReadInput turns the stick into a world move direction through the
// camera); the port takes the world direction directly, as the lab recordings store it.

#include <cstdint>

#include "scimitar/math.h"

namespace port {

// AC-style "puppeteer" buttons. LIVE: map them to AC2's pad proxy fields once decoded.
enum Button : std::uint32_t {
    kHighProfile = 1u << 0,  // RT / right mouse
    kLegs = 1u << 1,         // A / space: jump, free-run
    kHand = 1u << 2,         // B / grab, drop
    kHead = 1u << 3,         // Y / eagle vision
};

struct MoveInput {
    scimitar::Vec3 moveDir;  // world-space move direction, unit length or zero
    float moveMag = 0;       // stick magnitude 0..1
    std::uint32_t buttons = 0;

    bool held(Button b) const { return (buttons & b) != 0; }
};

}  // namespace port
