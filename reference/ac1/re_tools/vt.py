"""Locate MSVC RTTI vftables for class names and list their slots."""
import re, sys, struct
from pe import *

text = [s for s in SECS if s[0] == ".text"][0]
TEXT_LO, TEXT_HI = text[1], text[1] + text[2]


def type_descriptor(name):
    s = (f".?AV{name}@@" if "@" in name else f".?AV{name}@scimitar@@").encode()
    i = D.find(s + b"\0")
    if i < 0:
        return None
    # file offset -> va
    for n, sva, vs, rp, rs in SECS:
        if rp <= i < rp + rs:
            return sva + (i - rp) - 8
    return None


def find_all_u32(value):
    pat = struct.pack("<I", value)
    res, i = [], D.find(pat)
    while i >= 0:
        for n, sva, vs, rp, rs in SECS:
            if rp <= i < rp + rs:
                res.append(sva + (i - rp))
        i = D.find(pat, i + 1)
    return res


def vtables(name):
    td = type_descriptor(name)
    out = []
    for ref in find_all_u32(td):
        col = ref - 12  # CompleteObjectLocator.pTypeDescriptor at +12
        if u32(col) != 0:
            continue
        offset = u32(col + 4)
        for colref in find_all_u32(col):
            vt = colref + 4
            slots = []
            a = vt
            while True:
                v = u32(a)
                if v is None or not (TEXT_LO <= v < TEXT_HI):
                    break
                if a != vt and find_all_u32(a - 4) and a - 4 != colref and False:
                    break
                slots.append(v)
                a += 4
                if len(slots) > 400:
                    break
            out.append((offset, vt, slots))
    return out


if __name__ == "__main__":
    for name in sys.argv[1:]:
        for offset, vt, slots in vtables(name):
            print(f"{name} this+{offset:#x} vftable {vt:#x} ({len(slots)} slots)")
            print("   " + " ".join(f"{s:x}" for s in slots))
