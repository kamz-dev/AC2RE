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

// Context data
inline constexpr std::uint32_t HumanInAirData_FastLoad = 0x90C4D0;  // named

// Engine and world
inline constexpr std::uint32_t Engine_BeginFrame = 0x227F0;  // scimitar::Engine::BeginFrame; named
inline constexpr std::uint32_t World_GetPlayer = 0x3A300;    // scimitar::World::GetPlayer; named

}  // namespace ac2sdk::addr
