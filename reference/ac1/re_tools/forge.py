"""Reader for Assassin's Creed (2008, PC) .forge archives (format version 25).

Format reverse engineered from the game data and AssassinsCreed_Dx9.exe (see RE/08_forge_format.md).
Read-only: never writes to the game folder.

Usage:
    python forge.py list    <file.forge>                 # stored files
    python forge.py classes <file.forge> [--limit N]     # resource class histogram (decompresses)
    python forge.py extract <file.forge> <name|index> <out_dir>   # writes resources as .bin files
"""
import os
import re
import struct
import sys
import zlib
import collections

from lzo1x import decompress as lzo1x_decompress

MAGIC = b"scimitar\x00"
COMPRESSED_MAGIC = 0x1004FA9957FBAA33
FILEDATA_HEADER_SIZE = 0x1B8          # "FILEDATA" + name + per-file header, before the payload
CODECS = {0: "LZO1X_1", 1: "LZO1X_999", 2: "LZO2A", 3: "LZX"}   # codec table @0x192F46C in the exe


class ForgeEntry:
    __slots__ = ("index", "offset", "file_id", "size", "name", "timestamp")

    def __repr__(self):
        return f"<{self.index} {self.name!r} id={self.file_id:08x} off={self.offset:#x} size={self.size}>"


class Forge:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        hdr = self.f.read(0x1D)
        if hdr[:9] != MAGIC:
            raise ValueError("not a forge file")
        self.version, self.filedata_header_offset = struct.unpack_from("<IQ", hdr, 9)
        if self.version != 25:
            raise ValueError(f"unsupported forge version {self.version}")
        self.entries = []
        self._read_tables()

    def _read(self, off, n):
        self.f.seek(off)
        return self.f.read(n)

    def _read_tables(self):
        # FileDataHeader: {i32 totalFiles, i32 ?, u64 ?, i64 ?(-1), i32 maxFiles, i32 ?, u64 firstBlockOffset}
        h = self._read(self.filedata_header_offset, 0x28)
        self.total_files = struct.unpack_from("<i", h, 0)[0]
        block = struct.unpack_from("<Q", h, 0x20)[0]
        while block not in (0, 0xFFFFFFFFFFFFFFFF):
            # IndexBlock: {i32 count, i32 ?, u64 indexTable, u64 nextBlock, i32 firstIdx, i32 lastIdx,
            #              u64 nameTable, u64 rawTable}
            b = self._read(block, 0x30)
            count, _, index_off, next_block, _, _, names_off, _ = struct.unpack_from("<iiQQiiQQ", b, 0)
            if count > 0:
                idx = self._read(index_off, 16 * count)
                names = self._read(names_off, 0xBC * count)
                for i in range(count):
                    e = ForgeEntry()
                    e.index = len(self.entries)
                    e.offset, e.file_id, e.size = struct.unpack_from("<QII", idx, 16 * i)
                    nb = names[0xBC * i: 0xBC * (i + 1)]
                    e.timestamp = struct.unpack_from("<I", nb, 0x28)[0]
                    e.name = nb[0x2C:0xAC].split(b"\0", 1)[0].decode("latin-1")
                    self.entries.append(e)
            block = next_block

    # --- compressed containers -------------------------------------------------------------
    def _container(self, off, verify=True):
        """Decode one compressed container at file offset `off`; returns (bytes, end_offset)."""
        h = self._read(off, 17)
        magic, ver, codec, max_raw, max_packed, n = struct.unpack("<QHBHHH", h)
        if magic != COMPRESSED_MAGIC:
            raise ValueError(f"bad container magic at {off:#x}")
        if codec not in (0, 1):
            raise NotImplementedError(f"codec {CODECS.get(codec, codec)}")
        table = self._read(off + 17, 4 * n)
        chunks = [struct.unpack_from("<HH", table, 4 * i) for i in range(n)]
        pos = off + 17 + 4 * n
        body = self._read(pos, sum(p + 4 for _, p in chunks))
        out = bytearray()
        q = 0
        for raw, packed in chunks:
            chk = struct.unpack_from("<I", body, q)[0]
            blob = body[q + 4: q + 4 + packed]
            q += 4 + packed
            if verify and zlib.adler32(blob, 0) != chk:     # Adler-32 with initial value 0
                raise ValueError("chunk checksum mismatch")
            out += blob if packed == raw else lzo1x_decompress(blob, raw)
        return bytes(out), pos + q

    def read_file(self, entry):
        """Return (toc, data) for a stored file: toc = [(resource_id, size)], data = concatenated resources."""
        start = entry.offset + FILEDATA_HEADER_SIZE
        if struct.unpack("<Q", self._read(start, 8))[0] != COMPRESSED_MAGIC:
            # GlobalMetaFile (first entry of every forge) is a small uncompressed metadata record
            return [], b""
        toc_raw, nxt = self._container(start)
        data, end = self._container(nxt)
        n = struct.unpack_from("<H", toc_raw, 0)[0]
        toc = [struct.unpack_from("<II", toc_raw, 2 + 8 * i) for i in range(n)]
        if sum(s for _, s in toc) != len(data):
            raise ValueError("TOC sizes do not match data length")
        return toc, data

    def resources(self, entry):
        """Yield Resource dicts {id, class_hash, name, header_size, payload} for a stored file."""
        toc, data = self.read_file(entry)
        pos = 0
        for rid, size in toc:
            ch, dsz, nl = struct.unpack_from("<III", data, pos)
            name = data[pos + 12: pos + 12 + nl].decode("latin-1")
            flag = data[pos + 12 + nl]
            hdr = size - dsz                     # payload = last dataSize bytes of the resource
            if hdr < 12 + nl + 1 or (flag == 0 and hdr != 12 + nl + 1):
                raise ValueError(f"resource {rid:08x}: header/size mismatch")
            yield {"id": rid, "class_hash": ch, "name": name, "flag": flag,
                   "extra": data[pos + 12 + nl + 1: pos + hdr],   # optional sub-header when flag != 0
                   "header_size": hdr, "payload": data[pos + hdr: pos + size]}
            pos += size


