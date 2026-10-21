#pragma once
// The contexts' data. In the game each context's data is a reflected class that also holds the context's runtime
// state (RE/11_ground.md 4): HumanGround reads its inputs from HumanGroundData (PC: HumanGround +0x10). Field names
// are the port's until the reflection names are recovered; each field's comment gives its PC offset, and the
// layout is in sdk/include/ac2sdk/human_ground.h.

#include "scimitar/math.h"

namespace scimitar::ac2 {

struct HumanGroundData {
    Vec3 m_MoveDirection;               // +0x20 desired move direction (hypothesis)
    int m_MovementMode = 0;             // +0x80 3 = oriented move
    float m_StickMagnitude = 0;         // +0x88 0..1 (hypothesis)
    float m_TurnAngle = 0;              // +0xB4
    float m_TurnAngle2 = 0;             // +0xB8
    bool m_SpeedBandUp = false;         // +0xEE raises the target speed ratio by 0.2 (LIVE row 5)
    bool m_CrouchRequest = false;       // +0xF3 (Mac; not yet checked on PC)
    bool m_HighProfileRequest = false;  // +0x181 (Mac; not yet checked on PC) (LIVE row 5)
};

// PORT: ground speed in the game is the root motion of the MoveCycle animations blended by the speed ratio
// (RE/11_ground.md 5). Until those animations are decoded, the port maps the ratio to m/s through these stand-in
// speeds at ratio 0, 0.2, 0.4, 0.6, 0.8 and 1.0 (linear in between).
struct PortGroundSpeeds {
    float atRatio[6] = {0.0f, 1.2f, 1.8f, 3.0f, 4.5f, 6.0f};
    // PORT: how far below the feet still counts as ground, metres. Not decoded.
    float stepDownHeight = 0.3f;
};

struct HumanInAirData {
    // PORT: placeholder gravity (m/s^2), not from the game.
    float gravity = 9.81f;
};

}  // namespace scimitar::ac2
