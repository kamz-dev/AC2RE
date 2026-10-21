"""Headless annotation sync through idalib (IDA 9's `idapro` Python package).

Usage (repository root, with no other IDA process holding that IDB):
    py tools/ida/sync.py import IDB/AssassinsCreedIIGame.exe.i64     after every pull
    py tools/ida/sync.py export IDB/AssassinsCreedIIGame.exe.i64     before every commit  [--force]

It opens the IDB, runs tools/ida/annotations.py, saves and closes. See that file for the format and the rules.
If an MCP session already has the IDB open, run annotations.py inside that session instead, or idb_save + idb_close
it first: two IDA processes on one database can corrupt it.
"""
import os
import sys

import idapro  # must be imported before any ida_* module


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("import", "export"):
        sys.exit(__doc__)
    action, idb = sys.argv[1], os.path.abspath(sys.argv[2])
    if not idb.lower().endswith((".i64", ".idb")) or not os.path.exists(idb):
        sys.exit(f"not an IDB: {idb}")
    if idapro.open_database(idb, run_auto_analysis=False) != 0:
        sys.exit(f"could not open {idb}")
    ok = False
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import annotations
        ok = annotations.export_annotations("--force" in sys.argv) if action == "export" else annotations.import_annotations()
    finally:
        # Save in both cases: closing without saving can leave the unpacked .id0/.id1/... files behind.
        idapro.close_database(save=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
