"""Find reflection class descriptors by name and dump them.

Layout observed for HumanClimbData (@0x19978d8):
  +0x00 PropDesc* props      (contiguous 32-byte property descriptors)
  +0x04 int      numProps
  +0x08 EnumDesc* enums
  +0x0c int      numEnums
  +0x10 ?        (0)
  +0x14 ?        (0)
  +0x18 Group*   groups      (array of char* group labels)
  +0x1c int      numGroups
  +0x20 char*    name
  +0x24 u32      hash (class id)
  +0x28 u32      parent class hash
  +0x2c u32      size (bytes)
"""
import struct, sys, re
from pe import *


def find_name_refs(name):
    s = name.encode() + b"\0"
    res = []
    i = D.find(b"\0" + s)
    while i >= 0:
        # va of string
        for n, sva, vs, rp, rs in SECS:
            if rp <= i + 1 < rp + rs:
                sva_str = sva + (i + 1 - rp)
                pat = struct.pack("<I", sva_str)
                j = D.find(pat)
                while j >= 0:
                    for n2, sva2, vs2, rp2, rs2 in SECS:
                        if rp2 <= j < rp2 + rs2 and n2 == ".data":
                            res.append(sva2 + (j - rp2))
                    j = D.find(pat, j + 1)
        i = D.find(b"\0" + s, i + 1)
    return res


def classdesc(name):
    for ref in find_name_refs(name):
        d = ref - 0x20
        props, nprops = u32(d), i32(d + 4)
        size = u32(d + 0x2c)
        if 0 < nprops < 400 and sec(props) == ".data" and 0 < size < 0x100000:
            return d
    return None


HASH2NAME = {}


def dump_class(name, verbose=True):
    d = classdesc(name)
    if d is None:
        print(f"{name}: descriptor not found")
        return None
    props, nprops = u32(d), i32(d + 4)
    enums, nenums = u32(d + 8), i32(d + 0xc)
    groups, ngroups = u32(d + 0x18), i32(d + 0x1c)
    h, ph, size = u32(d + 0x24), u32(d + 0x28), u32(d + 0x2c)
    gl = [cstr(u32(groups + 4 * i)) for i in range(ngroups)] if groups else []
    en = [cstr(u32(enums + 16 * i + 12)) for i in range(nenums)] if enums else []
    print(f"== {name} desc@{d:#x} size={size:#x} hash={h:08x} parent={ph:08x} "
          f"props={nprops} enums={en} groups={gl}")
    rows = []
    for i in range(nprops):
        p = props + 32 * i
        w = [u32(p + 4 * k) for k in range(8)]
        rows.append(w)
        if verbose:
            print("   " + " ".join(f"{x:08x}" for x in w))
    return dict(desc=d, size=size, hash=h, parent=ph, props=rows, enums=en, groups=gl)


if __name__ == "__main__":
    for n in sys.argv[1:]:
        dump_class(n)
