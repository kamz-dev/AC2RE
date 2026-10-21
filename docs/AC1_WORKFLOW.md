> **Reading this for AC2.** This is the AC1 project's write-up, kept as it was. In this repository its
> `tools/livelab/` is `reference/ac1/livelab/`, its `tools/re/` is `reference/ac1/re_tools/` (the generic readers
> are adapted in `RE/tools/`), and its Rust/Bevy port is our C++ `port/`. AC2 differs where the AC1 text relies on
> full RTTI (AC2 PC has almost none: names come from the Mac and Revelations symbols, `RE/02_symbol_sources.md`) and
> on AC1 offsets and data formats. The rules in `CLAUDE.md` win over anything here.

# How the AC1 project works

A rundown of the workflow, the techniques and the tools we built for reversing Altaïr's movement in Assassin's Creed
(2008) and recreating it in Rust/Bevy. Most of it carries over to AC2.

The recreation is public, so you can read how the RE turned into code: github.com/Banned445/AC1-Movement-Rewritten.
It has the movement contexts, the animation runtime, IK, the guidance queries, the streamed Damascus, and the replay
harness that the lab tools drive.

## 1. Static reverse engineering
1. **Triage the exe.**
   - AC1 was not packed: normal MSVC sections and about 260 imports.
   - It shipped **full RTTI**, about 3,400 class names (`.?AVHuman@scimitar@@` …). That is the map for everything else.
2. **Map the classes.** List every RTTI vftable of the movement classes (`tools/re/vt.py`). In AC1 each movement
   context (`HumanGround`, `HumanInAir`, `HumanLedge`, `HumanClimb`, …) is a class with a shared interface, so the
   vftables give each context's code range and its update / enter / exit / event slots.
3. **Recover the reflection data.**
   - Scimitar keeps class, property and enum descriptors in the exe (`classes.py`, `enums.py`, `refl_index.py`).
   - Property names are hashed with CRC-32. `crack.py` recovers them by dictionary combination.
   - This gives the field names and offsets of every `*Data` struct, the runtime state of each context.
4. **Read the data formats.** `.forge` archives (`forge.py`, LZO1X with `lzo1x.py`), animations (`ac_anim.py`), the
   animation graph (`ac_actions.py`: actions, items, transitions, blends), events (`anim_events.py`) and the collision
   layer matrix (`collision_layers.py`).
   - These are AC1 formats (forge version 25). AC2's will differ; anviltoolkit already covers the archives.
5. **Parallel analysis.**
   - Several agents, each with one subsystem and a shared brief, decompile, rename, comment and each write one report
     (`RE/01_…` to `RE/06_…`).
   - A synthesis pass then cross-checks the reports against each other and writes `RE/00_overview.md`.
6. **Doc rules:**
   - every claim cites its address;
   - guesses are marked **(hypothesis)**;
   - names follow `Class__Method`;
   - `RE/README.md` indexes every doc.

## 2. The recreation (port)
- **Clean-room rules:**
  - the port reads the owner's install at runtime and never ships game data;
  - every rule in the code cites its RE section and address;
  - departures from the game are tagged `PORT:` with the reason.
- **Build it in contexts,** mirroring the game's: Ground, InAir, Ledge, Climb, NarrowObject (beams), Walling, Ladder,
  HayStack, Kiosk (market stalls), PassOver. Each has its own module and its own sim tests.
- **Test it headless.** A simulation harness runs the player with no window (the city loaded, fixed timestep), so
  tests and replays run in CI-like conditions.
- **Debug tools in the port:**
  - F9 bug capture;
  - a guidance overlay;
  - target and catch debug views;
  - `AC_REPLAY=<folder>` to replay a capture;
  - `AC_VIDEO=<file.mp4>` to render a replay straight to ffmpeg for demo videos.

## 3. Live game analysis
### Cheat Engine (the start)
The first live checks used Cheat Engine through an MCP bridge, attached to the running game: reading the player's
context, the action id and positions to confirm the static reading. It works, but it's slow and manual, and the game
pauses when it loses focus.

### The lab mod (`tools/livelab/`)
A DLL injected at start-up by a small launcher. **Nothing on disk in the game folder changes**; every hook lives in
memory. It's what made the live work fast.
- **Hooks:**
  - the player's `PostUpdate` (every Human goes through it, and the player is picked out of them);
  - the pad proxy's update, for recording and injecting input;
  - the action interpreter, for the world move direction and stick;
  - the engine's frame task, for the game clock;
  - D3D9 `Present`, for the overlay.
- **Overlay:** context, previous context, the playing action by name and item word, position, speed, dt, custom
  fields, probe counters and a check page.
- **Recording (F3):** `frames.csv` every frame:
  - position, facing and velocity;
  - context, action and item word;
  - stick and buttons;
  - the six handle bones (head, spine, hands, feet);
  - events and marks.
- **Probes:**
  - int3 at any instruction, with a vectored handler that logs, restores, single-steps and re-arms;
  - no 4-breakpoint limit, and the game never waits on a debugger;
  - each probe has a condition (`when=[ecx+8]==$human`), a hit cap, and fields read with a small expression
    language (`$human`, `$entity`, `$hg+0x5E8:f32`, `vec3`, `act`, `ctx`).
- **Clock control:** pause, step one frame, and play at ×0.5 / ×0.25 / ×0.1 to catch fast moments.
- **Pad scripts:** `stick x y for s`, `hold LEGS`, `goto x y` (steering through the camera), so a move can be repeated
  exactly.
