"""Dump Scimitar reflection class descriptors from the PC exe, with property names recovered from their hashes.

Usage: py RE/tools/refl.py HumanGroundData [HumanInAirData ...] [--md]
  Hash names: a text file with one candidate string per line (CRC-32, zlib), from AC2_HASHES or --hashes <path>.
  --md prints a Markdown table (for RE docs).

Class descriptor (found from the class-name string; same layout as AC1's, RE/11_ground.md 15):
  +0x00 PropDesc* props   +0x04 count     +0x08 EnumDesc* enums   +0x0C count
  +0x18 char** groups     +0x1C count     +0x20 char* name        +0x24 class hash
  +0x28 parent class hash +0x2C size
Property descriptor (32 bytes on PC, 36 on Mac):
  +0x00 flags  +0x04 name hash  +0x08 type hash (struct / enum members)  +0x0E type code  +0x0F element type
  +0x12 offset * 4
Addresses are printed as PC IDB addresses (rebased to 0).
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import D, SECS, cstr, ida, i32, sec, u32  # noqa: E402

# Type codes, from the sizes the members take (HumanGroundData, RE/11_ground.md 15); others print as hex.
TYPES = {0x00: "bool", 0x06: "u32", 0x07: "u32 flags", 0x0A: "float", 0x0D: "vec4", 0x12: "handle",
         0x13: "struct", 0x19: "enum"}

_names = {}


def load_hashes(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.rstrip("\r\n")
            if s:
                _names.setdefault(zlib.crc32(s.encode("utf-8")), s)


def name_of(h):
    return _names.get(h)


def _file_hits(value):
    pat = struct.pack("<I", value)
    i, out = D.find(pat), []
    while i >= 0:
        for n, sva, vs, rp, rs in SECS:
            if rp <= i < rp + rs:
                out.append(sva + (i - rp))
        i = D.find(pat, i + 1)
    return out


def find_class(name):
    """The class descriptor (file VA) whose name is `name`, or None."""
    s = name.encode() + b"\0"
    i = D.find(b"\0" + s)
    while i >= 0:
        for n, sva, vs, rp, rs in SECS:
            if rp <= i + 1 < rp + rs:
                for ref in _file_hits(sva + (i + 1 - rp)):
                    d = ref - 0x20
                    props, nprops, size = u32(d), i32(d + 4), u32(d + 0x2C)
                    if sec(ref) == ".data" and 0 < nprops < 500 and sec(props) == ".data" and 0 < size < 0x100000:
                        return d
        i = D.find(b"\0" + s, i + 1)
    return None


def class_info(d):
    props, nprops = u32(d), i32(d + 4)
    enums, nenums = u32(d + 8), i32(d + 0xC)
    out = dict(desc=d, name=cstr(u32(d + 0x20)), hash=u32(d + 0x24), parent=u32(d + 0x28), size=u32(d + 0x2C),
               props=[], enums=[])
    for k in range(nprops):
        p = props + 32 * k
        w4 = u32(p + 0x0C)
        out["props"].append(dict(addr=p, flags=u32(p), name_hash=u32(p + 4), type_hash=u32(p + 8),
                                 type=(w4 >> 16) & 0xFF, elem=(w4 >> 24) & 0xFF, offset=(u32(p + 0x10) >> 16) >> 2))
    for k in range(nenums if enums else 0):
        out["enums"].append(cstr(u32(enums + 16 * k + 12)))
    return out


def show(name, md=False):
    d = find_class(name)
    if d is None:
        print(f"{name}: descriptor not found")
        return
    c = class_info(d)
    parent = name_of(c["parent"]) or f"{c['parent']:08x}"
    print(f"## {c['name']}  (descriptor {ida(d):#x}, size {c['size']:#x}, hash {c['hash']:08x}, parent {parent})")
    if c["enums"]:
        print(f"enums: {', '.join(e or '?' for e in c['enums'])}")
    if md:
        print("| Offset | Name | Type |\n|---|---|---|")
    for p in sorted(c["props"], key=lambda p: p["offset"]):
        nm = name_of(p["name_hash"]) or f"?{p['name_hash']:08x}"
        ty = TYPES.get(p["type"], f"type {p['type']:#x}")
        if p["type_hash"]:
            ty += " " + (name_of(p["type_hash"]) or f"{p['type_hash']:08x}")
        if p["type"] == 0x1D:
            ty += f" of {TYPES.get(p['elem'], hex(p['elem']))}"
        if md:
            print(f"| +{p['offset']:#05x} | `{nm}` | {ty} |")
        else:
            print(f"  +{p['offset']:#05x}  {nm:40} {ty}")


def main():
    args = sys.argv[1:]
    path = os.environ.get("AC2_HASHES")
    if "--hashes" in args:
        i = args.index("--hashes")
        path = args[i + 1]
        del args[i:i + 2]
    md = "--md" in args
    args = [a for a in args if a != "--md"]
    if path:
        load_hashes(path)
    else:
        print("(no hash list: set AC2_HASHES or pass --hashes; names will be shown as hashes)")
    for name in args:
        show(name, md)


if __name__ == "__main__":
    main()
