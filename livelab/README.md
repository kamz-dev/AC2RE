# livelab: the AC2 live-analysis mod (to be built)

The AC1 lab (`reference/ac1/livelab/`, plain C) is the template: a launcher starts the game and injects a 32-bit
DLL. The DLL hooks the player's update, the pad and the frame, then draws a D3D9 overlay, records `frames.csv`,
runs int3 probes without a debugger, controls the clock, plays pad scripts and serves a localhost bridge for
`lab.py`. Read `reference/ac1/livelab/README.md` and `docs/AC1_WORKFLOW.md` (section 3) first.

## Compatibility with AC2Tweaker
The owner always runs the game with their own mod, **AC2Tweaker** (a separate repository, a 32-bit DLL that patches
code inline with 5-byte call / jmp detours). Every hook the lab adds must coexist with it: list the addresses
AC2Tweaker patches (its sources use `GameModule + 0x...`) and never patch the same or overlapping bytes. One example:
AC2Tweaker detours inside `Human::Update` at 0x93359F. Prefer untouched sites or vtable slots, or add the hook to
AC2Tweaker itself. Read-only external readers (`peek.py`) need no coordination.

## Porting it to AC2
- Write it in C++ (32-bit, `vs2022-x86`), and take every address and layout from `sdk/include/ac2sdk/`. The AC1
  addresses in `reference/ac1/livelab/src/` are all wrong for AC2.
- **Addresses are RVAs** (the PC IDB is rebased to 0): resolve each at run time as
  `GetModuleHandle(nullptr) + addr`.
- AC2 PC renders with D3D9 like AC1, so the overlay, the GDI font atlas and the `Present` / `Reset` hooks carry over.
- What it needs from P0 (`RE/ROADMAP.md`): the PC `Human::Update` and how to pick out the player
  (`scimitar::World::GetPlayer` 0x3A300 is a lead), the entity's position / velocity, the current context, the pad
  proxy (live row 3), and the frame clock (`scimitar::Engine::BeginFrame` 0x227F0).
- Nothing in the game folder changes; everything lives in memory.
- Sessions, recordings, screenshots and save backups stay out of git (`.gitignore`).

Start with the launcher, the injection and the overlay showing the player's position; then the recorder.

## Until then: the read-only reader
`peek.py` reads the running game's memory with `ReadProcessMemory` (no injection, no writes), and `record.py`
records the player's HumanGround, HumanGroundData and entity at ~100+ Hz:
```
py livelab/record.py 100 %TEMP%
ec.jsonl
```
How the player is found is in `RE/20_live_checks.md` (session 1). It can't hook, probe or control time, so
questions about who calls a function (row 7) still need the mod.
