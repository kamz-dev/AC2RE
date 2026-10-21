# Roadmap

**Finish line:** every movement context in scope is rebuilt from its decoded FSM with the game's names, rules and
data; every `PORT:` marker is settled or explained; every live row is answered; replays of lab recordings match the
game (or have an evidenced triage row); and the owner plays with no new bug.

**Claiming:** before deep work on an item, put your name and branch in its **Owner** cell in a small PR. One owner
per item. Release it (clear the cell) if you stop.

Status: `open` / `claimed` / `partial` / `done`.

## P0: Foundations
| Item | Owner | Status | Notes |
|---|---|---|---|
| Repo, rules, IDB annotation sync, C++ skeleton, SDK skeleton | owner | done | 2026-10-21 |
| Pin the exe, triage | owner | partial | `01_triage.md`; `triage.py` not yet run on the owner's exe |
| Symbol sources and the matching method | | partial | `02_symbol_sources.md` |
| Reflection: descriptor format, property names, every `Human*Data` field | | open | → `22_reflection.md`; feeds `port/src/scimitar/ac2/human_data.h` |
| Match `Human`, `HumanContextBundle` and its getters on PC; the context switch | | open | → `10_human_update.md` |
| The FSM runtime: what `LinkFSM` builds, how Test / Exec / Enter / Exit run | | open | → `00_overview.md`; feeds `port/src/scimitar/fsm.h` |
| Data loading: the archive format of AC2 (forge), reading the `*Data` objects from the install | | open | AC1 readers in `reference/ac1/re_tools/` (forge v25) as templates |

## P1: The lab mod
| Item | Owner | Status | Notes |
|---|---|---|---|
| Launcher + DLL injection, D3D9 overlay, bridge (port from AC1) | | open | `livelab/README.md` |
| Player hook (`Human::Update`), recorder (`frames.csv`), pad recording / injection | | open | needs P0 Human match and live row 3 |
| Probes, check pages, clock control, pad scripts, courses | | open | |

## P2: Contexts (one RE doc + port module + sim tests each)
| Context | Owner | Status | RE doc |
|---|---|---|---|
| Ground (locomotion only) | kamz-dev (`kamz-dev/ground-re`, `kamz-dev/ground-transitions`) | partial | `11_ground.md`: class, vftable, FSM tree, MoveCycle speed model, Wait / MoveCycle transitions, turning done 2026-10-21; MoveStop, turn states, landings, jumps, edges open |
| InAir (jumps, falls, catches, landings) | | open | `12_in_air.md` |
| Ledge + Climb | | open | `13_ledge_climb.md` |
| NarrowObject (beams, poles) | | open | `14_narrow_object.md` |
| Walling (wall runs) | | open | `15_walling.md` |
| Ladder | | open | `16_ladder.md` |
| HayStack + Kiosk | | open | `17_haystack_kiosk.md` |
| Swim | | open | `18_swim.md` |
| Guidance (climbable geometry data) | | open | `19_guidance.md` |
| Input: pad → interpreter → move direction | | open | `21_input.md` |

## P3: Replay comparison, triage and fixes
As in AC1 (docs/AC1_WORKFLOW.md, section 4): record in the game, replay in the port's sim, find the first
divergence, triage, fix in groups. No recording gets worse unless explained.

## PORT census
Every `PORT:` marker in `port/`, with its class: engine adaptation, outside movement, data convention, checked
against the game, or still to decode. Regenerate the list with `git grep -n "PORT:" port/`.

| Where | What | Class |
|---|---|---|
| `scimitar/ac2/human_data.h` | `PortGroundSpeeds`: ratio → m/s stand-in for MoveCycle's root motion; step-down; gravity | to decode (animations) |
| `scimitar/ac2/human_ground.cpp` | no action system: the "free" part of the transition tests, the item flags and the restriction-mode branch are skipped; MoveStop / TurnStart / TurnOnSpot missing; moves along the facing at stand-in speeds; LocalInit called from the ctor; direct switch to InAir | to decode (Ground FSM, 11_ground.md 9; animations) |
| `scimitar/ac2/human_ground.cpp` (turning) | opposite vectors turn counter-clockwise; the sign of `Gear::GetAngle` | to decode |
| `scimitar/ac2/human_data.h` | `PortHeadingSettings`: the player's heading rotation values; initial rotation ratio / step | live row 6 |
| `scimitar/ac2/human.cpp` (input) | HumanGroundData filled straight from the input | live rows 3, 5 |
| `scimitar/ac2/human_in_air.cpp` | ballistic fall, no catches | to decode (InAir FSM) |
| `scimitar/ac2/human.cpp` / `human.h` | context switching; Update's arguments; position / velocity stand-ins | to decode (P0) |
| `scimitar/ac2/human_context.h` | context id order; `ActorContext` has only Enter / Exit / Update | to decode (P0) |
| `scimitar/math.h` | vector type and yaw convention | live row 1 |
| `port/sim.h` | 30 Hz tick | live row 2 |
| `port/input.h` | input arrives as a world direction | engine adaptation until `21_input.md` |

## Owner decisions
| Date | Decision |
|---|---|
| 2026-10-21 | The port mirrors the game as closely as possible (names, structure, rules, layouts), to serve as an SDK. |
| 2026-10-21 | Port member fields use `m_` (the game's name when known, else a descriptive one with its PC offset in a comment). |
| 2026-10-21 | C++ / CMake. One repository for RE, SDK and port; IDBs and game files stay out of git. |
