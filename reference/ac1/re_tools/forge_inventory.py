"""Inventory every .forge in the game folder: files, resource classes, errors. Read-only.

Writes RE/data/forge_inventory.json and prints a summary.
"""
import collections
import glob
import json
import os
import sys
import time
import traceback

from forge import Forge, class_name

from paths import GAME
OUT = os.path.join(os.path.dirname(__file__), "..", "data", "forge_inventory.json")

inv = {}
grand = collections.Counter()
t0 = time.time()
for path in sorted(glob.glob(os.path.join(GAME, "*.forge")), key=os.path.getsize):
    name = os.path.basename(path)
    fg = Forge(path)
    cls = collections.Counter()
    errors = []
    examples = collections.defaultdict(list)
    nres = 0
    for e in fg.entries:
        try:
            for r in fg.resources(e):
                c = class_name(r["class_hash"])
                cls[c] += 1
                nres += 1
                if len(examples[c]) < 5:
                    examples[c].append(f"{e.name}/{r['name']}")
        except Exception as ex:  # keep going; record the failure
            errors.append(f"{e.index} {e.name}: {type(ex).__name__}: {ex}")
    grand.update(cls)
    inv[name] = {"files": len(fg.entries), "resources": nres, "classes": dict(cls.most_common()),
                 "examples": examples, "errors": errors[:50], "error_count": len(errors)}
    print(f"{name}: {len(fg.entries)} files, {nres} resources, {len(errors)} errors, {time.time()-t0:.0f}s",
          flush=True)
    json.dump(inv, open(OUT, "w"), indent=1)

inv["_total"] = dict(grand.most_common())
json.dump(inv, open(OUT, "w"), indent=1)
print("done", f"{time.time()-t0:.0f}s")