- **Bridge:** a localhost socket (`lab.py`):
  - eval and read memory, name objects by RTTI, list an entity's components;
  - teleport, start and stop recordings, add probes at run time;
  - take screenshots.
- **Check pages:** each open live question gets a numbered row in the live-check list (RE/20, 76 rows in AC1).
  `gen_checks.py` turns those rows into overlay pages, each with steps, probes and fields.
- **Courses** (`course.py`): back to Ground, teleport, settle, arm probes, play a script, record, and print the
  context / action timeline. `rerun.py` plays a recording's own stick and buttons again from the same start.
- **Unattended sessions:** the boot script drives the menus with real keystrokes and loads the latest save. An agent
  can then run whole check lists alone while the owner is away.

Batch the live checks: log every `LIVE:` question during static work, then answer them all in one session.

## 4. The replay lab: comparing the port with the game
The core of finishing the movement.
1. Record moves in the game with the lab: 318 recordings in AC1, labelled by family (`climb_…`, `freerun_…`,
   `beam_…`, `hay…`).
2. `compare.py` replays a recording's input in the port's headless harness from the same start, then compares the
   two timelines frame by frame:
   - the first divergence (position more than 0.5 m apart, or the context different for more than 0.1 s);
   - the first differing action pair.
3. `compare_all.py` runs every recording and writes a summary.
   - `--set quick`: about 70 representative recordings in about 4 minutes, for checks while working.
   - `--chunks 4`: the full run in 4 processes; about 16 minutes at 5–8.5 GB, against 30 minutes at 19 GB in one.
4. `diff_summaries.py` compares two summaries recording by recording: better, worse, and the match count.
   **The rule: no recording gets worse** unless it's explained.
5. **Triage** (`RE/34` in AC1): every diverging recording gets one class and a fix code:
   - (a) a port rule is wrong;
   - (b) a knife-edge under the lab's steering;
   - (c) a lab artifact;
   - (d) a game hitch frame;
   - (e) out of scope.

   The (a) rows become the fix list, grouped into codes (A01, A02, …) with their addresses.
6. **Limb check:** `bone_errors.py` ranks clips by how far the port's hand and foot bones sit from the game's.

In AC1 this took the port from 134 to 196 of 318 recordings matching the game to the end, over a few days of fix
groups.

## 5. The bug pipeline
- The owner plays and presses F9 when something looks wrong. The port writes a capture folder: the whole session's
  input, the last 10 s of state, a screenshot, and a note.
- **A capture with a written note is a report:** replayed and investigated, never asserted. **A template note makes
  it a fixture:** it's replayed in the test suite against a saved baseline, and re-baselined only for intended
  changes, each traced to its commit first.
- **Lesson from AC1:** the recorder must also store map switches and teleports, or a capture can't be replayed on its
  own.

## 6. Planning and tracking
- **The roadmap has a written finish line,** for example: every recording matches or has an evidenced triage row,
  every `PORT:` marker is settled, the checklist is closed, the live rows are answered, and the owner plays with no
  new bug.
- **It runs in phases:**
  - M0: owner test and baseline;
  - M1: bookkeeping;
  - M2: replay triage;
  - M3: fixes in groups;
  - M4: the animation runtime's model;
  - M5: the last live round;
  - M6: sign-off and one engine-agnostic spec index.
- **A generated pipeline page** (`build_pipeline.py` + `pipeline_template.html`) shows every phase and item as done /
  partial / open, rebuilt from a Python list.
- **The `PORT:` marker census:** read every stand-in in the code, classify it (engine adaptation, outside movement,
  data convention, checked against the game, or still to decode), and plan the ones left.
- **Owner decisions are recorded** in the roadmap with their date: what's accepted as explained, and what waits for a
  live check.

## 7. Running agents
- **One routing session plans and checks; each work group runs in its own session.** The brief names:
  - the items, with their addresses and recordings;
  - the branch to cut from;
  - the tests and the replay rule;
  - what to leave alone;
  - the hand-over (PR text, RE doc sections, pipeline update).
- **Medium reasoning effort was enough** for the groups once the briefs were precise.
- **Stacked branches:** each group cuts from the last, and the owner opens the PRs in order.
- **Guard rails that saved us:**
  - one IDA writer at a time;
  - save the IDB after every group (a Windows Update restart once lost a night's comments);
  - one full replay at a time, because two at once ran the PC out of memory;
  - delete build folders and temporary outputs;
  - commit only your own files by path in a shared checkout.
- **Stop rule:** when a fix grows into a system rewrite, the agent stops, writes it up, and asks.

## 8. What's in `tools/`
| Folder | What |
|---|---|
| `tools/livelab/` | the AC1 lab mod (C source, launcher, `build.bat`, example check page, probes and pad scripts) and its Python tools: `lab.py`, `boot.py`, `keys.py`, `course.py`, `rerun.py`, `compare.py`, `compare_all.py`, `diff_summaries.py`, `bone_errors.py`, `gen_checks.py`, `memscan.py`, `camalign.py`, `shots.py`, `bmp2png.py`, plus `sets/quick.txt` |
| `tools/re/` | the static readers: `pe.py`, `vt.py` (RTTI vftables), `calls.py`, `classes.py`, `enums.py`, `refl.py`, `refl_index.py`, `crack.py`, `forge.py`, `forge_find.py`, `forge_inventory.py`, `lzo1x.py`, `ac_anim.py`, `ac_actions.py`, `anim_events.py`, `collision_layers.py`, and the pipeline generator with its template |

All of it carries AC1 v1.02 addresses and AC1 formats. Treat it as a working template, not something to run on AC2
as it is. The ideas port directly; the offsets don't.
