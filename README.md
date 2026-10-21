# AC2RE

Reverse engineering Ezio's movement in Assassin's Creed II (PC) and rebuilding it in C++, as close to the original
as possible: the game's own classes, names, states and data layouts, documented address by address.

Educational, personal-use project. No game files, executables or IDA databases are in this repository. The
recreation loads data from your own install at run time.

## Contributing with an AI agent
1. Clone the repository and set the machine up once: `docs/SETUP.md` (IDA Pro + MCP, the IDBs in `IDB/`,
   Visual Studio 2022).
2. Start your agent in the repository root and tell it:

   > Read CLAUDE.md and follow it.

   Claude Code loads `CLAUDE.md` on its own; other agents find it through `AGENTS.md`.
3. Pick an open item in `RE/ROADMAP.md`, claim it, and work on a branch. Share IDA work through
   `IDB/annotations/` (`py tools/ida/sync.py export IDB/AssassinsCreedIIGame.exe.i64`), never by sending IDBs.

## Map
| Path | What |
|---|---|
| `CLAUDE.md` | The rules and the project map: start here |
| `RE/` | Findings, the roadmap, the live-check list, static tools |
| `IDB/` | IDA databases (not in git) and their shared annotations (in git) |
| `sdk/` | Layout-exact headers of the game's classes and the address table |
| `port/` | The C++ recreation: `cmake --preset vs2022 && cmake --build --preset debug && ctest --preset debug` |
| `livelab/` | The live-analysis mod (to be built) |
| `reference/ac1/` | The AC1 project's tools, as templates |
| `docs/` | Setup, requirements, and how the AC1 project ran |
