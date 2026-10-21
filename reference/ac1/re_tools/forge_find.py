"""Search resource names in .forge archives (read-only).

Usage: python forge_find.py <regex> [forge names...]   (default: DataPC.forge DataPC_Common.forge)
Prints: forge | file index | file name | resource id | class | resource name | payload size
"""
import os
import re
import sys

from forge import Forge, class_name

from paths import GAME

pat = re.compile(sys.argv[1], re.I)
forges = sys.argv[2:] or ["DataPC.forge", "DataPC_Common.forge"]
for fn in forges:
    fg = Forge(os.path.join(GAME, fn))
    for e in fg.entries:
        file_hit = bool(pat.search(e.name))
        try:
            for r in fg.resources(e):
                if file_hit or pat.search(r["name"]):
                    print(f"{fn} | {e.index} | {e.name} | {r['id']:08x} | {class_name(r['class_hash'])} | "
                          f"{r['name']} | {len(r['payload'])}", flush=True)
        except Exception as ex:
            print(f"{fn} | {e.index} | {e.name} | ERROR {ex}", flush=True)
