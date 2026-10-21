"""Project and game-install locations, without machine-specific paths.

ROOT: the repository (two levels above this file).
GAME: the Assassin's Creed install: the AC_GAME_DIR environment variable, else a folder named
"Assassin's Creed" next to the repository.
"""
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "RE" / "tools"
DATA = ROOT / "RE" / "data"
GAME = os.environ.get("AC_GAME_DIR") or str(ROOT.parent / "Assassin's Creed")
EXE = str(ROOT / "bin" / "AssassinsCreed_Dx9.exe")