_CLASS_NAMES = None


def class_name(h):
    """CRC32(class name) -> name, using RTTI names from the exe."""
    global _CLASS_NAMES
    if _CLASS_NAMES is None:
        _CLASS_NAMES = {}
        try:
            from pe import D
            for m in re.finditer(rb"\.\?AV([A-Za-z0-9_]+)@(?:scimitar@)?@", D):
                n = m.group(1)
                _CLASS_NAMES[zlib.crc32(n) & 0xFFFFFFFF] = n.decode()
        except Exception:
            pass
    return _CLASS_NAMES.get(h, f"?{h:08x}")


def _main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    cmd, path = argv[1], argv[2]
    fg = Forge(path)
    if cmd == "list":
        print(f"{len(fg.entries)} files (version {fg.version})")
        for e in fg.entries:
            print(f"{e.index:5d} {e.file_id:08x} {e.size:10d}  {e.name}")
    elif cmd == "classes":
        limit = int(argv[argv.index("--limit") + 1]) if "--limit" in argv else len(fg.entries)
        cnt = collections.Counter()
        for e in fg.entries[:limit]:
            for r in fg.resources(e):
                cnt[class_name(r["class_hash"])] += 1
        for k, v in cnt.most_common():
            print(f"{v:7d} {k}")
    elif cmd == "extract":
        key, out = argv[3], argv[4]
        e = fg.entries[int(key)] if key.isdigit() else next(x for x in fg.entries if x.name == key)
        os.makedirs(out, exist_ok=True)
        for r in fg.resources(e):
            fn = f"{r['id']:08x}_{class_name(r['class_hash'])}_{re.sub(r'[^A-Za-z0-9_.-]', '_', r['name'])}.bin"
            with open(os.path.join(out, fn), "wb") as fo:
                fo.write(r["payload"])
        print("extracted", e.name, "to", out)
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv))
