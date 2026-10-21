"""bmp2png.py - convert a lab screenshot (24-bit BMP) to PNG, optionally downscaled.

  py tools/bmp2png.py in.bmp out.png [step]
"""
import struct
import sys
import zlib


def convert(src, dst, step=1):
    d = open(src, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]
    w, h = struct.unpack("<ii", d[18:26])
    row = (w * 3 + 3) & ~3
    ow, oh = w // step, abs(h) // step
    out = bytearray()
    for y in range(oh):
        sy = abs(h) - 1 - y * step if h > 0 else y * step
        base = off + sy * row
        out += b"\0"
        for x in range(ow):
            i = base + x * step * 3
            out += bytes((d[i + 2], d[i + 1], d[i]))

    def chunk(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xFFFFFFFF)

    with open(dst, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", ow, oh, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(out), 6)) + chunk(b"IEND", b""))
    return ow, oh


if __name__ == "__main__":
    print(convert(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1))
