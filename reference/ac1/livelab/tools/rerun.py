"""rerun.py - play a recording again in the live game: the same start, heading, camera, stick and buttons.

  py tools/rerun.py <recording dir> [--probes <file>] [--label <name>] [--compare] [--until <s>]

Writes courses/_rerun_<label>.txt and runs it with course.py:
- face + tp: the recording's first position and its forward (the lab's tp keeps the heading, so the character first
  walks briefly toward it);
- camera: stick-up's world direction, from the first frame with a firm stick and a move direction
  (md angle - stick angle + 90 degrees; the interpreter snaps md within 5 degrees of straight ahead, so a near-straight
  stick only gives the camera to that precision);
- script: the recorded raw stick (lx, ly) and the LEGS / HAND / HIGH buttons (bits 0 / 3 / 12) by game time.
--probes: a file of probe lines (the course syntax) added to the course.
"""
import csv
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
sys.path.insert(0, HERE)

BUTTONS = [(0, "LEGS"), (3, "HAND"), (12, "HIGH")]


def build(rec, label, probes, until):
    rows = list(csv.DictReader(open(os.path.join(rec, "frames.csv"))))
    f = lambda r, k: float(r[k])
    r0 = rows[0]
    yaw = math.degrees(math.atan2(f(r0, "fy"), f(r0, "fx")))
    cam = None
    for r in rows:
        if math.hypot(f(r, "lx"), f(r, "ly")) >= 0.4 and math.hypot(f(r, "mdx"), f(r, "mdy")) >= 0.5:
            a = math.atan2(f(r, "mdy"), f(r, "mdx")) - math.atan2(f(r, "ly"), f(r, "lx")) + math.pi / 2
            cam = (math.cos(a), math.sin(a))
            break
    lines, t0 = [], f(r0, "time")
    cur, held, acc = None, 0, 0.0
    for i, r in enumerate(rows):
        if until and f(r, "time") - t0 > until:
            break
        dt = f(r, "dt") if i + 1 < len(rows) else 0.0
        b = int(r["buttons"], 16)
        if b != held:
            if cur is not None and acc > 0:
                lines.append(f"stick {cur[0]:.3f} {cur[1]:.3f} for {acc:.4f}")
                acc = 0.0
            for bit, name in BUTTONS:
                if (b >> bit) & 1 and not (held >> bit) & 1:
                    lines.append(f"hold {name}")
                elif not (b >> bit) & 1 and (held >> bit) & 1:
                    lines.append(f"release {name}")
            held = b
        s = (round(f(r, "lx"), 3), round(f(r, "ly"), 3))
        if s != cur:
            if cur is not None and acc > 0:
                lines.append(f"stick {cur[0]:.3f} {cur[1]:.3f} for {acc:.4f}")
            cur, acc = s, 0.0
        acc += dt
    if cur is not None and acc > 0:
        lines.append(f"stick {cur[0]:.3f} {cur[1]:.3f} for {acc:.4f}")
    lines += ["release all", "stick 0 0", "wait 0.5"]
    path = os.path.join(LAB, "courses", f"_rerun_{label}.txt")
    with open(path, "w") as out:
        out.write(f"# rerun of {rec}\n")
        out.write(f"face: {yaw:.2f}\n")
        out.write(f"tp: {f(r0, 'x'):.4f} {f(r0, 'y'):.4f} {f(r0, 'z'):.4f} {yaw:.2f}\n")
        out.write("settle: 1.5\n")
        out.write(f"camera: {cam[0]:.4f} {cam[1]:.4f}\n" if cam else "camera: off\n")
        if probes:
            out.write("probes:\n")
            for line in open(probes, encoding="utf-8"):
                if line.strip() and not line.lstrip().startswith("#"):
                    out.write("  " + line.strip() + "\n")
        out.write("script:\n")
        for line in lines:
            out.write("  " + line + "\n")
    return f"_rerun_{label}", yaw, cam


def main(args):
    if not args:
        print(__doc__)
        return
    rec = args[0]
    label = args[args.index("--label") + 1] if "--label" in args else os.path.basename(rec.rstrip("/\\")).split("_", 1)[-1]
    probes = args[args.index("--probes") + 1] if "--probes" in args else None
    until = float(args[args.index("--until") + 1]) if "--until" in args else None
    name, yaw, cam = build(rec, label, probes, until)
    print(f"course {name}: heading {yaw:.1f} deg, camera {cam}")
    import course
    course.run(name, "--compare" in args)


if __name__ == "__main__":
    main(sys.argv[1:])
