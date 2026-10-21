"""Pure-Python LZO1X decompressor (format used by lzo1x_1 / lzo1x_999; decoder = lzo1x_decompress).

Written from the public LZO1X bitstream description. The game's own copy is lzo1x_decompress at
0x9A0F40 (codec table 0x192F46C in AssassinsCreed_Dx9.exe). Raises ValueError on malformed input.
"""


def decompress(src: bytes, out_len: int | None = None) -> bytes:
    out = bytearray()
    ip = 0
    n = len(src)

    def byte():
        nonlocal ip
        b = src[ip]
        ip += 1
        return b

    def copy_lit(cnt):
        nonlocal ip
        out.extend(src[ip:ip + cnt])
        ip += cnt

    def copy_match(dist, cnt):
        start = len(out) - dist
        if start < 0:
            raise ValueError("lookbehind overrun")
        for k in range(cnt):
            out.append(out[start + k])

    def run_len(t, mask):
        # extended length: zeros add 255 each, then a final byte
        if t == 0:
            t = mask
            while src[ip] == 0:
                t += 255
                _ = byte()
            t += byte()
        return t

    state = 0  # number of literals copied by previous instruction (0..3) or 4 = after long literal run
    t = byte()
    if t > 17:
        copy_lit(t - 17)
        state = 4 if t - 17 >= 4 else t - 17
        t = byte()
    while True:
        if t >= 64:          # M2: 3..8 bytes, dist <= 2048
            cnt = (t >> 5) - 1 + 2
            dist = ((t >> 2) & 7) + (byte() << 3) + 1
        elif t >= 32:        # M3: dist <= 16384
            cnt = run_len(t & 31, 31) + 2
            lo = byte(); hi = byte()
            dist = ((hi << 8 | lo) >> 2) + 1
            t = lo
        elif t >= 16:        # M4: dist 16385..49151, or end of stream
            cnt = run_len(t & 7, 7) + 2
            lo = byte(); hi = byte()
            dist = ((t & 8) << 11) + ((hi << 8 | lo) >> 2)
            if dist == 0:
                break        # end marker
            dist += 16384
            t = lo
        else:                # t < 16
            if state == 0:   # literal run
                cnt = run_len(t, 15) + 3
                copy_lit(cnt)
                state = 4
                t = byte()
                continue
            elif state < 4:  # M1 after short literal: 2 bytes, dist <= 1024
                cnt = 2
                dist = (t >> 2) + (byte() << 2) + 1
            else:            # M1 after long literal run: 3 bytes, dist 2049..3072
                cnt = 3
                dist = (t >> 2) + (byte() << 2) + 2049
        copy_match(dist, cnt)
        # trailing literals encoded in the low 2 bits of the last byte read for the match
        lit = t & 3
        copy_lit(lit)
        state = lit
        t = byte()
    if out_len is not None and len(out) != out_len:
        raise ValueError(f"size mismatch {len(out)} != {out_len}")
    return bytes(out)
