"""Record edge clamping live (RE/20 row 12), read-only.

Usage: py livelab/record_edges.py <seconds> <out.jsonl>   (recordings stay out of git)
       py livelab/record_edges.py --summary <out.jsonl>

Per sample: the position, the current context, the Human's edge-clamping flags (+0x991..+0x996), its
EdgeClampingSettings (+0x9F0), its gap (m_Gap +0x8E0, m_IsGapValid +0x920; RE/11_ground.md 16), and the player's
AssassinTask: high profile (+0x94C), the stick length (+0xEC0), GapForStopWalking (m_Gap +0x840, m_IsSet +0x8A0).
A status line is printed about 4 times a second.
"""
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from peek import Proc, player_entity, player_task  # noqa: E402

CONTEXTS = {0x19BFCD8: "Ground", 0x19C05B0: "InAir", 0x19C0010: "Ledge", 0x1A2EC08: "Walling",
            0x1A30588: "NarrowObject"}


def gap(b):
    """IHuman::Gap (0x40): edge position +0x10, edge normal +0x20, depth +0x30, square distance +0x34, no-edge +0x38."""
    ex, ey, ez, _, nx, ny, nz, _, depth, sq = struct.unpack_from("<10f", b, 0x10)
    return {"edge": [round(ex, 3), round(ey, 3), round(ez, 3)], "n": [round(nx, 3), round(ny, 3)],
            "depth": round(depth, 3), "sq": round(sq, 4), "noEdge": b[0x38]}


def sample(p, ent, human, task, t):
    e, h, k = p.read(ent + 0x40, 12), p.read(human, 0xA20), p.read(task, 0xF00)
    if not (e and h and k):
        return None
    ctx = struct.unpack_from("<I", h, 0x48)[0]
    vt = p.u32(ctx) if ctx else None
    hg = struct.unpack_from("<I", h, 0x440)[0]
    gd = p.u32(hg + 0x10) if hg else None
    g = p.read(gd, 0xF0) if gd else None
    return {
        "t": round(t, 4),
        "pos": [round(v, 3) for v in struct.unpack("<3f", e)],
        "ctx": CONTEXTS.get(vt - p.base, hex(vt - p.base)) if vt else None,
        "active": h[0x991], "fight": h[0x992], "enable": h[0x993], "zero": h[0x994], "used": h[0x995],
        "forced": h[0x996],
        "edgeDist": round(struct.unpack_from("<f", h, 0x9F0)[0], 3),
        "rejectDir": [round(v, 3) for v in struct.unpack_from("<2f", h, 0xA00)],
        "rejectAngle": round(struct.unpack_from("<f", h, 0xA10)[0], 4),
        "gapValid": h[0x920], "gap": gap(h[0x8E0:0x920]),
        "hp": k[0x94C], "len": round(struct.unpack_from("<f", k, 0xEC0)[0], 3),
        "stopSet": k[0x8A0], "stopGap": gap(k[0x840:0x880]),
        # HumanGroundData: CurSpeedRatio +0x84, DestSpeedRatio +0x88, BreakHard +0xED, Sprint +0xEE
        "cur": round(struct.unpack_from("<f", g, 0x84)[0], 3) if g else None,
        "dest": round(struct.unpack_from("<f", g, 0x88)[0], 3) if g else None,
        "brk": g[0xED] if g else None, "sprint": g[0xEE] if g else None,
    }


def record(secs, out):
    p = Proc()
    ent = player_entity(p)
    task = player_task(p)
    human = p.u32(task + 0x10)
    print(f"entity {ent:#x} human {human:#x} task {task:#x}", flush=True)
    t0 = time.perf_counter()
    last = -1.0
    with open(out, "w") as f:
        while (t := time.perf_counter() - t0) < secs:
            s = sample(p, ent, human, task, t)
            if s:
                f.write(json.dumps(s) + "\n")
                if t - last >= 0.25:
                    last = t
                    gp = s["gap"]
                    print(f"{t:6.1f} {s['ctx']:>7} z {s['pos'][2]:7.2f} hp {s['hp']} spr {s['sprint']} "
                          f"act {s['active']} en {s['enable']} zero {s['zero']} rej {s['rejectAngle']:.2f} "
                          f"gap {s['gapValid']} depth {gp['depth']:5.2f} sq {gp['sq']:6.3f} stop {s['stopSet']} "
                          f"brk {s['brk']} dest {s['dest']}", flush=True)
            time.sleep(0.008)
    print("done", out)


def summary(path):
    """One line per change of context or of the clamp's enable flag, with the gap's depth at that moment."""
    prev = None
    for line in open(path):
        s = json.loads(line)
        key = (s["ctx"], s["enable"], s["stopSet"], s["hp"], s["sprint"])
        if key != prev:
            print(f"{s['t']:7.2f} {s['ctx']:>7} pos {s['pos']} hp {s['hp']} sprint {s['sprint']} "
                  f"active {s['active']} enable {s['enable']} zero {s['zero']} reject {s['rejectAngle']} "
                  f"gapValid {s['gapValid']} depth {s['gap']['depth']} sq {s['gap']['sq']} "
                  f"stop {s['stopSet']} stopDepth {s['stopGap']['depth']}")
            prev = key


if __name__ == "__main__":
    if sys.argv[1] == "--summary":
        summary(sys.argv[2])
    else:
        record(float(sys.argv[1]), sys.argv[2])
