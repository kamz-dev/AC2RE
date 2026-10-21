"""camalign.py - turn the game camera until the stick's up points along a world direction.

  py tools/camalign.py <dx> <dy> [--tol 8]

Reads the camera through the interpreter: a stick push inside the dead zone (0.25, nothing moves) makes the
interpreter report its world move direction (+4688), so stick-up's world angle is that direction. The right stick
then turns the camera (rate learnt from the first turn) until stick-up is within --tol degrees of (dx, dy).
"""
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LAB = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from lab import Lab  # noqa: E402

TMP = os.path.join(LAB, "scripts", "_cam.txt")


def play(lab, lines, timeout=5.0):
    open(TMP, "w").write("\n".join(lines) + "\n")
    lab.cmd("play scripts/_cam.txt")
    t0 = time.time()
    while time.time() - t0 < timeout and lab.status().get("play") != "0":
        time.sleep(0.05)


def up_angle(lab):
    """World angle (degrees) of stick-up."""
    open(TMP, "w").write("stick 0 0.25 for 0.4\nstick 0 0\n")
    lab.cmd("play scripts/_cam.txt")
    time.sleep(0.25)
    r = lab.cmd("eval md=$interp+4688:vec3")
    while lab.status().get("play") != "0":
        time.sleep(0.05)
    x, y, _ = (float(v) for v in r.split("(")[1].split(")")[0].split())
    if math.hypot(x, y) < 0.5:
        raise RuntimeError("no move direction from the interpreter: " + r)
    return math.degrees(math.atan2(y, x))


def wrap(a):
    return (a + 180) % 360 - 180


def align(lab, dx, dy, tol=8.0):
    """Turn the camera until stick-up points along (dx, dy) within tol degrees; returns the last error."""
    want = math.degrees(math.atan2(dy, dx))
    rate = None   # degrees per second of right stick x = +1
    for _ in range(8):
        a = up_angle(lab)
        err = wrap(want - a)
        print(f"stick-up at {a:7.1f} deg, want {want:7.1f}, off {err:+6.1f}")
        if abs(err) <= tol:
            return err
        if rate is None:
            t = 0.4
            play(lab, [f"rstick 1 0 for {t}", "rstick 0 0", "wait 0.3"])
            b = up_angle(lab)
            rate = wrap(b - a) / t
            print(f"right stick +1 turns stick-up by {rate:+.0f} deg/s")
            if abs(rate) < 5:
                print("camera: the right stick did not turn it"); return err
            continue
        t = min(abs(err / rate), 3.0)
        play(lab, [f"rstick {1 if err / rate > 0 else -1} 0 for {t:.3f}", "rstick 0 0", "wait 0.3"])
    print("camera: did not converge")
    return err


def main(argv):
    tol = float(argv[argv.index("--tol") + 1]) if "--tol" in argv else 8.0
    align(Lab(), float(argv[0]), float(argv[1]), tol)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
