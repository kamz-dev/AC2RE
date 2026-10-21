# Requirements

## The game
- A legally owned PC install of Assassin's Creed II whose `AssassinsCreedIIGame.exe` matches the pinned build
  (sha256 `f4983add9f4c366c388f067355fad52e10138aff588c8db7cc1dc4d84c56dafa`). Every address depends on it.
- A save that reaches free-roam, with as many abilities as possible. Back up saves before live work.

## Reverse engineering
- **IDA Pro 9.x** with Hex-Rays x86, and **idalib** + **ida-pro-mcp** for agents (`docs/SETUP.md`).
- The IDB baseline in `IDB/` (from the project owner).
- **Python 3.11+** for `RE/tools/` and `tools/ida/`.
- Optional: **Cheat Engine** (an MCP bridge exists) for the first live reads, before the lab mod exists.

## The recreation and the lab
- **Visual Studio 2022** with the C++ tools (CMake 3.24+ bundled). The port builds 64-bit; the lab mod and the
  `vs2022-x86` preset build 32-bit like the game.
- **Windows** for the lab: AC2 PC is D3D9, like AC1, so the AC1 lab's overlay, int3 probes and localhost bridge
  carry over.
- **ffmpeg** later, for demo videos from replays.
- **Git**. IDBs, the exe and game data never go in git.

## Agents
- **Claude Code** (or any agent that can use MCP), with the IDA MCP. Every agent follows `CLAUDE.md`.
- One routing session that plans and reviews, and one session per work group, helped in AC1 (docs/AC1_WORKFLOW.md,
  section 7).

## Hardware (what AC1 needed)
- 32 GB of RAM: headless IDA sessions take several GB each (the Mac IDB most), and full replay runs took 5–19 GB.
- 60+ GB free disk: the IDBs are ~1.8 GB, and each unpacked IDA session temporarily needs about as much again.
