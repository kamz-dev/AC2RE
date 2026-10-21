# Setting up a machine

Once per contributor. Afterwards, start your agent in the repository root and tell it "Read CLAUDE.md and
follow it."

## 1. The repository and the IDBs
1. Clone the repository.
2. Get the IDB baseline from the project owner and put the files in `IDB/` under the names in `IDB/README.md`.
   Check their sha256 against that table.
3. Import the shared annotations into the PC IDB (once IDA is installed, step 2):
   `py tools/ida/sync.py import IDB/AssassinsCreedIIGame.exe.i64`

## 2. IDA Pro and the MCP server
- **IDA Pro 9.x** with the x86 decompiler (Hex-Rays). The PowerPC decompiler helps with the Xbox IDB but isn't
  required.
- **idalib** (ships with IDA 9). Install its Python package once so `import idapro` works:
  `py "<IDA dir>\idalib\python\py-activate-idalib.py"` (then `pip install "<IDA dir>\idalib\python"` if needed).
- **ida-pro-mcp** (github.com/mrexodia/ida-pro-mcp) gives agents decompile, xrefs, rename, comment, set_type and
  int_convert. Use its headless idalib mode: agents open databases with `idb_open` on the absolute path of
  `<repo>\IDB\<file>.i64` and the session ids in `CLAUDE.md`. Register it with your agent (Claude Code:
  `claude mcp add ...` as its README says, or the IDA plugin's own installer).
- Don't open the same `.i64` in the IDA GUI and the MCP at the same time.

## 3. Python
**Python 3.11+**, no packages needed. `RE/tools/` reads the exe read-only. Tell it where the exe is with
`AC2_GAME_DIR` (the install folder) or `AC2_EXE` (the exe itself), then check the build:
`py RE/tools/triage.py` must print `== pinned build`.

For `RE/tools/refl.py`, get the hash list from the project owner (a text file of the strings the game hashes; it
is not in git) and set `AC2_HASHES` to its path.

## 4. The C++ build
**Visual Studio 2022** with "Desktop development with C++". Its bundled CMake is enough
(`<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`), or install CMake 3.24+.
```
cmake --preset vs2022
cmake --build --preset debug
ctest --preset debug
```
Or open the folder in Visual Studio (it reads `CMakePresets.json`).

## 5. The game (for live work)
A legally owned PC copy of Assassin's Creed II whose exe matches the pinned hash (`RE/01_triage.md`). Put it next
to the repository as `Assassin's Creed II`, or set `AC2_GAME_DIR`. Never modify it. Back up your saves before the
first live session.

## Before you start working
- `git pull`, then `py tools/ida/sync.py import IDB/AssassinsCreedIIGame.exe.i64`.
- Claim your item in `RE/ROADMAP.md`.
- Before committing: `py tools/ida/sync.py export IDB/AssassinsCreedIIGame.exe.i64`, then commit the
  `IDB/annotations/` changes with your work.
