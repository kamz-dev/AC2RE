# AC2RE: Assassin's Creed II movement, reverse engineered and rebuilt in C++

**This is the main instructions file for every contributor's agent** (Claude Code reads it automatically; other
agents are pointed here by `AGENTS.md`). Read it fully at the start of each session, then follow **Read next**.
The rules are adapted from an AC1 project that finished the same job; its write-up is `docs/AC1_WORKFLOW.md`.

## Goal
Reverse engineer Ezio's movement in Assassin's Creed II (PC, `AssassinsCreedIIGame.exe`, the pinned build in
`RE/01_triage.md`), and rebuild it in C++ **as close to the original as possible**. The result is an SDK-grade
recreation:
- the game's own class names, namespaces and structure (`scimitar::ac2::Human`, `HumanContextBundle`,
  `HumanGround`, `IHumanGround`, `HumanGroundData`, ...), taken from the symbols;
- the same states, transitions, conditions, timings, speeds, detection rules and data layouts, with each rule
  tracing to an address;
- data loaded from the owner's own install at run time;
- layout-exact headers of the live game's classes (`sdk/`), which the lab mod also uses.

Movement covers ground locomotion; jumps, falls and landings; ledges and climbing; beams, poles, ladders and ropes
(NarrowObject, Ladder); wall running (Walling); free running; hay stacks and kiosks; swimming; the world's
guidance data that marks climbable geometry; and player input → movement.

This is an educational, personal-use project. Nothing from the game is redistributed: no exe, no data files, no
IDBs in git.

