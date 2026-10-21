"""Decoder for Assassin's Creed (2008, PC) `Animation` resources (class hash 0x0FA3067F).

Spec: RE/10_animation_format.md.  Pure Python, no dependencies.

decode(payload, extra=None) -> dict:
    {
      'id', 'duration' (s), 'hash' (u32 at payload+0x0C),
      'event_tracks': {...raw...},
      'tracks': [ {'key': trackKey (BoneID or fixed id), 'desc': descriptor index,
                   'kind': 'quat'|'vec3'|'float'|'byte', 'comp': compression name,
                   'times': [seconds], 'frames': [1/60 s ticks], 'values': [tuple]} ... ]
    }
Quaternions are (x, y, z, w).
"""
import struct, math

# ---- descriptor table (exe 0x4B45A0 registers 64 AnimTrackDescriptorTyped singletons into 0x1A11E90) ----
# index = 4*group + variant; variant bit1 = KeyCount16, bit0 = Time16
GROUPS = [
    ('quat', 'QuatNone', 16), ('quat', 'Quat16', 2), ('quat', 'Quat24', 3), ('quat', 'Quat32', 4),
    ('quat', 'Quat48', 6), ('quat', 'Quat64', 8), ('quat', 'Quat96', 12),
    ('vec3', 'Vec3None', 12), ('vec3', 'Vec332', 4), ('vec3', 'Vec348', 6),
    ('float', 'FloatNone', 4), ('float', 'Float8', 1), ('float', 'Float16', 2),
    ('byte', 'ByteLinear', 1), ('byte', 'ByteLowest', 1), ('byte', 'ByteNearest', 1),
]
TIME_SCALE = 60.0          # f32 @0x1912CA0: key time unit = 1/60 s
INV_SQRT2 = 0.70710677     # 0xBF3504F3 = -0.70710677 (offset)


def _f(u):  # u32 bits -> float
    return struct.unpack('<f', struct.pack('<I', u))[0]


Q16_SCALE = _f(0x3DC11659)   # sqrt(2)/15
Q24_SCALE = _f(0x3C3671D7)   # sqrt(2)/127
Q32_SCALE = _f(0x3AB504F3)   # sqrt(2)/1024 (note: 1024, not 1023)
Q48_SCALE = _f(0x3835065D)   # sqrt(2)/32767
Q64_SCALE = _f(0x35B504FF)   # sqrt(2)/1048575


def _smallest3(c, idx, neg=False):
    s = c[0] * c[0] + c[1] * c[1] + c[2] * c[2]
    m = math.sqrt(max(0.0, 1.0 - s))
    if neg:
        m = -m
    q = list(c)
    q.insert(idx, m)
    return tuple(q)


def dq16(b, o):
    v = struct.unpack_from('<H', b, o)[0]
    c = [((v >> 8) & 15) * Q16_SCALE - INV_SQRT2, ((v >> 4) & 15) * Q16_SCALE - INV_SQRT2,
         (v & 15) * Q16_SCALE - INV_SQRT2]
    return _smallest3(c, v >> 14, bool(v & 0x2000))


def dq24(b, o):
    b0, b1, b2 = b[o], b[o + 1], b[o + 2]
    c = [(x & 0x7F) * Q24_SCALE - INV_SQRT2 for x in (b0, b1, b2)]
    return _smallest3(c, (b0 >> 7) | ((b1 >> 7) << 1))


def dq32(b, o):
    v = struct.unpack_from('<I', b, o)[0]
    c = [((v >> 20) & 0x3FF) * Q32_SCALE - INV_SQRT2, ((v >> 10) & 0x3FF) * Q32_SCALE - INV_SQRT2,
         (v & 0x3FF) * Q32_SCALE - INV_SQRT2]
    return _smallest3(c, v >> 30)


def dq48(b, o):
    s = struct.unpack_from('<3H', b, o)
    c = [(x & 0x7FFF) * Q48_SCALE - INV_SQRT2 for x in s]
    return _smallest3(c, (s[0] >> 15) | ((s[1] >> 15) << 1))


def dq64(b, o):
    q = struct.unpack_from('<Q', b, o)[0]
    a = (q >> 8) & 0xFFFFF
    bb = ((q >> 52) & 0xFFF) | ((q & 0xFF) << 12)
    cc = (q >> 32) & 0xFFFFF
    c = [x * Q64_SCALE - INV_SQRT2 for x in (a, bb, cc)]
    return _smallest3(c, (q >> 30) & 3)


