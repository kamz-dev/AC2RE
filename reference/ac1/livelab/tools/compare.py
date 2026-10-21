"""compare.py - replay a lab recording in the port and compare the two timelines.

  py tools/compare.py <recording dir or frames.csv> [--flat] [--port <port repo>] [--no-run]

1. Runs the port's `replay_lab_trace_city` test (the port's Damascus, headless, AC_LAB_TRACE=<frames.csv>; the
   recording's start position and facing) or with --flat `replay_lab_trace` (flat ground). It writes port_trace.csv
   next to frames.csv: the same input (the game's dt, buttons, world move direction and stick magnitude).
2. Writes compare.txt next to them: the context segments of both, the action / clip sequences, a 0.25 s table of
   context, position (relative to the start, game axes) and horizontal speed, and the first divergence
   (a context difference lasting over 0.1 s, or the positions more than 0.5 m apart).

Recordings made in Damascus compare position for position in the city replay; --flat compares context and timing. The port repo defaults to ../../AC1-Movement-Rewritten (next to this repo); CARGO_TARGET_DIR is
target/claude.
"""
import csv
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_PORT = os.environ.get("AC_PORT") or next(
    (p for p in (os.path.normpath(os.path.join(HERE, "..", "..", "..", n)) for n in ("AC1-Movement-Rewritten-live", "AC1-Movement-Rewritten"))
     if os.path.isdir(p)), "")


def action_names():
    names = {}
    path = os.path.join(os.path.dirname(HERE), "names", "actions.tsv")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            if "\t" in line and not line.startswith("#"):
                k, v = line.rstrip("\n").split("\t", 1)
                names[k.upper()] = v
    return names


