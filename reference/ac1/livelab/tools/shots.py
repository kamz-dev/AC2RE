"""shots.py - game screenshots at views comparable with the port's AC_SHOTS views (back / left / right / front / high round the
player), steering the game's own LowHigh camera with the right stick. Writes <spot>_<view>.png and views.txt."""
import sys, os, time, math, struct, glob, re
LAB = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(LAB, "tools"))
from lab import Lab
import bmp2png

OUT = os.path.join(LAB, os.environ.get("LAB_SHOT_DIR", "shots_2026_10_17"))
os.makedirs(OUT, exist_ok=True)
L = Lab(timeout=20)


def session_dir():
    return sorted(x for x in glob.glob(os.path.join(LAB, "sessions", "2*")) if os.path.isdir(x))[-1]


def find_cam():
    """The shown LowHigh camera: the `this` of its view function 0x694080 (one probe hit)."""
    log = os.path.join(session_dir(), "events.log")
    n0 = os.path.getsize(log)
    L.cmd("probe clear")
    L.cmd("probe add 0x694080 LH_Cam max=1 this=ecx")
    hits = []
    for _ in range(20):
        time.sleep(0.25)
        with open(log) as f:
            f.seek(n0)
            hits = re.findall(r"probe LH_Cam .*this=0x([0-9A-Fa-f]+)", f.read())
        if hits:
            break
    L.cmd("probe clear")
    return int(hits[-1], 16)


def f32s(a, n):
    r = L.cmd(f"read {a:08X} {4 * n}")
    return struct.unpack(f"<{n}f", bytes(int(x, 16) for x in r.split()))


def rot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), 2 * (x * vy - y * vx)
    return (vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx))


def cam(c):
    eye, q = f32s(c + 0x20, 3), f32s(c + 0x50, 4)
    f = rot(q, (0, 1, 0))
    return eye, q, math.degrees(math.atan2(f[1], f[0])), math.degrees(math.asin(max(-1, min(1, f[2]))))


def play(lines):
    p = os.path.join(LAB, "scripts", "_shots.txt")
    open(p, "w").write("\n".join(lines) + "\n")
    L.cmd("play scripts/_shots.txt")
    t0 = time.time()
    while time.time() - t0 < 20:
        time.sleep(0.2)
        if L.status().get("play") == "0":
            break


def wrap(a):
    return (a + 180) % 360 - 180


def heading():
    r = L.cmd("eval f=$entity+0x20:vec3")
    v = [float(x) for x in r.split("(")[1].split(")")[0].split()]
    return math.degrees(math.atan2(v[1], v[0]))


def turn_to(c, want_yaw, want_pitch=None):
    for _ in range(30):
        _, _, yaw, pitch = cam(c)
        e = wrap(want_yaw - yaw)
        ep = 0 if want_pitch is None else want_pitch - pitch
        if abs(e) < 4 and abs(ep) < 4:
            return True
        lines = []
        if abs(e) >= 4:
            lines.append(f"rstick {-1 if e > 0 else 1} 0 for {min(abs(e) / 220, 0.4):.3f}")
        if abs(ep) >= 4:
            lines.append(f"rstick 0 {1 if ep > 0 else -1} for {min(abs(ep) / 120, 0.3):.3f}")
        lines.append("rstick 0 0 for 0.4")
        play(lines)
    return False


def shoot(name, x, y, z, yaw_deg):
    L.cmd("stop")
    print(L.cmd(f"tp {x} {y} {z} {yaw_deg}"))
    time.sleep(2.0)
    play(["tap R3 0.1", "wait 1.0"])
    c = find_cam()
    h = heading()
    log = []
    for view, off, pitch in (("back", 0, None), ("left", 90, None), ("right", -90, None), ("front", 180, None), ("high", 35, -40)):
        ok = turn_to(c, wrap(h + off), pitch)
        time.sleep(0.6)
        eye, q, yaw, p = cam(c)
        pos = L.status().get("pos")
        bmp = os.path.join(OUT, f"{name}_{view}.bmp")
        L.cmd(f"shot {bmp}")
        time.sleep(0.8)
        bmp2png.convert(bmp, bmp[:-4] + ".png", 1)
        os.remove(bmp)
        line = (f"{name}_{view}: steered={ok} player {pos} heading {h:.1f} | camera eye ({eye[0]:.3f} {eye[1]:.3f} "
                f"{eye[2]:.3f}) quat ({q[0]:.4f} {q[1]:.4f} {q[2]:.4f} {q[3]:.4f}) yaw {yaw:.1f} pitch {p:.1f}")
        print(line)
        log.append(line)
    with open(os.path.join(OUT, "views.txt"), "a") as f:
        f.write("\n".join(log) + "\n")


SPOTS = {"bureau": (46.39, 6.13, 4.3, -18), "street": (65.84, 5.45, 4.2, 0), "roof": (52.97, 6.0, 11.8, 90)}

if __name__ == "__main__":
    names = sys.argv[1:] or list(SPOTS)
    if not sys.argv[1:]:
        with open(os.path.join(OUT, "views.txt"), "w") as f:
            f.write("# AC1 Lab screenshots 2026-10-17 (960x540 window, game HUD on, lab overlay off). Views as the port's "
                    "AC_SHOTS (back / left / right / front / high round the player) with the game's own LowHigh camera. "
                    "Game space, Z up; camera quaternion (x y z w), forward = local +y; FOV 0.8 rad vertical.\n")
    for n in names:
        shoot(n, *SPOTS[n])
