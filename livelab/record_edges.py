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


if __name__ == "__main__":
    if sys.argv[1] == "--summary":
        summary(sys.argv[2])
    else:
        record(float(sys.argv[1]), sys.argv[2])