def dq96(b, o):
    u = struct.unpack_from('<3I', b, o)
    c = [_f(x) for x in u]
    idx = {(0, 0): 0, (1, 0): 1, (0, 1): 2, (1, 1): 3}[(u[0] & 1, u[1] & 1)]
    return _smallest3(c, idx)


def dqnone(b, o):
    return struct.unpack_from('<4f', b, o)


def _sx(v, bits):
    v &= (1 << bits) - 1
    return v - (1 << bits) if v & (1 << (bits - 1)) else v


def dv332(b, o):
    v = struct.unpack_from('<I', b, o)[0]
    return (_sx(v >> 21, 11) * 0.001, _sx(v >> 10, 11) * 0.001, _sx(v, 10) * 0.001)


def dv348(b, o):
    return tuple(x * 0.001 for x in struct.unpack_from('<3h', b, o))


def dv3none(b, o):
    return struct.unpack_from('<3f', b, o)


DECODERS = {
    'QuatNone': dqnone, 'Quat16': dq16, 'Quat24': dq24, 'Quat32': dq32, 'Quat48': dq48,
    'Quat64': dq64, 'Quat96': dq96, 'Vec3None': dv3none, 'Vec332': dv332, 'Vec348': dv348,
    'FloatNone': lambda b, o: (struct.unpack_from('<f', b, o)[0],),
    'Float8': lambda b, o: (struct.unpack_from('<b', b, o)[0] * 0.008,),
    'Float16': lambda b, o: (struct.unpack_from('<h', b, o)[0] * 0.008,),
    'ByteLinear': lambda b, o: (b[o],), 'ByteLowest': lambda b, o: (b[o],),
    'ByteNearest': lambda b, o: (b[o],),
}


class Reader:
    def __init__(self, b, o=0):
        self.b, self.o = b, o

    def u8(self):
        v = self.b[self.o]; self.o += 1; return v

    def u16(self):
        v = struct.unpack_from('<H', self.b, self.o)[0]; self.o += 2; return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]; self.o += 4; return v

    def f32(self):
        v = struct.unpack_from('<f', self.b, self.o)[0]; self.o += 4; return v

    def raw(self, n):
        v = self.b[self.o:self.o + n]; self.o += n; return v


def read_track(r):
    """One compressed track (AnimTrackDescriptorTyped<...>::Read, vtable slot 21)."""
    desc = r.u8()
    kind, comp, vsize = GROUPS[desc >> 2]
    k16 = desc & 2
    t16 = desc & 1
    alloc = r.u32()             # runtime blob size (ignored)
    n = r.u32()                 # key count
    times = [0]
    for _ in range(n - 1 if n else 0):
        times.append(r.u16() if t16 else r.u8())
    raw = r.raw(n * vsize)
    dec = DECODERS[comp]
    values = [dec(raw, i * vsize) for i in range(n)]
    return {'desc': desc, 'kind': kind, 'comp': comp, 'k16': bool(k16), 't16': bool(t16),
            'times': [t / TIME_SCALE for t in times], 'frames': times, 'values': values,
            'alloc': alloc}


def decode(payload, extra=None):
    r = Reader(payload)
    res = {'id': r.u32(), 'class': r.u32()}
    res['duration'] = r.f32()                   # Animation+0x0C
    res['hash'] = r.u32()                       # Animation+0x10 (== AnimTrackData first field)
    res['flags'] = (r.u8(), r.u8())             # Animation+0x20 bits 0,1
    n_obj = r.u32()                             # Animation+0x14: AnimTrack* array (event tracks)
    obj_start = r.o
    # Event tracks are reflected objects (AnimTrackEvent etc.); skip to the AnimTrackData object.
    i = payload.find(struct.pack('<I', 0x0181EFE8), obj_start)
    if i < 0:
        raise ValueError('AnimTrackData not found')
    res['event_tracks'] = {'count': n_obj, 'raw': payload[obj_start:i - 5]}
    r.o = i - 5
    marker = r.u8(); _oid = r.u32(); _cls = r.u32()
    res['trackdata_hash'] = r.u32()
    nmap = r.u32()
    keys = []
    for _ in range(nmap):
        _oid2 = r.u32(); cls = r.u32(); key = r.u32()
        assert cls == 0x653CAA76, hex(cls)
        keys.append(key)
    res['trackdata_u16'] = r.u16()
    ntr = r.u32()
    tracks = []
    for k in range(ntr):
        t = read_track(r)
        t['key'] = keys[k] if k < len(keys) else None
        tracks.append(t)
    res['tracks'] = tracks
    res['end'] = r.o
    res['size'] = len(payload)
    return res


