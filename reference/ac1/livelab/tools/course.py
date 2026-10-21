"""course.py - run a test course in the live game: back to Ground, teleport, settle, record a script, summarise.

  py tools/course.py <course> [--compare] [--keep]      run one course (COURSES below, or courses/*.txt)
  py tools/course.py --list

A course file (courses/<name>.txt):
  tp: x y z yaw_degrees                 (game coordinates; start on Ground)
  settle: 1.5                           (seconds standing before recording)
  face: yaw_degrees                     (before the teleport, walk briefly toward that world direction so the
                                         teleport keeps it as the heading: the lab's tp does not set the heading)
  camera: dx dy | off                   (turn the camera so stick-up points along (dx, dy) before recording;
                                         default: the script's first `dir`. Hangs and climbs read the stick
                                         relative to the view, so the camera decides what a `dir` does there)
  fields:                               (recorded every frame)
    name=expr:type
  probes:                               (armed for the run)
    0xADDR name [when=...] [max=n] fields...
  script:                               (lab pad script, see input.c)
    dir 0 -1 0.6 for 3
The summary lists the context and action segments with the recorded fields at each change, the probe hits, and
with --compare the port's Damascus replay (tools/compare.py).
"""
import csv
import glob
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from lab import Lab  # noqa: E402


def parse(path):
    c = {"tp": None, "face": None, "camera": None, "settle": 1.5, "fields": [], "probes": [], "script": [], "after": 1.0, "rec_first": 0.0}
    block = None
    for raw in open(path, encoding="utf-8"):
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith((" ", "\t")):
            key, _, val = line.partition(":")
            key, val = key.strip(), val.strip()
            block = None
            if key in ("tp", "camera", "face"):
                c[key] = val
            elif key in ("settle", "after", "rec_first"):
                c[key] = float(val)
            elif key in ("fields", "probes", "script"):
                block = key
        elif block:
            c[block].append(line.strip())
    return c


def ctx(lab):
    return lab.status().get("ctx", "")


def to_ground(lab):
    for _ in range(6):
        c = ctx(lab)
        if c.startswith("Ground"):
            return True
        # drop from hangs / beams (empty hand), let falls finish
        if c.startswith(("Ledge", "Climb", "Ladder", "NarrowObject", "Walling")):
            lab.cmd("play scripts/tap_b.txt")
        elif c.startswith(("HayStack", "Kiosk")):
            lab.cmd("play scripts/exit_stick.txt")
        time.sleep(2.5)
    return ctx(lab).startswith("Ground")


def run(name, compare=False):
    path = os.path.join(LAB, "courses", name + ".txt")
    c = parse(path)
    lab = Lab(timeout=10)
    lab.cmd("stop")
    if not to_ground(lab):
        print("could not get back to Ground:", ctx(lab))
    lab.cmd("field clear")
    for f in c["fields"]:
        lab.cmd("field add " + f)
    lab.cmd("probe clear")
    for p in c["probes"]:
        lab.cmd("probe add " + p)
    # LAB_EXTRA_PROBES=<file>: probes (one per line, the course syntax) armed on every course too
    extra = os.environ.get("LAB_EXTRA_PROBES")
    if extra and os.path.exists(extra):
        for line in open(extra, encoding="utf-8"):
            if line.strip() and not line.lstrip().startswith("#"):
                lab.cmd("probe add " + line.strip())
    if c["rec_first"]:
        # record the teleport itself (falls)
        lab.cmd("rec start " + name)
    if c["face"] is not None:
        import math
        yaw = math.radians(float(c["face"]))
        face_script = os.path.join(LAB, "scripts", "_face.txt")
        with open(face_script, "w") as f:
            f.write(f"dir {math.cos(yaw):.4f} {math.sin(yaw):.4f} 0.5 for 0.9\ndir 0 0 0\nwait 0.6\n")
        lab.cmd("play scripts/_face.txt")
        t0 = time.time()
        while time.time() - t0 < 10 and lab.status().get("play") != "0":
            time.sleep(0.1)
    if c["tp"]:
        print(lab.cmd("tp " + c["tp"]))
    time.sleep(c["settle"])
    cam = c["camera"]
    if cam is None:
        first = next((l.split() for l in c["script"] if l.split()[:1] == ["dir"] and len(l.split()) >= 3), None)
        cam = f"{first[1]} {first[2]}" if first and (float(first[1]), float(first[2])) != (0.0, 0.0) else "off"
    if cam != "off":
        import camalign
        camalign.align(lab, *(float(v) for v in cam.split()))
        time.sleep(0.5)
    script = os.path.join(LAB, "scripts", "_course.txt")
    with open(script, "w") as f:
        f.write("\n".join(c["script"]) + "\n")
    if not c["rec_first"]:
        lab.cmd("rec start " + name)
    lab.cmd("play scripts/_course.txt")
    t0 = time.time()
    while time.time() - t0 < 120:
        time.sleep(0.5)
        if lab.status().get("play") == "0":
            break
    time.sleep(c["after"])
    lab.cmd("rec stop")
    lab.cmd("probe clear")
    session = sorted(d for d in glob.glob(os.path.join(LAB, "sessions", "2*")) if os.path.isdir(d))[-1]
    rec = sorted(glob.glob(os.path.join(session, "*_" + name)))[-1]
    summarise(rec)
    if compare:
        subprocess.run([sys.executable, os.path.join(HERE, "compare.py"), rec])
    return rec


def summarise(rec):
    rows = list(csv.DictReader(open(os.path.join(rec, "frames.csv"))))
    if not rows:
        print("no frames")
        return
    base = set("frame,time,dt,scale,x,y,z,fx,fy,fz,vx,vy,vz,ctx,ctx_name,prev_ctx,action,action_name,item_word,buttons,"
               "lx,ly,rx,ry,injected,mdx,mdy,mdz,mmag,action1,action1_name".split(","))
    extra = [k for k in rows[0] if k not in base and not k.endswith(("_x", "_y", "_z"))]
    t0 = float(rows[0]["time"])
    z0 = float(rows[0]["z"])
    print(f"{rec}: {len(rows)} frames, {float(rows[-1]['time']) - t0:.2f} s")
    prev = None
    for r in rows:
        key = (r["ctx_name"], r["action_name"], r.get("action1_name", ""))
        if key != prev:
            ex = " ".join(f"{k}={r[k]}" for k in extra)
            print(f"  {float(r['time']) - t0:6.2f} {r['ctx_name']:<12} {r['action_name'][:62]:<62} "
                  f"pos ({float(r['x']):.2f} {float(r['y']):.2f} {float(r['z']):.2f}) dz {float(r['z']) - z0:+.2f} "
                  f"v ({float(r['vx']):.2f} {float(r['vy']):.2f} {float(r['vz']):.2f}) {ex}"
                  + (f" UPPER {r.get('action1_name') or r.get('action1')}" if r.get("action1", "00000000") != "00000000" else ""))
            prev = key
    ev = os.path.join(rec, "events.log")
    hits = [l.strip() for l in open(ev) if "probe" in l or "GOTO" in l or "MARK" in l] if os.path.exists(ev) else []
    for h in hits[:40]:
        print("  >", h[:220])


def main(args):
    if not args or args[0] == "--list":
        for p in sorted(glob.glob(os.path.join(LAB, "courses", "*.txt"))):
            print(os.path.splitext(os.path.basename(p))[0])
        return 0
    run(args[0], "--compare" in args)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
