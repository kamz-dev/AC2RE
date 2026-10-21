# AC1 Lab: the live-analysis mod

A mod for Assassin's Creed (Steam v1.02 b86610, `AssassinsCreed_Dx9.exe`) that records, probes and drives the
running game for the live checks in `RE/20` (#1–67). It is loaded into the game's memory at start-up; **nothing in
the game folder or `bin/` changes**. Hooks, probes and the controls exist only in the running process.

## Build and start

```
livelab\build.bat            (Visual Studio 2022 C++ tools; builds ac1lab.exe + ac1lab.dll, 32-bit)
livelab\ac1lab.exe           (starts the game with the lab; finds ..\Assassin's Creed or AC_GAME_DIR)
py livelab\tools\boot.py     (the same, then title -> profile -> Continue -> "load latest memory")
```

`lab.ini` holds the settings:
- `Windowed`: also load `tools\ac1_windowed` for a movable window.
- `KeepRunningInBackground`.
- `SkipIntro`: skips the start-up videos in memory (EaglePatch's branch 0x405495).
- `UnlockAbilities` / `AbilityMask`: every frame, ORs the mask (hex, default all 34 flags) into each set of the player's
  ability stack and sets HighProfile + AllActions. The test save has not unlocked the air grab (Grasp, bit 6) and six
  other abilities yet (RE/05 §5.7); with this on it can do everything. Memory only (`src/abilities.c`).
- `Overlay`, `Hotkeys` and `BridgePort`.

Your saves were backed up to `livelab/save_backup/2026-10-12/` before the first launch.

## In the game

| Key | Does |
|---|---|
| F1 | overlay on / off |
| F2 / Shift+F2 | next / previous check page (arms that page's probes and fields) |
| F3 | start / stop a recording |
| F4 / Shift+F4 | mark the moment (Shift: "looked wrong") |
| F5 / F6 (Ctrl: slot 2) | save / restore the position (a teleport; from Ground only) |
| F7 / Shift+F7 | speed ×1 → ×0.5 → ×0.25 → ×0.1 |
| F8 | pause (the game clock's scale goes to 0) |
| F10 | step one frame while paused |
| F11 | run the page's script (checks with an automatic version, e.g. #62, #67) |
| F12 | record the pad (a replayable `pad_NNN.txt`) |

The overlay has three parts:
- **Left:** context, previous context, the main slot's action by name and item word, position, speed, dt, the page's
  fields and the probe counters (logged / hits).
- **Right:** the check page.
- **Bottom:** the keys.

A session folder `sessions/<date-time>/` holds:
- `lab.log` and `events.log`, with every probe hit and mark;
- one folder per recording (`NNN_<label>/frames.csv` and `events.log`);
- screenshots and pad recordings.

## A live session

1. `py tools/boot.py` (or start `ac1lab.exe` and load the save yourself).
2. F2 to the check, read its page, then F3 to record and do the move. Press F4 when it happens, and Shift+F4 if it
   looks wrong. Press F3 again to stop.
3. Use F8 / F10 / F7 to catch fast moments. F11 runs a check's script where one exists.
4. Afterwards tell Claude the session folder. The recordings, probe logs and marks are the evidence for closing
   the RE/20 rows.

## Files

| Path | What |
|---|---|
| `src/` | the DLL (C): `abilities.c` the ability unlock, `main.c` hooks, `game.c` player / reads, `expr.c` expressions, `probes.c`, `trace.c`, `checks.c`, `control.c` (keys, clock, commands), `input.c` (pad scripts / recording), `overlay.c`, `bridge.c`, `launcher.c` |
| `checks/NN.txt` | one page per RE/20 live check, made by `tools/gen_checks.py` (edit by hand after deleting its first line) |
| `probes/always.txt` | probes armed on every page |
| `scripts/*.txt` | pad scripts |
| `names/actions.tsv` | action id → name (local, from the port's `probe_dump_action_names`; rebuild below) |
| `tools/lab.py` | command line for the bridge |
| `tools/boot.py` | launch + menus |
| `tools/keys.py` | real keystrokes for the menus |
| `tools/compare.py` | game recording → port replay → `compare.txt` |
| `tools/compare_all.py` | every recording through `compare.py`, the first divergences in `sessions/compare_summary.txt` |
| `tools/gen_checks.py` | RE/20 → check pages |
| `tools/bmp2png.py` | screenshots |
| `tools/course.py` + `courses/*.txt` | a test course: back to Ground, teleport, settle, fields and probes, record a script, print the context / action timeline (`--compare` replays it in the port's Damascus) |
| `tools/memscan.py` | look through an object over the bridge: position copies, RTTI-named pointers, floats |

Rebuild the names (port repo, branch with the probe):
`PROBE_OUT=<livelab>/names/actions.tsv cargo test --release probe_dump_action_names -- --ignored --nocapture`.

## Pages, probes, fields and scripts

A page (`checks/NN.txt`):

```
id: 39
title: Ledge stop at the edge line
script: scripts/xxx.txt                 (optional, F11)
steps:
  what to do / look at (shown on the overlay)
probes:
  0xD93C60 LedgeStop_Enter when=[ecx+8]==$human max=200 feet=$entity+0x40:vec3
fields:
  speed_param=$hg+0x5E8:f32
```

**Probes.** A probe is `address name [when=<expr>] [max=<n>] <field>...`.
- It puts an int3 at the address. A vectored handler logs the hit, puts the byte back, single-steps and re-arms.
- There is no 4-breakpoint limit, and the game never waits on a debugger.
- The address must be an instruction start; check it in IDA first.
- After `max` logged hits the probe takes itself out.

**Expressions** (probes, fields and `eval`):
- Syntax:
  - numbers;
  - registers (probes only: `eax ebx ecx edx esi edi ebp esp`);
  - `[x]` reads the dword at `x`;
  - `+ - == != < > &`.
- Variables: `$human`, `$entity`, `$hdata` (HumanDataBundle), `$ctx` (the current locomotion context), `$hg`
  (HumanGround), `$controller`, `$world`, `$engine`.
- `expr:type` reads that type at the address: `u8 u16 u32 i32 f32 hex vec3 ctx act str bits`.
- Without a type, the expression's value is printed.

**Scripts:**
- `wait s`;
- `stick x y [for s]` (x right, y forward) and `rstick x y [for s]`;
- `hold|release|tap <A X Y B DOWN LEFT UP RIGHT BACK START LT LB RT RB L3 R3 | LEGS HAND HIGH | all>`;
- `goto x y [mag] [radius] [timeout]` (walk / run to a game-space point, blocking) and `dir dx dy [mag] [for s]` (hold
  a world direction): they steer through the camera, learnt from the interpreter's world move direction for a small
  push inside the dead zone;
- any lab command, e.g. `mark`, `rec start`, `scale`, `pause`.

Script time is game time. While a script plays, the keyboard / pad is replaced.

## Bridge (`py tools/lab.py <command>`)

| Command | Does |
|---|---|
| `status` | live state |
| `frame` | current field values |
| `eval <fields>` / `read <addr> <n>` / `rtti <expr>` / `comps [entity]` | read memory, name objects, list an entity's components |
| `write <addr> <hex bytes>` / `writef <addr> <float>` | write memory (logged as an event) |
| `scale <f>` / `pause` / `resume` / `step [n]` | game clock |
| `rec start <label>` / `rec stop` / `mark <text>` | recordings |
| `check <id|next|prev>` / `checks` | pages |
| `probe add <line>` / `probe clear` / `probes` | probes at run time |
| `play <script>` / `stop` / `inputrec on|off` | pad scripts |
| `save <n>` / `load <n>` / `tp x y z [yaw°]` | bookmarks and teleports (game coordinates; from Ground: a hang or haystack context keeps its own anchor) |
| `field add name=expr:type` / `field clear` / `checks reload` | extra per-frame fields; reread the pages |
| `shot [path]` / `shot ui [path]` | the frame as a BMP, without / with the overlay |
| `stacks` / `modules` | where each thread is (hangs) and the loaded modules |
| `overlay` | toggle the overlay |
| `abilities` / `abilities on|off` / `abilities mask <hex>` | the ability stack (base and pushed sets, Grasp) and the unlock |

## How it hooks (addresses from the IDB, verified live 2026-10-12)

- `Human__PostUpdate` 0xB26FD0 (Human vftable 0x16D9314, slot 18): every Human goes through it.
  - The player is the main character: Human vt20 → handle +4 = entity, and entity `+168 & 7 == 1`
    (`Human__CharacterLayer` 0xB0EAA0).
  - During a load a placeholder main character exists first; the newest one wins.
- `PadProxyPC__Update` 0x909C90 (vftable slot 0x16B1420): pad recording and injection run after it.
  - The proxy holds `+8` last / `+24` this-frame buttons, `+48` the stamp, `+56` the press stamps, `+184` values,
    and `+256` / `+272` the sticks.
  - The menus read the active source pad (`+1284` index, pads at `+1292`, stride 536), so both are written.
  - The title screen and the menus read the keyboard directly; `tools/keys.py` sends real keystrokes for them.
- `GoAssassinActionInterpreter__Update` 0xEEE700 (vftable 0x16F52AC, slot 18): remembers the interpreter.
  `ReadInput` 0xEEDE60 stores the world move direction at +4688 and the stick magnitude at +4672 (recorded as
  `mdx mdy mdz mmag`).
- The game clock is the Engine::BeginFrame task object `*0x1A1197C` (`sub_417470` → `sub_416420`).
  - The frame dt = `+12` (scale) × the mode's time, clamped to at least 0.0001 s, and goes to `g_FrameDt`
    (0x192DDA8, setter 0x93E0F0).
  - Pause sets the scale to 0.
- `0x419880` returns the current **World** (`Game__GetWorld`, +128 = the death height), not the player. It was
  misnamed `Game__GetPlayerEntity`; renamed in the IDB.
- The playing action is read as the game reads it: Human vt20 → `sub_7183F0` → `sub_5015C0(comp, 0)` → Action
  `+8`, the request id (`HumanLedge` 0xDD0B60 compares it this way).
- Position is the entity matrix at +0x10 (translation +0x40). Forward is row 1.
- Velocity is the `CharacterComponent`'s +0x90 (the character controller; `comps` lists the entity's components).
- Each frame also records the six handle bones (Human +0x280.., `Bone__GetWorldPosition` 0x4EC0E0), the upper-body
  slot's action (slot 1) and the interpreter's world move direction / stick magnitude.
- Overlay: `IDirect3DSwapChain9::Present` (the game presents through the swap chain after its first frame) and
  `IDirect3DDevice9::Present` / `Reset`.
  - Text is a GDI-made font atlas.
  - The game's own D3DX font stalled its renderer after one frame (tested), so it isn't used.

## Known limits

- **F5 / F6 bookmarks are untested.** They move the entity matrix and any controller copies found by value. The
  controller follows the entity by a sweep, and no CharacterController component was found. Expect them to fail or
  to slide; test on open ground.
- **`compare.py` starts the port on flat ground.** It compares position only for recordings on open flat ground.
  Context and timing compare anywhere.
- **The port replay assumes the camera yaw.** It doesn't know the game camera's yaw, so the climb / ledge / ladder
  stick (actor frame) is approximate there.
- **Injected sticks skip the pad's dead zone.** The game zeroes sticks under 0.35 inside the pad source
  (`Pad__ctor` 0x940340, `PadXenon__Update`); the lab writes after that, so an injected 0.2 reaches the interpreter,
  which a real pad never does (RE/25 round 2, #41).
- **Untested fields.** The field expressions of #1 (InAir +336) and #67 (HumanData +0x202, HG +0x588) come from the
  RE docs and haven't been read live yet.
- **Smoke tested on 2026-10-12 only:**
  - boot to gameplay;
  - the overlay;
  - screenshots;
  - pad scripts (walk / stop / turn);
  - recording and marks;
  - the bridge;
  - the port replay and comparison.

  The probes armed without errors, but no probe hit was exercised live.
