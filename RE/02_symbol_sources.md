# 02: Symbol sources: from Mac and Revelations to PC

The PC build has almost no RTTI and few names. Two other builds have names:

| Build | IDB | Symbols | Relation to the target |
|---|---|---|---|
| AC2 Mac | `IDB/scimitar_sf.i64` (`ac2mac`) | Partial: Itanium-mangled names like `__ZN8scimitar3ac211HumanGround7LinkFSMEv`, many methods, no types | **Same game**, same code, different compiler and platform. |
| AC Revelations, Xbox 360 | `IDB/scimitar_final_ltcg.xex.i64` (`acrxbox`) + `scimitar_final_ltcg.pdb` | Full: names, types, struct layouts, enums | **A later game on the same engine.** PowerPC, big-endian, 32-bit, link-time code generation (heavy inlining). |

## What the Mac build shows already (2026-10-21)
- Namespaces: engine code in `scimitar::`, game code in `scimitar::ac2::` (the PC IDB already uses the same,
  e.g. `scimitar::ac2::Human::Update` 0x933480).
- `scimitar::ac2::Human::HumanContextBundle` (ctor Mac 0x12C374E, dtor 0x12C4360) has one getter per context:
  GetGround 0x12C4950, GetInAir 0x12C490C, GetLedge 0x12C48EA, GetClimb 0x12C48C8, GetWalling 0x12C48A6,
  GetLadder 0x12C492E, GetNarrowObject 0x12C46A8, GetHayStack 0x12C4774, GetKiosk 0x12C47B8, GetSwim 0x12C4686,
  GetRiding 0x12C4884, GetBoat 0x12C4862, GetGliderPilot 0x12C4840, GetCart 0x12C481E, GetWagon 0x12C47FC,
  GetCannonOperator 0x12C47DA, GetCarriedBody 0x12C46EC, GetDead 0x12C4796, GetRagdollGround 0x12C470E,
  GetLookAt 0x12C4752, GetVocalization 0x12C46CA, GetUpperBody 0x12C815E, GetDebugContext 0x12C4730.
  Some contexts are created on demand: `CreateContextOnDemandHelper<HumanSwim>` 0x161DC08.
- **The contexts are state machines.** `HumanGround::LinkFSM` 0xFA4130 (0x8D9 bytes) builds the machine; states
  have `Enter<State>` / `Exit<State>` methods (`EnterFrozen`, `ExitPush`, `ExitTurnOnSpotStep`, ...); transitions
  are `Test<T>(const FSMParam*, FSMParam*)` / `Exec<T>(const FSMParam*)` pairs, some named after interface commands
  (`HumanGroundHurtState::TestFallToInAirOnHumanGroundInterfaceCommand_FallOffEdge`). Sub-machines are their own
  classes (`HumanGroundHurtState`, `HumanGroundDodgeState`, ... each with `LinkFSM`).
- `scimitar::ac2::IHumanGround` (ctor 0x153BBE2, `CanRoll` 0xEFE534) is the interface others use to reach the
  ground context (`ClimbClip::UpdateGroundContext(Entity*, IHumanGround*)` 0xD97E12).
- `HumanGround::Update(long)` 0x1544B74; `Human::Update()` 0x12EDA06; `HumanGroundData_ClassData::m_Properties`
  (reflection property tables have symbols).

## Carrying a name to PC
Addresses never carry over. Match the function, then name it on PC with a comment citing the source
(`Mac 0x12EDA06`).
1. **Strings.** A function that references a unique string (an assert, a class name, a property name) on Mac
   references the same string on PC. Find the string on PC, follow its xrefs.
2. **Reflection.** The `Human*Data` class-name strings and the `*_ClassData::m_Properties` tables tie each data
   class to its loader (`FastLoad`) and its property list on both builds.
3. **Call shape.** Once a few anchors are matched, walk callers and callees: the same call order, the same
   constants (floats, enum values, vftable slot offsets) and the same branch structure.
4. **Vftables.** Without RTTI, find PC vftables from the constructors of matched classes, then match slots in
   order against the Mac vtable for the same class.
5. Tools such as Diaphora or BinDiff can propose matches between the Mac and PC IDBs; check every proposal by hand
   before naming.

From Revelations, take **names, types and layouts as leads**: struct definitions and enums from the PDB tell you
what a PC structure probably holds, but its offsets, sizes and code differ (another game, another platform).
Verify on the PC decompilation before anything goes in `sdk/`.

**Reading the PDB:** IDA imported most types but not all (e.g. `scimitar::ac2::HumanGround` is only a forward
declaration in the IDB), and llvm-pdbutil refuses this PDB. Use `py RE/tools/pdbtypes.py "<full name>"` for a
class's members and bases or an enum's values, and `--find <text>` to search type names.

**Matching members to AC2** (how HumanGround and HumanGroundData were named, 2026-10-21): the movement members keep
their order from AC2 to Revelations, with a block offset (HumanGroundData: +0x20 after 0x150; HumanGround: +0x150
around +0x464, +0x18C to +0x200 later on, because Revelations added members in between). A run of neighbouring
members whose types and AC2 uses agree one for one is the evidence; the PDB's enums then have to agree with the
values the PC code uses (e.g. `TransitionToMotionType` with the speed caps). Members Revelations added (combat,
bombs, the apple) have no AC2 counterpart; leave AC2 fields with no clear partner named after what they do.

## Where things go
- Names, comments, prototypes and types: in the PC IDB, shared through `IDB/annotations/` (`CLAUDE.md`).
- Layouts confirmed on PC: `sdk/include/ac2sdk/` with offset checks; addresses in `addresses.h`.
- The behaviour: the subsystem's RE doc, then the port.
