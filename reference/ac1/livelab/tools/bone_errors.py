"""bone_errors.py - rank the clips by how far the port's hand and foot bones sit from the game's, over every replayed
recording (the limb placement check, pipeline L3).

  py tools/bone_errors.py [session glob, default 2026-*] [--top N] [--bones lh,rh,lf,rf]

Reads each recording's frames.csv and the port_trace.csv a replay left next to it (compare_all.py), pairs the frames
by time where the game's action and the port's clip share their base name (compare.py's rule), and accumulates per
clip the distance between the game's and the port's bone in the character's frame (forward / right / up from the
feet). Prints the clips with the most error (mean distance × frames), with the mean offsets per bone.

Limits: the port's replays run the limb IK only with AC_LAB_IK=1 in the environment of compare_all.py (a headless joint
rig, `ik::add_headless_rig`, plan_bone_positions phase 3); without it the port's hands and feet are the clip's alone.
The pairing is by clip name only, so a clip played in two contexts (the hang look-arounds in the Ledge and the Climb)
mixes their frames.
"""
import bisect
import csv
import glob
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import compare  # noqa: E402

SESSIONS = os.path.normpath(os.path.join(HERE, "..", "sessions"))


def main(args):
    pattern = args[0] if args and not args[0].startswith("--") else "2026-*"
    top = int(args[args.index("--top") + 1]) if "--top" in args else 20
    bones = args[args.index("--bones") + 1].split(",") if "--bones" in args else ["lh", "rh", "lf", "rf"]
    acc = {}
    recs = 0
    names = compare.action_names()
    for d in sorted(glob.glob(os.path.join(SESSIONS, pattern, "[0-9][0-9][0-9]_*"))):
        fg, fp = os.path.join(d, "frames.csv"), os.path.join(d, "port_trace.csv")
        if not (os.path.exists(fg) and os.path.exists(fp)):
            continue
        with open(fg, newline="", encoding="utf-8", errors="replace") as f:
            game = list(csv.DictReader(f))
        with open(fp, newline="", encoding="utf-8", errors="replace") as f:
            sim = list(csv.DictReader(f))
        if not game or not sim or "lh_x" not in game[0] or "lh_f" not in sim[0]:
            continue
        recs += 1
        g0 = float(game[0]["time"])
        times = [float(x["time"]) for x in sim]
        for r in game:
            i = bisect.bisect_right(times, float(r["time"]) - g0) - 1
            p = sim[max(i, 0)]
            ga = r["action_name"].split(": ", 1)[-1].split(" + ")[0].replace(".man", "")
            c = p["clip"]
            if c.startswith("act_"):
                c = names.get(c[4:12].upper(), c) + c[12:]
            pc = c.split(": ", 1)[-1].split(" + ")[0].replace(".man", "")
            if not ga or ga != pc or float(p["head_u"]) == 0.0:
                continue
            g = compare.game_local(r)
            if not g:
                continue
            a = acc.setdefault(ga, {"n": 0, "recs": set()} | {b: [0.0, [0.0] * 3, [0.0] * 3] for b in bones})
            a["n"] += 1
            a["recs"].add(os.path.basename(d))
            for b in bones:
                pv = [float(p[b + "_" + k]) for k in ("f", "r", "u")]
                a[b][0] += math.dist(g[b], pv)
                for k in range(3):
                    a[b][1][k] += g[b][k]
                    a[b][2][k] += pv[k]
    rows = []
    for clip, a in acc.items():
        n = a["n"]
        err = sum(a[b][0] for b in bones) / (n * len(bones))
        rows.append((err * n, err, clip, a))
    rows.sort(key=lambda r: -r[0])
    print(f"{recs} recordings; {len(rows)} clips compared; bones {','.join(bones)}")
    for total, err, clip, a in rows[:top]:
        n = a["n"]
        print(f"{clip}  frames {n}  recordings {len(a['recs'])}  mean distance {err:.3f} m")
        for b in bones:
            gm = [v / n for v in a[b][1]]
            pm = [v / n for v in a[b][2]]
            print(f"    {b}: game ({gm[0]:6.3f} {gm[1]:6.3f} {gm[2]:6.3f})  port ({pm[0]:6.3f} {pm[1]:6.3f} {pm[2]:6.3f})  "
                  f"mean dist {a[b][0] / n:.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
