"""Project and game-install locations, without machine-specific paths.

ROOT: the repository (two levels above this file).
IDB:  the IDA databases, ROOT/IDB (not in git; see IDB/README.md).
GAME: the Assassin's Creed II install: the AC2_GAME_DIR environment variable, else a folder named
      "Assassin's Creed II" next to the repository.
EXE:  the pinned PC exe: AC2_EXE, else ROOT/bin/AssassinsCreedIIGame.exe (a working copy), else the one in GAME.
      Every reader opens it read-only. Check it against EXE_SHA256 with `py RE/tools/triage.py`.
"""
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "RE" / "tools"
DATA = ROOT / "RE" / "data"
IDB = ROOT / "IDB"
GAME = os.environ.get("AC2_GAME_DIR") or str(ROOT.parent / "Assassin's Creed II")

EXE_NAME = "AssassinsCreedIIGame.exe"
# The build the PC IDB (IDB/AssassinsCreedIIGame.exe.i64) was made from. Every address in RE/ and livelab/ is for it.
EXE_SHA256 = "f4983add9f4c366c388f067355fad52e10138aff588c8db7cc1dc4d84c56dafa"
EXE_MD5 = "2dd4d3c196fa270945826d2faab42007"


def _exe():
    if os.environ.get("AC2_EXE"):
        return os.environ["AC2_EXE"]
    local = ROOT / "bin" / EXE_NAME
    if local.exists():
        return str(local)
    return str(pathlib.Path(GAME) / EXE_NAME)


EXE = _exe()
