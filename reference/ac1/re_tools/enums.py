"""Find reflection enum descriptors {records*, count, hash, name*} and class descriptors.

Enum record layout (verified on ActorStateID @0x190DFE8 and HumanClimbData enums @0x1997808):
    struct EnumRecord { const char* name; int32 value; uint32 hash; }   // 12 bytes
Enum descriptor (verified @0x1997844, 0x1997854, 0x1997864):
    struct EnumDesc { EnumRecord* records; int32 count; uint32 hash; const char* name; }  // 16 bytes
"""
import struct, json, sys
from pe import *

data_secs = [s for s in SECS if s[0] in (".data", ".rdata")]


def scan():
    found = []
    for name, sva, vs, rp, rs in data_secs:
        for o in range(rp, rp + rs - 16, 4):
            recs, count, h, nameptr = struct.unpack_from("<IiII", D, o)
            if not (1 <= count <= 512):
                continue
            nm = cstr(nameptr) if nameptr else None
            if not nm or len(nm) < 3:
                continue
            if sec(recs) not in (".data", ".rdata"):
                continue
            # validate records
            ok = True
            items = []
            for i in range(count):
                rn = u32(recs + i * 12)
                rv = i32(recs + i * 12 + 4)
                s = cstr(rn) if rn else None
                if s is None:
                    ok = False
                    break
                items.append((rv, s))
            if ok:
                va = sva + (o - rp)
                found.append((va, nm, items))
    return found


if __name__ == "__main__":
    res = scan()
    out = {f"{va:#x}": {"name": nm, "values": items} for va, nm, items in res}
    json.dump(out, open(sys.argv[1], "w"), indent=1)
    print(len(res), "enum descriptors")
