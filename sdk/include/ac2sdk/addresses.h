#pragma once
// Function and data addresses in the pinned PC build (AssassinsCreedIIGame.exe, sha256 f4983add...; see
// RE/01_triage.md). Numbers are as the PC IDB shows them: the IDB is rebased to image base 0, so each is an RVA and
// the run-time address is the module base + the number.
//
// One line per address: the name as in the PC IDB (Class__Method or scimitar::...), the Mac symbol it was matched
// from when there is one, and its status: "named" (named in the IDB, not checked further), "static" (checked in
// the decompiler), or "live <date>" (confirmed in the running game).

#include <cstdint>

namespace ac2sdk::addr {

// scimitar::ac2::Human
inline constexpr std::uint32_t Human_Update = 0x933480;      // Mac __ZN8scimitar3ac25Human6UpdateEv 0x12EDA06; named
inline constexpr std::uint32_t Human_ApplyDeath = 0x929650;  // named

// scimitar::ac2::HumanGround (RE/11_ground.md)
inline constexpr std::uint32_t HumanGround_vftable = 0x19BFCD8;                // static (only xref of LinkFSM)
inline constexpr std::uint32_t HumanGround_ctor = 0x6317A0;                    // static (writes the vftable)
inline constexpr std::uint32_t HumanGround_Update = 0x63B2C0;                  // slot 14, Mac 0x1544B74; static
inline constexpr std::uint32_t HumanGround_LinkFSM = 0x602270;                 // slot 15, Mac 0xFA4130; static
inline constexpr std::uint32_t HumanGround_UpdateHumanGround = 0x6381D0;       // Mac 0xFED468; static
inline constexpr std::uint32_t HumanGround_UpdateMove = 0x634D20;              // Mac 0xFE069C; static
inline constexpr std::uint32_t HumanGround_UpdateMoveCycle = 0x621B90;         // Mac 0xFE05A2; static
inline constexpr std::uint32_t HumanGround_UpdateMoveCycleMain = 0x61BB60;     // Mac 0xFCD20A; static
inline constexpr std::uint32_t HumanGround_LocalInit = 0x61F920;               // slot 2, Mac 0xFA71B0; static
inline constexpr std::uint32_t HumanGround_UpdateMoveMain = 0x604990;          // Mac 0xFCA184; static
inline constexpr std::uint32_t HumanGround_UpdateMovementIntention = 0x5E03E0; // Mac 0xF98C54; static
inline constexpr std::uint32_t HumanGround_UpdateTurnAngle = 0x5FE6A0;         // Mac 0xFB34C4; static
inline constexpr std::uint32_t HumanGround_UpdateOrientation = 0x5FFC40;       // Mac 0xFB4E32; static
inline constexpr std::uint32_t HumanGround_TestWaitToMoveCycleMain = 0x5F4E70; // Mac 0xF9EAE6; static
inline constexpr std::uint32_t HumanGround_TestMoveCycleToWaitMain = 0x5EAB90; // Mac 0xFB5D7A; static
inline constexpr std::uint32_t HumanGround_IHumanGround_vftable = 0x19BF390;   // at +0x14 (ctor); static
inline constexpr std::uint32_t RotateVectorToVectorWithAngularSpeed = 0x5FF930;  // Mac 0xFB4A3C; static
inline constexpr std::uint32_t HumanGroundData_GetMovementDivisionFromSpeedRatio = 0x90E190;  // Mac 0x1119F44

// Context data
inline constexpr std::uint32_t HumanInAirData_FastLoad = 0x90C4D0;  // named

// Engine and world
inline constexpr std::uint32_t Engine_BeginFrame = 0x227F0;  // scimitar::Engine::BeginFrame; named
inline constexpr std::uint32_t World_GetPlayer = 0x3A300;    // scimitar::World::GetPlayer; named

}  // namespace ac2sdk::addr
