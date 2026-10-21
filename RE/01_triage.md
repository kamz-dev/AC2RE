# 01: Triage of the pinned PC build

## The build (2026-10-21, from the PC IDB's metadata)
| | |
|---|---|
| File | `AssassinsCreedIIGame.exe` |
| sha256 | `f4983add9f4c366c388f067355fad52e10138aff588c8db7cc1dc4d84c56dafa` |
| md5 | `2dd4d3c196fa270945826d2faab42007` |
| Architecture | 32-bit x86 (PE) |
| IDB image base | **0x0**: the IDB is rebased, so an IDA address is an RVA. Run time: module base + address. `RE/tools/pe.py` reads file VAs; convert with `pe.ida()` / `pe.va()`. |
| Image size | 0x2170000 |

`py RE/tools/triage.py` re-checks the hash of your exe and prints the header's preferred base, sections, DLL names
and RTTI names.

## Sections (IDB)
| Name | Start | End | Perm |
|---|---|---|---|
| .text | 0x1000 | 0x18A5000 | rx |
| .idata | 0x18A5000 | 0x18A54FC | r |
| .rdata | 0x18A54FC | 0x1C6B000 | r |
| .data | 0x1C6B000 | 0x1E68000 | rw |
| .tls | 0x1E68000 | 0x1E69000 | rw |
| .data1 | 0x1E69000 | 0x1E6A000 | rw |

## What the IDB holds (2026-10-21)
- 154,778 functions: 11,612 named, 18,883 library, 124,283 unnamed. 35,768 strings.
- The exe exports Audiokinetic Wwise (`AK::SoundEngine`, `AK::MemoryMgr`, `AK::StreamMgr`, ...) by name; entry
  `start` at 0xED7F6B.
- **RTTI is nearly absent:** the only `.?AV` type names found are MSVC runtime ones (`std::bad_alloc`,
  `std::exception`, `type_info`, ...). Unlike AC1 (about 3,400 RTTI classes), class names must come from the Mac
  and Revelations symbols and from the engine's reflection strings (`02_symbol_sources.md`).
- Some engine functions are already named in the `scimitar::` style, e.g. `scimitar::main` 0x14890,
  `scimitar::Engine::BeginFrame` 0x227F0, `scimitar::World::GetPlayer` 0x3A300, `scimitar::ac2::Human::Update`
  0x933480, `scimitar::ac2::Human::ApplyDeath` 0x929650, `scimitar::HumanInAirData::FastLoad` 0x90C4D0.
  Their source isn't recorded; treat them as unchecked until each is confirmed.
- Reflection class-name strings for the Human contexts' data are in `.rdata`: `HumanGroundData` 0x19E5FC4,
  `HumanInAirData` 0x19E5D54, `HumanLedgeData` 0x19E50BC, `HumanClimbData` 0x19E63AC, `HumanNarrowObjectData`
  0x19E4D74, `HumanWallingData` 0x19E4904, `HumanLadderData` 0x19E57C0, `HumanHayStackData` 0x19E5E3C,
  `HumanKioskData` 0x19E5BD0, `HumanSwimData` 0x19E4B30, `HumanGuidance` 0x19E5FB4, `HumanDataBundle` 0x19E6324,
  `HumanActorContext` 0x19E68B8, plus the out-of-scope ones (Riding, Boat, GliderPilot, Cart, Wagon,
  CannonOperator, CarriedBody, Dead, LookAt, UpperBody, CustomAction, PushableContext, GroundCarryObject).
- Gait strings next to them: Sprint 0x19E4C14, Run 0x19E4C28, Jog 0x19E4C38, Walk 0x19E4C48 (and two more Walk
  variants at 0x19E4C58 / 0x19E4C6C), and `HumanSpeedCameraActivator` 0x19E4BD0.

## Next
- [ ] Find the reflection descriptor format (what references the `Human*Data` strings) → `22_reflection.md`.
- [ ] Match `HumanContextBundle` and its getters to PC from the Mac build → `10_human_update.md`.
- [ ] Confirm the pre-named functions above (decompile and compare with the Mac versions).