## Layout
| Path | What |
|---|---|
| `IDB/` | The IDA databases. **Not in git** (0.4–0.75 GB each); every contributor gets the same baseline once (`IDB/README.md`). All paths in docs and tools are relative to the repo root: `IDB/<file>.i64`. |
| `IDB/annotations/<idb>/` | **In git.** The shared IDA work (names, comments, prototypes, local types) as text. `tools/ida/` syncs it. |
| `RE/*.md` | Findings. `RE/README.md` is the index, `RE/00_overview.md` the architecture, `RE/ROADMAP.md` the plan. |
| `RE/tools/` | Static Python readers for the PC exe (`paths.py` finds everything; `triage.py`, `pe.py`, `vt.py`, `calls.py`, `lzo1x.py`, `refl.py` for reflected classes with their real names, `pdbtypes.py` for the Revelations PDB's classes and enums). Set `AC2_EXE` to the pinned exe and `AC2_HASHES` to the hash list (not in git; `RE/22_reflection.md`). |
| `tools/ida/` | `annotations.py` (export / import inside IDA) and `sync.py` (the same, headless through idalib). |
| `sdk/` | Layout-exact headers of the game's classes and the address table (`sdk/README.md`). |
| `port/` | The C++ recreation (CMake). `port/src/scimitar/` mirrors the game's classes; `port/src/port/` is the port's own scaffolding (sim harness, test worlds, finding the install). |
| `livelab/` | The AC2 live-analysis mod (32-bit DLL injected into the game), to be built from the AC1 one (`livelab/README.md`). |
| `reference/ac1/` | The AC1 project's lab mod and static tools: **templates with AC1 addresses and formats**. Read them; don't run them on AC2. |
| `docs/` | `SETUP.md` (first-time setup), `REQUIREMENTS.md`, `AC1_WORKFLOW.md`. |

The game install is read-only and never modified. It sits next to the repository (`../Assassin's Creed II`) or at
`AC2_GAME_DIR`. A working copy of the exe may go in `bin/` (ignored by git).

## The three IDBs
| Session id | File | What | Use it for |
|---|---|---|---|
| `ac2pc` | `IDB/AssassinsCreedIIGame.exe.i64` | **The target.** AC2 PC, 32-bit x86, rebased to image base 0 (IDA address = RVA; run time = module base + address). Little RTTI, partly named. | Every address in docs, `sdk/` and the port. |
| `ac2mac` | `IDB/scimitar_sf.i64` | AC2 for Mac, **partial symbols** (Itanium-mangled: `__ZN8scimitar3ac211HumanGround7LinkFSMEv`). | Real class and method names, FSM state names, class structure. Same game, different compiler: match functions to PC by strings, constants and call shape, never by address. |
| `acrxbox` | `IDB/scimitar_final_ltcg.xex.i64` (+ `scimitar_final_ltcg.pdb`) | **AC Revelations**, Xbox 360, **full symbols and types** (PDB). Big-endian PowerPC. | Names, struct and class layouts, enums and engine code the Mac build lacks. A different, later game on the same engine: a lead, not proof. |

Open them with the IDA MCP's `idb_open` using the absolute path of `<repo>/IDB/<file>` and the session ids above.
`RE/02_symbol_sources.md` says how to carry a name from Mac / Xbox to PC.

## Conventions
- **Reflected classes keep their reflected member names** (`py RE/tools/refl.py <Class>`), in the port and the SDK.
  Unreflected members take the Revelations PDB's names when the match is shown (`RE/02_symbol_sources.md`).
- **Names follow the game.** Use the symbol's own name in the PC IDB, in the PC IDB's existing style
  (`scimitar::ac2::Human::Update`). A name taken from the Mac or Xbox build gets a comment with its source
  (`Mac 0x12EDA06`). Scoped but unclear: `scimitar::ac2::HumanGround::sub_DE4F80`. Never invent a name that looks
  like a real symbol; prefix your own with `port_` / `unk_`.
- **Safe IDA edits only:** rename, comment, set_type, declare_type. Never patch, undefine or delete.
- **One IDB writer at a time per machine.** Before another process opens a database: `idb_save`, then `idb_close`.
  Two IDA processes on one database can corrupt it. Save after every group of edits.
  - Closing a session without saving leaves the unpacked `<name>.id0 .id1 .id2 .nam .til` next to the `.i64`. If you
    made no edits and the `.i64`'s timestamp is unchanged, delete them; otherwise close with save.
- **Share IDA work through git, never by sending IDBs.** After every pull: import. Before every commit: export.
  (`py tools/ida/sync.py import|export IDB/<file>.i64`, or `tools/ida/annotations.py` inside a running IDA. Export
  refuses if the repo has entries your IDB lacks: you haven't imported yet.) Only the PC IDB's annotations are
  shared by default; add others when there is work on them worth sharing.
- **Never convert number bases by hand.** Use the IDA `int_convert` tool.
- **Every claim in an RE doc cites the address it came from** (PC address first, Mac / Xbox symbol when it helped).
  Mark guesses **(hypothesis)**. State verified facts plainly, with the date when it matters.
- **Port code mirrors the game:**
  - classes, methods and fields keep the game's names and structure; each class and function cites its PC address
    and the symbol it was named from;
  - member fields use the `m_` prefix (`m_SpeedRatio`). Where the game's name isn't known yet, pick a descriptive
    one and give the field's PC offset in its comment (`// +0x7BC`), so it can be renamed once the reflection or
    symbols give the real name. Port-only helpers that stand in for undecoded code are named `Port...` and tagged
    `PORT:`;
  - a rule is written the way the game computes it (same order, same conditions, same constants), not an
    "equivalent" rewrite, and cites its RE section and address;
  - anything that departs from the game is a stand-in tagged `PORT:` with its reason, and is logged in
    `RE/ROADMAP.md`'s PORT census;
  - a question static code can't answer gets `LIVE:` and a numbered row in `RE/20_live_checks.md`;
  - fields whose layout is known go in `sdk/` with offset checks first; the port then uses them.
- **Tests:** a sim test for every behaviour (`port/tests/`). Placeholders are tested as placeholders and change with
  their RE values. Light runs: `ctest --preset debug`, or `ac2_tests <filter>`.
- **Git:**
  - work on feature branches (`<you>/<topic>`); open PRs to `main`; never force-push `main`;
  - commit only your own files, by path; never commit IDBs, the exe, game data, recordings or screenshots;
  - commit messages end with your agent's own `Co-Authored-By:` line;
  - claim a subsystem before deep work: add your name to its row in `RE/ROADMAP.md` in a small PR, so two people
    don't reverse the same context.

## Build
```
cmake --preset vs2022            # once (Visual Studio 2022 with C++; its bundled CMake works)
cmake --build --preset debug
ctest --preset debug
build/vs2022/port/Debug/ac2sim.exe [--csv out.csv]
```
`vs2022-x86` / `x86-debug` builds 32-bit like the game, so the `sdk/` layout checks are live. Run it after
touching `sdk/`.

## Scope
- Movement only. Combat, NPCs and crowds, the horse, boats, the glider, ragdoll and presentation (sound, visuals,
  look-at) stay out until the owner reopens them. The contexts are listed with their scope in
  `port/src/scimitar/ac2/human_context.h`.
- Work in phases from `RE/ROADMAP.md`. If a fix starts turning into a rewrite of a whole system, stop and ask.
  Fence experimental changes behind one named switch.

## Resources
- One full lab replay at a time; they use a lot of RAM. Keep disk use low: one build folder per checkout, delete
  large temporary outputs once their summary is saved.
- Opening an IDB headless takes a minute and several GB of RAM (the Mac one most). Close sessions you're done with.
- Report outcomes honestly: failing tests, skipped steps and unverified claims are stated plainly.

## Read next
1. `RE/ROADMAP.md`: what's done, what's claimed, what's next.
2. `RE/README.md` and `RE/00_overview.md`: what is known.
3. `RE/02_symbol_sources.md` before any IDA work.
4. `docs/SETUP.md` if this machine isn't set up yet (IDA MCP, IDBs, build).
5. `docs/AC1_WORKFLOW.md` for how the AC1 project ran: static RE, the port, the lab mod, the replay comparison,
   the bug pipeline and running agents. Most of it carries over.
