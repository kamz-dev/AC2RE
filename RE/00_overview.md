# 00: Overview of AC2's movement

A skeleton, to be rewritten by the synthesis pass once the subsystem docs exist (docs/AC1_WORKFLOW.md, section 1,
step 5). What's below is from the symbols (`02_symbol_sources.md`), not yet checked on PC unless an address says so.

## The pieces
- **`scimitar::ac2::Human`** is the character. `Human::Update` (PC 0x933480, Mac 0x12EDA06) runs every frame.
- **`Human::HumanContextBundle`** owns one object per context and hands them out through getters; some are
  created on demand. Exactly one locomotion context is active at a time **(hypothesis, as in AC1)**.
- **Each context is an FSM** built by its `LinkFSM`, with `Enter*` / `Exit*` / `Update*` per state and `Test*` /
  `Exec*` per transition. Contexts hand over through interface commands (e.g. `FallOffEdge` → InAir).
- **Tuning lives in reflected `*Data` classes** (`HumanGroundData`, `HumanInAirData`, ...), loaded from the data
  files (`HumanInAirData::FastLoad` PC 0x90C4D0).
- **Guidance** (`HumanGuidance`) is the world data that marks climbable geometry **(hypothesis, from the name and
  AC1)**.
- **Input** reaches the contexts through the pad proxy and an action interpreter **(hypothesis, as in AC1)**.

## The movement contexts
Ground, InAir, Ledge, Climb, Walling, Ladder, NarrowObject, HayStack, Kiosk, Swim. Their scope and getters are in
`port/src/scimitar/ac2/human_context.h`.

## Open
Everything: see `ROADMAP.md`.
