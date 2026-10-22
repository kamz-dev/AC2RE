# 22: Reflection

Scimitar describes its data classes at run time: each reflected class has a class descriptor (name, hash, parent,
size, a property list, enums), and the property names are stored as hashes. With the names recovered, these give
the game's own names and offsets for the `*Data` classes the movement code works on.

Status 2026-10-21: the descriptor formats are known on PC; `RE/tools/refl.py` dumps any class by name with its
property and enum names. Done: `HumanGroundData` (`11_ground.md` 15). Next: `HumanInAirData`, `HumanLedgeData`,
`HumanClimbData`, `HumanNarrowObjectData`, `HumanWallingData`, `HumanLadderData`, `HumanHayStackData`,
`HumanKioskData`, `HumanSwimData`, `HumanDataBundle`, `HumanData`.

## The hash list
Property names are hashed with the standard zlib CRC-32 of the name. The owner has a list of the strings the game
hashes (about 364,000 lines). It is **not in the repository**; point the tool at your copy with
`AC2_HASHES=<path>` (or `--hashes <path>`). Without it the tool prints hashes.

## Usage
```
set AC2_EXE=<install>\AssassinsCreedIIGame.exe       (the pinned build; see 01_triage.md)
set AC2_HASHES=<path to the hash list>
py RE/tools/refl.py HumanGroundData                  (plain listing)
py RE/tools/refl.py HumanGroundData --md             (a Markdown table for the RE docs)
```

## Formats (PC)
**Class descriptor** (in `.data`; found through the pointer to its name string, which sits at +0x20):

| Offset | Content |
|---|---|
| +0x00 / +0x04 | property descriptors / count |
| +0x08 / +0x0C | enum descriptors / count |
| +0x18 / +0x1C | group names (`char*`) / count |
| +0x20 | `char*` class name |
| +0x24 | class hash |
| +0x28 | parent class hash (`HumanGroundData`'s parent: `HumanGroundDatav2`) |
| +0x2C | size in bytes |

**Property descriptor**, 32 bytes (36 on Mac): +0x00 flags, +0x04 name hash, +0x08 type hash (the class or enum of a
struct / enum member), +0x0E type code, +0x0F element type (arrays), +0x10 high word = **offset × 4**.

Type codes seen so far (from the member sizes): 0x00 bool, 0x06 u32, 0x07 u32 flags, 0x0A float, 0x0D vec4,
0x12 handle, 0x13 struct, 0x19 enum.

**Enum descriptor**, 16 bytes: +0x00 values, +0x04 count, +0x08 hash, +0x0C `char*` name. **Enum value**, 12 bytes:
`char*` name, value, name hash. Enum value names are plain strings, so they need no cracking.

## Example: HumanGroundData
Descriptor PC 0x1D1C528, class hash 0xE00AE315, size 0x3A0, 36 properties, 4 enums: see `11_ground.md` 15. Its
property list is at PC 0x1D1BED8; the Mac copy is `HumanGroundData_ClassData::m_Properties` 0x2C6C000.
