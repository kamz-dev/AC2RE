"""Triage the pinned exe: hash against the IDB's build, machine, sections, imports and RTTI class names.

Usage: py RE/tools/triage.py [--rtti <substring>]
  --rtti lists the RTTI type names containing the substring (e.g. --rtti Human).
"""
import hashlib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paths import EXE, EXE_SHA256  # noqa: E402

MACHINES = {0x14C: "x86 (32-bit)", 0x8664: "x64", 0x1F2: "PowerPC (Xbox 360)"}


def main():
    if not os.path.exists(EXE):
        sys.exit(f"exe not found: {EXE}\nSet AC2_EXE or AC2_GAME_DIR (see RE/tools/paths.py).")
    d = open(EXE, "rb").read()
    sha = hashlib.sha256(d).hexdigest()
    print(f"exe      {EXE}")
    print(f"sha256   {sha}  {'== pinned build' if sha == EXE_SHA256 else '!= PINNED BUILD: addresses in RE/ will not match'}")
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    machine, nsec = struct.unpack_from("<HH", d, pe + 4)
    osz = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    print(f"machine  {MACHINES.get(machine, hex(machine))}, preferred base {base:#x} (the PC IDB is rebased to 0)")
    for i in range(nsec):
        o = pe + 24 + osz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode(errors="replace")
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        print(f"  {name:8} rva {va:#010x} vsize {vs:#010x} raw {rs:#010x}")
    dlls = sorted({m.group(1).decode().lower() for m in re.finditer(rb"([A-Za-z0-9_]+\.dll)\0", d, re.I)})
    print(f"dll names in file: {len(dlls)}: {', '.join(dlls)}")
    names = sorted({m.group(0).decode() for m in re.finditer(rb"\.\?A[VU][A-Za-z0-9_@?$]+@@", d)})
    scim = [n for n in names if "@scimitar@@" in n]
    print(f"RTTI type names: {len(names)} ({len(scim)} in namespace scimitar)")
    if "--rtti" in sys.argv:
        sub = sys.argv[sys.argv.index("--rtti") + 1]
        for n in names:
            if sub in n:
                print("  " + n)


if __name__ == "__main__":
    main()
