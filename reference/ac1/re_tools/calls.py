"""Scan .text for E8 rel32 calls; callers(lo,hi) -> list of (site, target)."""
import struct
from pe import *
text = [s for s in SECS if s[0] == ".text"][0]
_, TVA, TVS, TRP, TRS = text
_calls = None
def all_calls():
    global _calls
    if _calls is None:
        _calls = []
        i = TRP
        end = TRP + TRS - 5
        buf = D
        idx = buf.find(b"\xe8", i, end)
        while idx >= 0:
            rel = struct.unpack_from("<i", buf, idx + 1)[0]
            site = TVA + (idx - TRP)
            tgt = site + 5 + rel
            if TVA <= tgt < TVA + TRS:
                _calls.append((site, tgt))
            idx = buf.find(b"\xe8", idx + 1, end)
    return _calls
def callers(lo, hi, exclude=None):
    out = []
    for s, t in all_calls():
        if lo <= t < hi and not (exclude and exclude[0] <= s < exclude[1]):
            out.append((s, t))
    return out
