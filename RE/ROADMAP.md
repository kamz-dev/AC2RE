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
| Reflection: descriptor format, property names, every `Human*Data` field | | partial | `22_reflection.md`, `RE/tools/refl.py`; HumanGroundData done 2026-10-21, the other `Human*Data` classes next |
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
| Ground (locomotion only) | kamz-dev (`kamz-dev/ground-re`, PR #1) | partial | `11_ground.md` sections 1–16: every movement state of HumanGround decoded and matched (2026-10-21); edge clamping (Human, 16) decoded and ported, the edge commands (LookDown, EdgeStop, FallOffEdge, LedgePullDown, 17) decoded and checked live (2026-10-22); left: porting 17, `HumanGroundData` property names (with 22_reflection), the outside drivers (controller → `21_input.md`, free-run / guidance → `19_guidance.md`) |
| InAir (jumps, falls, catches, landings) | | open | `12_in_air.md` |
| Ledge + Climb | | open | `13_ledge_climb.md` |
| NarrowObject (beams, poles) | | open | `14_narrow_object.md` |
| Walling (wall runs) | | open | `15_walling.md` |
| Ladder | | open | `16_ladder.md` |
| HayStack + Kiosk | | open | `17_haystack_kiosk.md` |
| Swim | | open | `18_swim.md` |
| Guidance (climbable geometry data) | | open | `19_guidance.md` |
| Input: pad → interpreter → move direction | kamz-dev (`kamz-dev/ground-re`, PR #1) | partial | `21_input.md`: the ground controller (`AssassinTask::UpdateGroundMovement` PC 0xDF7FE0) decoded and ported (2026-10-21), its gaps and edge clamping (2026-10-22); left: PC 0xDF8C2D–0xDF941E (ladders, beams, spring jump), the edge-jump decision, the other contexts' controllers |

## P3: Replay comparison, triage and fixes
As in AC1 (docs/AC1_WORKFLOW.md, section 4): record in the game, replay in the port's sim, find the first
divergence, triage, fix in groups. No recording gets worse unless explained.

## PORT census
Every `PORT:` marker in `port/`, with its class: engine adaptation, outside movement, data convention, checked
against the game, or still to decode. Regenerate the list with `git grep -n "PORT:" port/`.

| Where | What | Class |
|---|---|---|
| `scimitar/ac2/human_data.h` | `PortGroundSpeeds`: ratio → m/s stand-in for MoveCycle's root motion; step-down; gravity | to decode (animations) |
| `scimitar/ac2/human_ground.cpp` | no action system: `PortAction` stands in for the actions (stand-in lengths in `PortActionLengths`), "interruptible by movement" is never true, item flags are skipped, the move cycle's entry action lasts a measured 0.68 s; Idle / HandPassOverReception and the jump / edge / obstacle states missing (no free-run system sends their commands); InAir picks the landing as the game does but without spring jumps, fall damage or feet (front foot 1), and its jump origin is a stand-in (the landing speed ratio comes from `AssassinTask::UpdateInAir`); collision only through `World::capsuleIsClear` (BoxWorld; no character capsule height); TurnOnSpotStep never chains (see TurnOnSpot); moves along the facing at stand-in speeds and spreads turn / run-turn animations' rotation; LocalInit called from the ctor; direct switch to InAir | to decode (11_ground.md 9; animations) |
| `scimitar/ac2/human_ground.cpp` (turning) | opposite vectors turn counter-clockwise (`RotateVectorToVectorWithAngularSpeed`'s side; `Gear::GetAngle`'s sign is decoded, 2026-10-22) | to decode |
| `port/world.h`, `scimitar/ac2/edge_clamping.cpp`, `human.cpp` (edge clamping) | guidance lines are BoxWorld's box top edges (flush edges kept); which lines a zone catches is the port's own overlap test (`CastEntityInternal` not decoded); `GetDepthInFrontOfEdge` measures the ground 0.07 m past the edge; no water (`GetWaterLevel` lowest float); the root motion is a world-space stand-in, moved by the ground's after-physics phase | to decode (11_ground.md 16 Open) |
| `scimitar/ac2/human_ground.cpp` TurnOnSpot | No custom actions (`IsInCustomAction` false) and no action channel 1, so TurnOnSpotStep never chains; interface command 155 reduced to the Wait → TurnOnSpot transition | the action system and upper body aren't ported |
| `scimitar/ac2/human_data.h` / `human.cpp` | The heading rotation angles (`kPortHeadingNormal`, live values; their setter isn't decoded); initial rotation ratio / step; TurnStart's length 0.39 s and the move cycle entry action 0.68 s (measured; `PortIsMoveCycleActionCurrent` only knows the TurnStart → MoveCycle case) | live row 8 |
| `scimitar/ac2/assassin_task.cpp`, `scimitar/pad_input.h` | The stick thresholds as constants (live values); `CameraProxy::RemapPadInput` as a rotation by `MoveInput::cameraYaw`; UpdateGround's handlers skipped; no crowds, carrying, praying, walls, group formations or first person; abilities all allowed; `OnSprintBegin` skipped; only the ground update; PC 0xDF8C2D–0xDF941E (beam probe, spring jump), the targeting settings and the edge-jump decision not ported | `21_input.md` open list |
| `scimitar/ac2/human_in_air.cpp` | ballistic fall, no catches; always hands over a plain Landing (MovementState 10) | to decode (InAir FSM, LandingType) |
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
