"""Tiny static reader for the pinned AssassinsCreedIIGame.exe: read data by address.

Internally every address is a file VA (the header's preferred base BASE + RVA), because pointers stored in the
file use that base. The PC IDB is rebased to image base 0, so an IDA address is an RVA: convert with ida(va) and
va(ida_addr) and print IDA addresses in docs. At run time: module base + IDA address.
"""
import struct

import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paths import EXE
D = open(EXE, "rb").read()
_pe = struct.unpack_from("<I", D, 0x3C)[0]
_nsec = struct.unpack_from("<H", D, _pe + 6)[0]
_osz = struct.unpack_from("<H", D, _pe + 20)[0]
BASE = struct.unpack_from("<I", D, _pe + 24 + 28)[0]
SECS = []
for i in range(_nsec):
    o = _pe + 24 + _osz + i * 40
    name = D[o:o + 8].rstrip(b"\0").decode()
    vs, va, rs, rp = struct.unpack_from("<IIII", D, o + 8)
    SECS.append((name, BASE + va, vs, rp, rs))


def ida(va):
    """File VA -> the address shown in the PC IDB (rebased to 0)."""
    return va - BASE


def va(ida_addr):
    """PC IDB address -> file VA."""
    return ida_addr + BASE


def off(va):
    for name, sva, vs, rp, rs in SECS:
        if sva <= va < sva + rs:
            return va - sva + rp
    return None


def sec(va):
    for name, sva, vs, rp, rs in SECS:
        if sva <= va < sva + max(vs, rs):
            return name
    return None


def u32(va):
    o = off(va)
    return None if o is None else struct.unpack_from("<I", D, o)[0]


def i32(va):
    o = off(va)
    return None if o is None else struct.unpack_from("<i", D, o)[0]


def f32(va):
    o = off(va)
    return None if o is None else struct.unpack_from("<f", D, o)[0]


def cstr(va, maxlen=200):
    o = off(va)
    if o is None:
        return None
    e = D.find(b"\0", o, o + maxlen)
    if e < 0:
        return None
    s = D[o:e]
    if not s or any(c < 0x20 or c > 0x7e for c in s):
        return None
    return s.decode()


def dump(va, n, fmt="dwords"):
    """Print n dwords from va, annotating pointers to strings/sections."""
    for i in range(n):
        a = va + i * 4
        v = u32(a)
        note = ""
        s = cstr(v) if v else None
        if s:
            note = f'"{s}"'
        elif v and sec(v):
            note = f"-> {sec(v)}"
        else:
            fv = f32(a)
            if fv is not None and 1e-4 < abs(fv) < 1e6:
                note = f"(f {fv:g})"
        print(f"{a:08x}: {v:08x} {note}")
