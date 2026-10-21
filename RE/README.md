# RE: findings index

Every claim cites its address (PC first). Guesses are marked **(hypothesis)**. Add each new doc to this table.

| Doc | What | Status |
|---|---|---|
| [ROADMAP.md](ROADMAP.md) | Phases, items, who has claimed what, the PORT census | live |
| [00_overview.md](00_overview.md) | Architecture of AC2's movement: Human, the context bundle, the context FSMs, data | skeleton |
| [01_triage.md](01_triage.md) | The pinned PC build: hashes, sections, imports, RTTI, the IDB's state | started 2026-10-21 |
| [02_symbol_sources.md](02_symbol_sources.md) | Getting names from the Mac and Revelations builds onto the PC build | started 2026-10-21 |
| [11_ground.md](11_ground.md) | HumanGround: class, vftable, FSM tree, MoveCycle speed model, Wait / MoveCycle transitions, turning | partial 2026-10-21 |
| [21_input.md](21_input.md) | The player controller (`AssassinTask`) and its interface to HumanGround | started 2026-10-21 |
| [22_reflection.md](22_reflection.md) | Reflection: class / property / enum descriptors, `refl.py`, the hash list | started 2026-10-21 |
| [20_live_checks.md](20_live_checks.md) | Numbered questions only the running game can answer | 6 open (2 half answered) |

Planned, one per subsystem (claim them in the roadmap): `10_human_update.md` (Human::Update and the context
switch), `12_in_air.md`, `13_ledge_climb.md`, `14_narrow_object.md`, `15_walling.md`,
`16_ladder.md`, `17_haystack_kiosk.md`, `18_swim.md`, `19_guidance.md` (the climbable-geometry data),
`21_input.md` (pad → action interpreter → move direction), `23_animation.md` (the parts movement depends on).
