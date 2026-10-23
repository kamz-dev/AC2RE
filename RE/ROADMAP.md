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
| Match `Human`, `HumanContextBundle` and its getters on PC; the context switch | kamz-dev (`kamz-dev/context-switch`) | partial | `10_human_update.md`: the tracks, the frame's phases and `AIActor::TransitToContext` (immediate Exit → Exec → Enter) decoded and ported (2026-10-22); the bundle's slot order and context ids done (live row 9); the controller runs before `Human::Update` (`WorldTasks::SetupUpdateAI`, 2026-10-22); left: the transitions' Execs |
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
| Ground (locomotion only) | kamz-dev (`kamz-dev/ground-re`, PR #1) | partial | `11_ground.md` sections 1–16: every movement state of HumanGround decoded and matched (2026-10-21); edge clamping (Human, 16) decoded and ported, the edge commands (LookDown, EdgeStop, FallOffEdge, LedgePullDown, 17) decoded and checked live (2026-10-22); 17 ported with stand-ins (no Ledge context yet); `HumanGroundData` property names (with 22_reflection), the outside drivers (controller → `21_input.md`, free-run / guidance → `19_guidance.md`) |
| InAir (jumps, falls, catches, landings) | | open | `12_in_air.md` |
| Ledge + Climb | kamz-dev (`kamz-dev/ledge`: Ledge first) | partial | `13_ledge_climb.md`: HumanLedge's class, FSM (29 states, Mac → PC), data, entry, the pull-down, the hang, release and pull-up, and the player's ledge controller decoded (2026-10-23); the pull-down, hang, release and pull-up ported with live timings (row 15). Next: moving along the ledge (anchor points, guidance), the other states, Climb |
| NarrowObject (beams, poles) | | open | `14_narrow_object.md` |
| Walling (wall runs) | | open | `15_walling.md` |
| Ladder | | open | `16_ladder.md` |
| HayStack + Kiosk | | open | `17_haystack_kiosk.md` |
| Swim | | open | `18_swim.md` |
| Guidance (climbable geometry data) | kamz-dev (`kamz-dev/guidance`) | partial | `19_guidance.md`: the data on entities (packed objects, compressed points, normals, EdgeFilter), contact objects, CastWorld, the sphere and box zones in contact and cut modes decoded and ported (2026-10-23). The anchor points, the hang positions and the depth detector / occluded report followed (2026-10-23). Next: the toe probe (GuidanceWorkspaceCastZPlanes), CastPullDownGuidanceReport in full and the Ledge pull-down on anchors, GetNextHorizontalLimbPos, capsule / plane / mesh zones, jump targets, loading from the install |
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
| `scimitar/ac2/human_ground.cpp` | no action system: `PortAction` stands in for the actions (stand-in lengths in `PortActionLengths`), "interruptible by movement" is never true, item flags are skipped, the move cycle's entry action lasts a measured 0.68 s; Idle / HandPassOverReception and the jump / obstacle states missing (no free-run system sends their commands); the edge commands (EdgeStop, LookDown, FallOffEdge, LedgePullDown) with their clearance tests passing, no upper body for LookDown, EdgeStop measured live (0.94 s back to Wait, the pull-down after the 0.27 s unbalance, a 0.75 m/s step back), the pull-down handing over to Ledge, and a 0.38 m support radius standing in for the character capsule; InAir picks the landing as the game does but without spring jumps, fall damage or feet (front foot 1), and its jump origin is a stand-in (the landing speed ratio comes from `AssassinTask::UpdateInAir`); collision only through `World::capsuleIsClear` (BoxWorld; no character capsule height); TurnOnSpotStep never chains (see TurnOnSpot); moves along the facing at stand-in speeds and spreads turn / run-turn animations' rotation; LocalInit called from the ctor; the switch to InAir when the ground drops away (without the Main transitions' tests and Execs) | to decode (11_ground.md 9; animations) |
| `scimitar/ac2/human_ground.cpp` (turning) | opposite vectors turn counter-clockwise (`RotateVectorToVectorWithAngularSpeed`'s side; `Gear::GetAngle`'s sign is decoded, 2026-10-22) | to decode |
| `scimitar/guidance_depth.cpp`, `port/world.h` (collision meshes), `scimitar/ac2/anchor_point.cpp` | the depth detector slices every triangle of the meshes the world's AABB test returns (no MOPP query, no layer-5 filter); BoxWorld's boxes as closed meshes and its floor as a 2000 m square; the anchors' occlusion capsule, ladder steps and a given report's RefineReport aren't ported; a ladder's bottom-most normal is the lower face's | to decode (19_guidance.md Open) |
| `scimitar/guidance.cpp`, `port/world.h` (BoxWorld guidance) | the broadphase is the world's AABB test (no MultiInertComponent split, no entity children); components carry their entity's (orthonormal) matrix; the component vftable +0xC4 check passes; PortAddPoint rounds to 5 mm; BoxWorld's components (top edges, LedgeGrab, WallGround filter); the box zone's AABB is its corners' | to decode / load from the install (19_guidance.md Open) |
| `port/world.h`, `scimitar/ac2/edge_clamping.cpp`, `human.cpp` (edge clamping) | the guidance casts are the game's (`19_guidance.md`), but BoxWorld's guidance is its boxes' top edges (LedgeGrab, WallGround filter; flush edges kept) and the broadphase is its own AABB test; `GetDepthInFrontOfEdge` is decoded, but its ray casts are BoxWorld's (`World::rayCast`: static boxes and the floor, every layer, no hit from inside a box); no water (`GetWaterLevel` lowest float); the root motion is a world-space stand-in, moved by the ground's after-physics phase | to decode (11_ground.md 16 Open) |
| `scimitar/ac2/human_ledge.cpp`, `human_data.h` (`PortLedgeMoves`), `assassin_task.cpp` (UpdateLedge) | the pull-down, hang and pull-up move the root linearly between live positions over live durations (no anchor points, DisplacementManager or animations); the wall / free hang is a ray at the wall 1.2 m down (GetToesDestAnchors not decoded); the hand anchors are always found; HangEntry is instant; Wait holds still (UpdateWaitMain not ported); CanRelease / CanPullup are true in Movement (IsActionInterruptibleByRelease, EvaluatePullupFromMovement not decoded); ReleaseToInAir is a still start of the fall; Pullup holds 0.27 s for NarrowObject and enters Ground in a standing wait; the other 23 states, the capsule / layers / IK, the root transitions and the after-physics phase aren't ported; the controller's horizon is the sight's right and its stick angle reads the pad stick (+0x1030, hypothesis); no leap of faith below, rebound, assassinations, swings or pass-over | to decode (13_ledge_climb.md Open) |
| `scimitar/ac2/human_ground.cpp` TurnOnSpot | No custom actions (`IsInCustomAction` false) and no action channel 1, so TurnOnSpotStep never chains; interface command 155 reduced to the Wait → TurnOnSpot transition | the action system and upper body aren't ported |
| `scimitar/ac2/human_data.h` / `human.cpp` | The heading rotation angles (`kPortHeadingNormal`, live values; their setter isn't decoded); initial rotation ratio / step; TurnStart's length 0.39 s and the move cycle entry action 0.68 s (measured; `PortIsMoveCycleActionCurrent` only knows the TurnStart → MoveCycle case) | live row 8 |
| `scimitar/ac2/assassin_task.cpp`, `scimitar/pad_input.h` | The stick thresholds as constants (live values); `CameraProxy::RemapPadInput` as a rotation by `MoveInput::cameraYaw`; UpdateGround's handlers skipped; no crowds, carrying, praying, walls, group formations or first person; abilities all allowed; `OnSprintBegin` skipped; only the ground update; PC 0xDF8C2D–0xDF941E (beam probe, spring jump), the targeting settings, the free-run edge jump and the run-off jump not ported; `ButtonJustPressedWithDelay`'s delay ignored (the grasp, pad button 3) | `21_input.md` open list |
| `scimitar/ac2/human_in_air.cpp` | ballistic fall, no catches; always hands over a plain Landing (MovementState 10) | to decode (InAir FSM, LandingType) |
| `scimitar/ac2/human.cpp` / `human.h`, `scimitar/ai_actor.*` | Update's arguments (input, world, dt passed in); the main track's after-physics update runs inside `Human::Update` (the port's physics step); only the main track has contexts; `TransitToContext` to a context the port lacks does nothing; `ForceContext` for tests and non-ground spawns; position / velocity stand-ins | engine adaptation (10_human_update.md Port) |
| `scimitar/ac2/human_context.h` | `ActorContext` has only GetID / Enter / Exit / Update(phase) | engine adaptation |
| `scimitar/math.h` | vector type and yaw convention | live row 1 |
| `port/sim.h` | 30 Hz tick | live row 2 |
| `port/input.h` | input arrives as a world direction | engine adaptation until `21_input.md` |

## Owner decisions
| Date | Decision |
|---|---|
| 2026-10-21 | The port mirrors the game as closely as possible (names, structure, rules, layouts), to serve as an SDK. |
| 2026-10-21 | Port member fields use `m_` (the game's name when known, else a descriptive one with its PC offset in a comment). |
| 2026-10-21 | C++ / CMake. One repository for RE, SDK and port; IDBs and game files stay out of git. |
