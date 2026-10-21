"""Index every reflected class descriptor in the exe (read-only).

ClassDesc layout (RE/07 §2.2): +0x00 props*, +0x04 nProps, +0x08 enums*, +0x0C nEnums, +0x18 groups*,
+0x1C nGroups, +0x20 name*, +0x24 base hash, +0x28 class hash (= CRC32(name), the hash written in
serialized payloads), +0x2C size.

Usage: py refl_index.py [out.json]      (default ../data/refl_classes.json)
"""
import json
import os
import struct
import sys
import zlib

from pe import D, SECS, BASE

DATA = [(n, sva, vs, rp, rs) for (n, sva, vs, rp, rs) in SECS if n == ".data"][0]
RDATA = [s for s in SECS if s[0] in (".rdata", ".data")]


def va_ok(va):
    for n, sva, vs, rp, rs in RDATA:
        if sva <= va < sva + max(vs, rs):
            return True
    return False


def rd(va, n):
    for _, sva, vs, rp, rs in SECS:
        if sva <= va < sva + rs:
            return D[rp + va - sva: rp + va - sva + n]
    return None


def cstr(va):
    b = rd(va, 200)
    if not b:
        return None
    s = b.split(b"\0")[0]
    if not s or any(c < 32 or c > 126 for c in s):
        return None
    return s.decode()


def scan():
    _, sva, vs, rp, rs = DATA
    blob = D[rp:rp + rs]
    out = {}
    for o in range(0, rs - 0x30, 4):
        props, nprops, enums, nenums, z0, z1, groups, ngroups, name, h24, h28, size = struct.unpack_from("<IiIiIIIiIIII", blob, o)
        if not (0 <= nprops < 600 and 0 <= nenums < 64 and 0 <= ngroups < 64 and z0 == 0 and z1 == 0):
            continue
        if nprops and not va_ok(props):
            continue
        if not va_ok(name):
            continue
        nm = cstr(name)
        if not nm or zlib.crc32(nm.encode()) & 0xFFFFFFFF != h28:
            continue
        rows = []
        for i in range(nprops):
            w = struct.unpack("<8I", rd(props + 32 * i, 32))
            rows.append(w)
        enum_list = []
        for i in range(nenums):
            er = struct.unpack("<IiII", rd(enums + 16 * i, 16))
            recs = []
            for k in range(er[1]):
                rn, rv, rh = struct.unpack("<IiI", rd(er[0] + 12 * k, 12))
                recs.append((cstr(rn), rv))
            enum_list.append(dict(hash=f"{er[2]:08x}", name=cstr(er[3]), values=recs))
        out[f"{h28:08x}"] = dict(
            name=nm, desc=f"{sva + o:#x}", base=f"{h24:08x}", size=size,
            props=[dict(flags=f"{w[0]:08x}", name=f"{w[1]:08x}", type=f"{w[2]:08x}", code=f"{w[3]:08x}",
                        offset=w[4] >> 18, raw=[f"{x:08x}" for x in w[4:]]) for w in rows],
            enums=enum_list,
        )
    return out


if __name__ == "__main__":
    dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "data", "refl_classes.json")
    idx = scan()
    json.dump(idx, open(dst, "w"), indent=1)
    print(f"{len(idx)} class descriptors -> {dst}")
