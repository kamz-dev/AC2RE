"""Record the player's HumanGround / HumanGroundData at ~100+ Hz, read-only.

Usage: py livelab/record.py <seconds> <out.jsonl>   (recordings stay out of git)
Each line: t, raw HumanGround (0x9A0 bytes), HumanGroundData (0x230), entity (0x100), the float at entity +0x6C
-> +0x5D8 (RE/20 row 4), and the current context (Human +0x48) with its vtable.
"""
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from peek import Proc, player_human_ground  # noqa: E402

secs, out = float(sys.argv[1]), sys.argv[2]
p = Proc()
ent, human, hg = player_human_ground(p)
d = p.u32(hg + 0x10)
print(f"entity {ent:#x} human {human:#x} hg {hg:#x}", flush=True)
t0 = time.perf_counter()
with open(out, "w") as f:
    while (t := time.perf_counter() - t0) < secs:
        h, g, e = p.read(hg, 0x9A0), p.read(d, 0x230), p.read(ent, 0x100)
        six = p.u32(ent + 0x6C)
        ctx = p.u32(human + 0x48)
        if h and g and e:
            f.write(json.dumps({"t": round(t, 4), "hg": h.hex(), "gd": g.hex(), "ent": e.hex(),
                                "dt6c": p.f32(six + 0x5D8) if six else None, "ctx": ctx,
                                "ctxvt": p.u32(ctx) if ctx else None}) + "\n")
        time.sleep(0.008)
print("done", out)