def load(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def teleport_skip(rows):
    """The frames before a teleport among the first 20 (a jump of more than 1 m in one frame, which no movement makes;
    the lab's teleports moved 1.7 m and more): the port's replay drops them too (`lab_teleport_skip` in sim_tests.rs),
    so the two start at the same frame."""
    pos = [(float(r["x"]), float(r["y"]), float(r["z"])) for r in rows[:20]]
    skip = 0
    for i in range(1, len(pos)):
        if sum((a - b) ** 2 for a, b in zip(pos[i], pos[i - 1])) > 1.0:
            skip = i
    return skip


def segments(rows, key, t0):
    out, cur, start = [], None, None
    for r in rows:
        v = r[key]
        t = float(r["time"]) - t0
        if v != cur:
            if cur is not None:
                out.append((start, t, cur))
            cur, start = v, t
    if cur is not None:
        out.append((start, float(rows[-1]["time"]) - t0, cur))
    return out


def at(rows, t, t0):
    # the last row at or before t
    best = rows[0]
    for r in rows:
        if float(r["time"]) - t0 <= t:
            best = r
        else:
            break
    return best


BONES = ("head", "spine", "lh", "rh", "lf", "rf")


def game_local(r):
    """The game's handle bones in the character's frame: forward, right, up from the root (Z up; forward = matrix row
    1, right = (fy, -fx))."""
    fx, fy = float(r["fx"]), float(r["fy"])
    root = [float(r[k]) for k in ("x", "y", "z")]
    out = {}
    for b in BONES:
        if b + "_x" not in r:
            return None
        d = [float(r[b + "_" + k]) - root[i] for i, k in enumerate("xyz")]
        out[b] = (d[0] * fx + d[1] * fy, d[0] * fy - d[1] * fx, d[2])
    return out


def bones(game, sim, g0):
    """Per bone, the mean game and port offsets over the frames where both play the same clip (game action name and
    port clip name share the clip's base name)."""
    if "lh_f" not in sim[0] or "lh_x" not in game[0]:
        return ["BONES: not in this recording"]
    acc = {}
    for r in game:
        t = float(r["time"]) - g0
        p = at(sim, t, 0.0)
        ga = r["action_name"].split(": ", 1)[-1].split(" + ")[0].replace(".man", "")
        pc = p["clip"].split(": ", 1)[-1].split(" + ")[0].replace(".man", "")
        if not ga or ga != pc:
            continue
        g = game_local(r)
        if not g or float(p["head_u"]) == 0.0:
            continue
        a = acc.setdefault(ga, {b: [[0.0] * 3, [0.0] * 3] for b in BONES} | {"n": 0})
        a["n"] += 1
        for b in BONES:
            for k, key in enumerate(("f", "r", "u")):
                a[b][0][k] += g[b][k]
                a[b][1][k] += float(p[b + "_" + key])
    lines = ["BONES (character frame from the feet, metres: forward / right / up; mean over frames on the same clip)"]
    for clip, a in sorted(acc.items(), key=lambda kv: -kv[1]["n"])[:8]:
        n = a["n"]
        lines.append(f"  {clip} ({n} frames)")
        for b in BONES:
            g = [v / n for v in a[b][0]]
            p = [v / n for v in a[b][1]]
            lines.append(f"    {b:<5} game ({g[0]:6.3f} {g[1]:6.3f} {g[2]:6.3f})  port ({p[0]:6.3f} {p[1]:6.3f} {p[2]:6.3f})"
                         f"  diff {math.dist(g, p):5.3f}")
    return lines


def main(args):
    if not args:
        print(__doc__)
        return 1
    src = args[0]
    frames = src if src.endswith(".csv") else os.path.join(src, "frames.csv")
    port = args[args.index("--port") + 1] if "--port" in args else DEFAULT_PORT
    folder = os.path.dirname(os.path.abspath(frames))
    port_csv = os.path.join(folder, "port_trace.csv")
    if "--no-run" not in args:
        env = dict(os.environ, AC_LAB_TRACE=os.path.abspath(frames), CARGO_TARGET_DIR="target/claude")
        test = "replay_lab_trace" if "--flat" in args else "replay_lab_trace_city"
        r = subprocess.run(["cargo", "test", "--release", test, "--", "--ignored", "--nocapture", "--exact",
                            "sim_tests::replay_lab_trace" if "--flat" in args else "city::stream_tests::replay_lab_trace_city"],
                           cwd=port, env=env, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(port_csv):
            print(r.stdout[-2000:], r.stderr[-3000:])
            return 1
    game, sim = load(frames), load(port_csv)
    game = game[teleport_skip(game):]
    g0 = float(game[0]["time"])
    gx0 = [float(game[0][k]) for k in ("x", "y", "z")]
    lines = [f"compare {frames}", f"game {len(game)} frames, port {len(sim)} frames", ""]

    lines.append("CONTEXTS  (start - end s: context)")
    gs, ps = segments(game, "ctx_name", g0), segments(sim, "ctx", 0.0)
    for i in range(max(len(gs), len(ps))):
        g = f"{gs[i][0]:6.2f}-{gs[i][1]:6.2f} {gs[i][2]:<14}" if i < len(gs) else " " * 29
        p = f"{ps[i][0]:6.2f}-{ps[i][1]:6.2f} {ps[i][2]}" if i < len(ps) else ""
        lines.append(f"  game {g} | port {p}")
    lines.append("")

    lines.append("ACTIONS (game) / CLIPS (port)")
    ga = segments(game, "action_name", g0)
    names = action_names()
    for r in sim:
        c = r["clip"]
        if c.startswith("act_"):
            r["clip"] = names.get(c[4:12].upper(), c) + c[12:]
    pa = segments(sim, "clip", 0.0)
    for i in range(max(len(ga), len(pa))):
        g = f"{ga[i][0]:6.2f} {ga[i][2][:58]:<58}" if i < len(ga) else " " * 65
        p = f"{pa[i][0]:6.2f} {pa[i][2][:50]}" if i < len(pa) else ""
        lines.append(f"  {g} | {p}")
    lines.append("")

    lines.append("TABLE  t | game ctx, pos (rel), hspeed | port ctx, pos (rel), hspeed | distance")
    end = min(float(game[-1]["time"]) - g0, float(sim[-1]["time"]))
    first = None
    ctx_diff_since = None
    t = 0.0
    while t <= end + 1e-6:
        g, p = at(game, t, g0), at(sim, t, 0.0)
        gp = [float(g[k]) - gx0[i] for i, k in enumerate(("x", "y", "z"))]
        pp = [float(p[k]) for k in ("x", "y", "z")]
        d = math.dist(gp, pp)
        gh = math.hypot(float(g["vx"]), float(g["vy"]))
        lines.append(f"  {t:6.2f} | {g['ctx_name']:<9} ({gp[0]:6.2f} {gp[1]:6.2f} {gp[2]:5.2f}) {gh:5.2f} | "
                     f"{p['ctx']:<9} ({pp[0]:6.2f} {pp[1]:6.2f} {pp[2]:5.2f}) {float(p['hspeed']):5.2f} | {d:5.2f}")
        t += 0.25
    # first divergence, frame by frame on the game's clock
    for r in game:
        t = float(r["time"]) - g0
        if t > end:
            break
        p = at(sim, t, 0.0)
        if r["ctx_name"] != p["ctx"]:
            ctx_diff_since = t if ctx_diff_since is None else ctx_diff_since
            if t - ctx_diff_since > 0.1 and not first:
                first = f"context at {ctx_diff_since:.2f} s: game {r['ctx_name']}, port {p['ctx']}"
        else:
            ctx_diff_since = None
        gp = [float(r[k]) - gx0[i] for i, k in enumerate(("x", "y", "z"))]
        if not first and math.dist(gp, [float(p[k]) for k in ("x", "y", "z")]) > 0.5:
            first = f"position at {t:.2f} s: more than 0.5 m apart"
    lines.insert(2, "FIRST DIVERGENCE: " + (first or "none"))
    lines += [""] + bones(game, sim, g0)
    out = os.path.join(folder, "compare.txt")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines[:40]))
    print("...\nwritten", out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
