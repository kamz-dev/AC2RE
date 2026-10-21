#pragma once
// The per-context tuning data. In the game these are reflected classes loaded from the data files
// (Mac: scimitar::ac2::HumanGroundData_ClassData::m_Properties; PC: HumanInAirData::FastLoad 0x90C4D0; class name
// strings at 0x19E5FC4 HumanGroundData, 0x19E5D54 HumanInAirData). Field names come from the reflection data once
// recovered; the port then loads the values from the owner's install.

namespace scimitar::ac2 {

// The four gaits have name strings in the PC exe (0x19E4C14 Sprint, 0x19E4C28 Run, 0x19E4C38 Jog, 0x19E4C48 Walk).
struct HumanGroundData {
    // PORT: placeholder speeds (m/s), not from the game.
    float walkSpeed = 1.5f;
    float jogSpeed = 2.5f;
    float runSpeed = 3.5f;
    float sprintSpeed = 6.0f;
    // PORT: placeholder; how far below the feet still counts as ground, metres.
    float stepDownHeight = 0.3f;
};

struct HumanInAirData {
    // PORT: placeholder gravity (m/s^2), not from the game.
    float gravity = 9.81f;
};

}  // namespace scimitar::ac2
