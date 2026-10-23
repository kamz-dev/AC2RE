"""Record edge clamping live (RE/20 row 12), read-only.

Usage: py livelab/record_edges.py <seconds> <out.jsonl>   (recordings stay out of git)
       py livelab/record_edges.py --summary <out.jsonl>
       py livelab/record_edges.py --depths <out.jsonl>    (row 14: the gap depths against the real drops)

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

# HumanGround's FSM: the current child at each level (sdk/include/ac2sdk/human_ground.h): the root (+0x42) and the
# records of Vulnerable, Navigation, Move and Wait (their current-child byte: record +2).
STATE_BYTES = {"root": 0x42, "vuln": 0x8E, "nav": 0x91, "move": 0x94, "wait": 0x97}
STATES = {1: "Vulnerable", 2: "Navigation", 3: "Move", 4: "Wait", 5: "Idle", 8: "LookDown", 10: "Search", 11: "Loot",
          15: "MoveCycle", 16: "MoveCycleDefault", 22: "MoveStop", 23: "Roll", 24: "HandPassOverReception",
          25: "Landing", 26: "LandingHard", 27: "LandingBalanceFall", 28: "RunTurn", 29: "TurnStart",
          30: "TurnOnSpot", 32: "Shocked", 35: "CrouchWait", 36: "CrouchMove", 37: "CrouchTurn", 38: "JumpImpulsion",
          39: "JumpImpulsionStraight", 40: "JumpImpulsionForwardFromIdle", 41: "WallingImpulsion",
          42: "ObstacleCollision", 43: "EdgeStop", 44: "OrientedMove", 45: "CarryObject", 46: "CustomAction",
          47: "Acting", 48: "Frozen", 49: "Dodge", 50: "Rest", 62: "EdgeLanding", 63: "GetUp", 64: "Fight"}


def state_name(i):
    return STATES.get(i, str(i))


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
    hgb = p.read(hg, 0x100) if hg else None
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
        "st": {k: hgb[o] for k, o in STATE_BYTES.items()} if hgb else None,
    }


def states(s):
    st = s.get("st")
    if not st:
        return "-"
    return "/".join(state_name(st[k]) for k in ("nav", "move", "wait"))


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
                          f"brk {s['brk']} dest {s['dest']} | {states(s)}", flush=True)
            time.sleep(0.008)
    print("done", out)


def summary(path):
    """One line per change of context or of the clamp's enable flag, with the gap's depth at that moment."""
    prev = None
    for line in open(path):
        s = json.loads(line)
        key = (s["ctx"], s["enable"], s["stopSet"], s["hp"], s["sprint"], states(s))
        if key != prev:
            print(f"{s['t']:7.2f} {s['ctx']:>7} pos {s['pos']} hp {s['hp']} sprint {s['sprint']} "
                  f"active {s['active']} enable {s['enable']} zero {s['zero']} reject {s['rejectAngle']} "
                  f"gapValid {s['gapValid']} depth {s['gap']['depth']} sq {s['gap']['sq']} "
                  f"stop {s['stopSet']} stopDepth {s['stopGap']['depth']} | {states(s)}")
            prev = key


def depths(path):
    """Row 14: each edge the gap held (with its depth), then each drop off the ground (Ground -> Ledge / InAir ->
    Ground) with the real drop from the nearest of those edges to the landing, against that edge's depth."""
    samples = [json.loads(line) for line in open(path)]
    edges = {}  # one per guidance line: the gap's edge point slides along it with the character
    for s in samples:
        if not s["gapValid"]:
            continue
        g = s["gap"]
        offset = g["edge"][0] * g["n"][0] + g["edge"][1] * g["n"][1]  # the line's distance along its normal
        key = (tuple(round(v, 1) for v in g["n"]), round(offset, 1), round(g["edge"][2], 1), g["noEdge"])
        e = edges.setdefault(key, {"edge": g["edge"], "n": g["n"], "noEdge": g["noEdge"], "depths": [],
                                   "t0": s["t"], "t1": s["t"]})
        e["depths"].append(g["depth"])
        e["t1"] = s["t"]
    print("Edges (the gap the controller held):")
    for e in edges.values():
        d = e["depths"]
        print(f"  {e['t0']:7.2f}-{e['t1']:7.2f}s edge {e['edge']} n {e['n']} noEdge {e['noEdge']} "
              f"depth {min(d):.3f}..{max(d):.3f} ({len(d)} samples)")
    print("Drops:")
    last_ground = None
    off = None  # the last Ground sample before leaving it
    hang_z = None
    for s in samples:
        if s["ctx"] == "Ground":
            if off is not None:
                land = s["pos"]
                near = min(edges.values(), default=None,
                           key=lambda e: (e["edge"][0] - off["pos"][0]) ** 2 + (e["edge"][1] - off["pos"][1]) ** 2)
                line = (f"  {off['t']:7.2f}s off {off['pos']}" + (f" hang z {hang_z:.2f}" if hang_z is not None else "")
                        + f" landed {s['t']:7.2f}s {land}")
                if land[2] > off["pos"][2] - 0.3:
                    line += " | no drop (back up or climbed)"
                elif near:
                    drop = near["edge"][2] - land[2]
                    out = (land[0] - near["edge"][0]) * near["n"][0] + (land[1] - near["edge"][1]) * near["n"][1]
                    line += (f" | nearest edge {near['edge']} n {near['n']}: real drop {drop:.3f} {out:.2f} m out, "
                             f"depth {min(near['depths']):.3f}..{max(near['depths']):.3f}")
                print(line)
                off, hang_z = None, None
            last_ground = s
        elif s["ctx"] in ("Ledge", "InAir"):
            if off is None and last_ground is not None:
                off = last_ground
            if s["ctx"] == "Ledge":
                hang_z = s["pos"][2]


if __name__ == "__main__":
    if sys.argv[1] == "--summary":
        summary(sys.argv[2])
    elif sys.argv[1] == "--depths":
        depths(sys.argv[2])
    else:
        record(float(sys.argv[1]), sys.argv[2])
