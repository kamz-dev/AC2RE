"""memscan.py - look through an object's memory over the bridge.

  py tools/memscan.py vec <addr|expr> <bytes> <x> <y> <z> [tol]   offsets holding a vec3 near (x, y, z)
  py tools/memscan.py ptrs <addr|expr> <bytes>                   pointer fields whose target has an RTTI class name
  py tools/memscan.py floats <addr|expr> <bytes>                 every plausible float (|v| in 1e-3..1e5)
An address may be an expression the lab evaluates ($entity, [$human+0x48] ...).
"""
import struct
import sys

from lab import Lab


def addr_of(lab, s):
    try:
        return int(s, 16)
    except ValueError:
        r = lab.cmd("eval " + s)
        return int(r.split()[0], 16)


def dump(lab, a, n):
    data = b""
    while len(data) < n:
        k = min(256, n - len(data))
        r = lab.cmd(f"read {a + len(data):08X} {k}")
        if r.startswith("unreadable"):
            break
        data += bytes(int(x, 16) for x in r.split())
    return data


def main(args):
    lab = Lab()
    mode, a, n = args[0], addr_of(lab, args[1]), int(args[2], 0)
    data = dump(lab, a, n)
    if mode == "vec":
        x, y, z = map(float, args[3:6])
        tol = float(args[6]) if len(args) > 6 else 0.05
        for off in range(0, len(data) - 11, 4):
            v = struct.unpack_from("<3f", data, off)
            if abs(v[0] - x) < tol and abs(v[1] - y) < tol and abs(v[2] - z) < max(tol, 1.0):
                print(f"+0x{off:X}: {v[0]:.3f} {v[1]:.3f} {v[2]:.3f}")
    elif mode == "ptrs":
        for off in range(0, len(data) - 3, 4):
            p = struct.unpack_from("<I", data, off)[0]
            if 0x10000 < p < 0x7FFF0000:
                r = lab.cmd(f"rtti 0x{p:X}").split(" ", 1)
                if len(r) > 1 and r[1].strip():
                    print(f"+0x{off:X}: {p:08X} {r[1].strip()}")
    elif mode == "floats":
        for off in range(0, len(data) - 3, 4):
            f = struct.unpack_from("<f", data, off)[0]
            if 1e-3 < abs(f) < 1e5:
                print(f"+0x{off:X}: {f:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