# ---------------------------------------------------------------- sampling
FIXED_TRACKS = {0: 'DISPLACEMENT', 1: 'PIVOT', 2: 'ACUATORCONTACTS', 3: 'STEPPHASES', 4: 'CENTEROFMASS'}
LERP_DOT = _f(0x3F7D466B)    # 0.98935574: above -> nlerp, below -> slerp (exe 0x4916F0)


def _qdot(a, b):
    return sum(x * y for x, y in zip(a, b))


def qinterp(a, b, u):
    """AnimTrackInterpolatorQuaternionLinearSlerpTestForLerp (exe 0x4916F0 / 0x48F310 / 0x48F120)."""
    d = _qdot(a, b)
    if d < 0:
        b = tuple(-x for x in b); d = -d
    if d >= LERP_DOT or d >= 1.0:
        q = tuple(x + (y - x) * u for x, y in zip(a, b))
    else:
        th = math.acos(d)
        if abs(th) <= 0.0005:
            return b
        s = math.sin(th)
        wa, wb = math.sin((1 - u) * th) / s, math.sin(u * th) / s
        q = tuple(wa * x + wb * y for x, y in zip(a, b))
    n = math.sqrt(_qdot(q, q))
    return tuple(x / n for x in q) if n > 1e-20 else q


def sample(track, t):
    """Evaluate a decoded track at time t (seconds), clamped to [first,last] key."""
    ts, vs = track['times'], track['values']
    if len(vs) == 1 or t <= ts[0]:
        return vs[0]
    if t >= ts[-1]:
        return vs[-1]
    k = 0
    while k + 1 < len(ts) and ts[k + 1] <= t:
        k += 1
    u = (t - ts[k]) / (ts[k + 1] - ts[k])
    a, b = vs[k], vs[k + 1]
    if track['kind'] == 'quat':
        if track['comp'] == 'Quat16':       # InterpolatorQuaternionLinearLerp (hypothesis: nlerp)
            if _qdot(a, b) < 0:
                b = tuple(-x for x in b)
            q = tuple(x + (y - x) * u for x, y in zip(a, b))
            n = math.sqrt(_qdot(q, q))
            return tuple(x / n for x in q)
        return qinterp(a, b, u)
    if track['kind'] == 'byte' and track['comp'] != 'ByteLinear':
        return a                            # Lowest/Nearest: step (hypothesis)
    return tuple(x + (y - x) * u for x, y in zip(a, b))


def root_motion(anim):
    """(translation_at_end, rotation_at_end, speed m/s) of the DISPLACEMENT track (key 0)."""
    tr = next((t for t in anim['tracks'] if t['key'] == 0 and t['kind'] == 'vec3'), None)
    rq = next((t for t in anim['tracks'] if t['key'] == 0 and t['kind'] == 'quat'), None)
    v = tr['values'][-1] if tr else (0.0, 0.0, 0.0)
    q = rq['values'][-1] if rq else (0.0, 0.0, 0.0, 1.0)
    dist = math.sqrt(sum(x * x for x in v))
    return v, q, (dist / anim['duration'] if anim['duration'] else 0.0)


if __name__ == '__main__':
    import sys, pickle
    d = pickle.load(open(sys.argv[1], 'rb'))
    for name in sys.argv[2:]:
        rid, cls, extra, payload = d[name]
        a = decode(payload, extra)
        print(name, 'dur=%.4f' % a['duration'], 'tracks=%d' % len(a['tracks']),
              'end=%d/%d' % (a['end'], a['size']))
        for t in a['tracks']:
            print('  %08x %-9s n=%2d frames=%s first=%s' % (t['key'], t['comp'], len(t['values']),
                  t['frames'], tuple(round(x, 4) for x in t['values'][0])))
