"""Dump classes / enums from the Revelations PDB (IDB/scimitar_final_ltcg.pdb) by name.

llvm-pdbutil refuses this PDB ("Too many directory blocks"), so this reads the MSF 7.00 container and the TPI
stream directly. Only what the RE needs: LF_CLASS / LF_STRUCTURE with their data members and base classes, and
LF_ENUM with their enumerators.

Usage: py RE/tools/pdbtypes.py "scimitar::ac2::HumanGround" ["scimitar::ac2::HumanGroundData::MovementState" ...]
       py RE/tools/pdbtypes.py --find HumanGround     (list type names containing the text)
The PDB is big-endian-target (Xbox 360) but the PDB itself is little-endian. Revelations is a later game on the
same engine: offsets differ from AC2 (HumanGroundData: +0x20 after 0x150); names are a lead (RE/02).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PDB = os.path.join(HERE, "..", "..", "IDB", "scimitar_final_ltcg.pdb")

LF_FIELDLIST, LF_CLASS, LF_STRUCTURE, LF_ENUM = 0x1203, 0x1504, 0x1505, 0x1507
LF_MEMBER, LF_ENUMERATE, LF_BCLASS, LF_NESTTYPE, LF_ONEMETHOD, LF_METHOD = 0x150D, 0x1502, 0x1400, 0x1510, 0x1511, 0x150F
LF_INDEX = 0x1404
LF_BITFIELD = 0x1205
LF_STMEMBER, LF_VFUNCTAB, LF_VBCLASS, LF_IVBCLASS, LF_POINTER, LF_ARRAY, LF_MODIFIER = 0x150E, 0x1409, 0x1401, 0x1402, 0x1002, 0x1503, 0x1001
SIMPLE = {0x03: "void", 0x10: "char", 0x20: "uchar", 0x11: "short", 0x21: "ushort", 0x12: "long", 0x22: "ulong",
          0x74: "int", 0x75: "uint", 0x40: "float", 0x41: "double", 0x30: "bool", 0x68: "int8", 0x69: "uint8",
          0x72: "int16", 0x73: "uint16", 0x76: "int64", 0x77: "uint64", 0x13: "int64", 0x23: "uint64"}


def read_streams(path):
    f = open(path, "rb")
    hdr = f.read(0x38)
    assert hdr.startswith(b"Microsoft C/C++ MSF 7.00")
    bs, _, nblocks, dirsize, _ = struct.unpack_from("<5I", hdr, 0x20)
    n_dir_blocks = (dirsize + bs - 1) // bs
    n_map_blocks = (n_dir_blocks * 4 + bs - 1) // bs
    f.seek(0x34)
    map_addrs = struct.unpack(f"<{n_map_blocks}I", f.read(4 * n_map_blocks))

    def block(i):
        f.seek(i * bs)
        return f.read(bs)

    dir_block_ids = b"".join(block(a) for a in map_addrs)
    dir_blocks = struct.unpack_from(f"<{n_dir_blocks}I", dir_block_ids)
    directory = b"".join(block(b) for b in dir_blocks)[:dirsize]
    nstreams = struct.unpack_from("<I", directory)[0]
    sizes = struct.unpack_from(f"<{nstreams}I", directory, 4)
    pos = 4 + 4 * nstreams
    streams = []
    for s in sizes:
        if s == 0xFFFFFFFF:
            s = 0
        nb = (s + bs - 1) // bs
        ids = struct.unpack_from(f"<{nb}I", directory, pos)
        pos += 4 * nb
        streams.append((ids, s))

    def stream(k):
        ids, s = streams[k]
        return b"".join(block(i) for i in ids)[:s]

    return stream


def numeric(b, o):
    v = struct.unpack_from("<H", b, o)[0]
    if v < 0x8000:
        return v, o + 2
    size = {0x8000: ("<b", 1), 0x8001: ("<h", 2), 0x8002: ("<H", 2), 0x8003: ("<i", 4), 0x8004: ("<I", 4),
            0x8009: ("<q", 8), 0x800A: ("<Q", 8)}[v]
    return struct.unpack_from(size[0], b, o + 2)[0], o + 2 + size[1]


def cstr(b, o):
    e = b.index(b"\0", o)
    return b[o:e].decode("latin-1"), e + 1


class TPI:
    def __init__(self, data):
        hsize, self.ti_begin, self.ti_end = struct.unpack_from("<III", data, 4)
        self.recs = {}
        o = hsize
        ti = self.ti_begin
        while o < len(data) and ti < self.ti_end:
            ln, kind = struct.unpack_from("<HH", data, o)
            self.recs[ti] = (kind, data[o + 4:o + 2 + ln])
            o += 2 + ln
            ti += 1
        self.by_name = {}
        for ti, (kind, b) in self.recs.items():
            nm = self.name_of(ti)
            if nm and not self.is_forward(ti):
                self.by_name.setdefault(nm, ti)

    def is_forward(self, ti):
        kind, b = self.recs[ti]
        if kind in (LF_CLASS, LF_STRUCTURE, LF_ENUM):
            return bool(struct.unpack_from("<H", b, 2)[0] & 0x80)
        return False

    def name_of(self, ti):
        kind, b = self.recs.get(ti, (None, None))
        if kind in (LF_CLASS, LF_STRUCTURE):
            _, o = numeric(b, 16)
            return cstr(b, o)[0]
        if kind == LF_ENUM:
            return cstr(b, 12)[0]
        return None

    def type_name(self, ti):
        if ti < 0x1000:
            base = SIMPLE.get(ti & 0xFF, f"simple{ti:#x}")
            return base + ("*" if ti & 0x700 else "")
        kind, b = self.recs.get(ti, (None, b""))
        if kind in (LF_CLASS, LF_STRUCTURE, LF_ENUM):
            return self.name_of(ti)
        if kind == LF_POINTER:
            return self.type_name(struct.unpack_from("<I", b)[0]) + "*"
        if kind == LF_MODIFIER:
            return self.type_name(struct.unpack_from("<I", b)[0])
        if kind == LF_ARRAY:
            elem, _ = struct.unpack_from("<II", b)
            size, _ = numeric(b, 8)
            return f"{self.type_name(elem)}[{size} bytes]"
        if kind == LF_BITFIELD:  # {type, length, position}
            base, length, position = struct.unpack_from("<IBB", b)
            return f"{self.type_name(base)} : bits {position}..{position + length - 1}"
        return f"type{ti:#x}"

    def fields(self, flist):
        kind, b = self.recs[flist]
        assert kind == LF_FIELDLIST
        o = 0
        while o < len(b):
            if b[o] >= 0xF0:
                o += b[o] & 0x0F
                continue
            k = struct.unpack_from("<H", b, o)[0]
            if k == LF_MEMBER:
                attr, ty = struct.unpack_from("<HI", b, o + 2)
                off, p = numeric(b, o + 8)
                nm, p = cstr(b, p)
                yield ("member", off, nm, self.type_name(ty))
            elif k == LF_ENUMERATE:
                val, p = numeric(b, o + 4)
                nm, p = cstr(b, p)
                yield ("enum", val, nm, None)
            elif k == LF_BCLASS:
                attr, ty = struct.unpack_from("<HI", b, o + 2)
                off, p = numeric(b, o + 8)
                yield ("base", off, self.type_name(ty), None)
            elif k in (LF_VBCLASS, LF_IVBCLASS):
                _, p = numeric(b, o + 12)
                _, p = numeric(b, p)
            elif k == LF_NESTTYPE:
                _, p = cstr(b, o + 8)
            elif k == LF_ONEMETHOD:
                attr = struct.unpack_from("<H", b, o + 2)[0]
                p = o + 8 + (4 if ((attr >> 2) & 7) in (4, 6) else 0)
                _, p = cstr(b, p)
            elif k == LF_METHOD:
                _, p = cstr(b, o + 8)
            elif k == LF_STMEMBER:
                _, p = cstr(b, o + 8)
            elif k == LF_VFUNCTAB:
                p = o + 8
            elif k == LF_INDEX:  # the list continues in another LF_FIELDLIST
                yield from self.fields(struct.unpack_from("<I", b, o + 4)[0])
                return
            else:
                raise ValueError(f"leaf {k:#x} at {o}")
            o = p

    def dump(self, name):
        ti = self.by_name.get(name)
        if ti is None:
            print(f"{name}: not found")
            return
        kind, b = self.recs[ti]
        if kind == LF_ENUM:
            count, prop, utype, flist = struct.unpack_from("<HHII", b)
            print(f"enum {name}  ({count} values)")
            for _, val, nm, _ in self.fields(flist):
                print(f"  {val:6} {nm}")
        else:
            count, prop, flist, derived, vshape = struct.unpack_from("<HHIII", b)
            size, _ = numeric(b, 16)
            print(f"class {name}  size {size:#x}")
            for what, off, nm, ty in self.fields(flist):
                if what == "base":
                    print(f"  +{off:#06x}  base {nm}")
                elif what == "member":
                    print(f"  +{off:#06x}  {nm:40} {ty}")


def main():
    args = sys.argv[1:]
    tpi = TPI(read_streams(PDB)(2))
    if args and args[0] == "--find":
        for nm in sorted(n for n in tpi.by_name if args[1] in n):
            print(nm)
        return
    for a in args:
        tpi.dump(a)


if __name__ == "__main__":
    main()
