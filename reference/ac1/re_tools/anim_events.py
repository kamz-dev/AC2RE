"""Animation event tracks (AnimTrackEvent, RE/10 §2.1): parse and dump.

Layout (AnimTrackEvent__Serialize 0x6DF4E0, AnimTrack__ReadKeyTimes 0x58D1C0), per track:
  u8 0, u32 objId, u32 0x41682213, u32 kind(+12), u32 n, n x EventSeed object, u32 (+24), u16 k, k x u16 key time
EventSeed (ContactEventSeed 0x993AB038 ...): u8 0, u32 objId, u32 class, then the EventSeed fields
  Event* (u8 0, u32 objId, u32 class, Event fields...), f32, u8 bool, EventSeedLinkData (u8 0, u32 objId,
  u32 0x6CE04D52, u32 link, f32, u8 bool)
ContactEvent 0x97840CBF fields: u32 handle (Event base), u32 ContactEventTypeHuman, u8, u8.

usage: py anim_events.py [name-substring ...]   (no args: summary + RE/data/anim_events.json)
"""
import os, sys, struct, json, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from forge import Forge
from paths import GAME, DATA
import ac_anim

C_TRACK_EVENT = 0x41682213
C_CONTACT_EVENT = 0x97840CBF
C_LINK = 0x6CE04D52
HUMAN = ["ft_body_roll", "ft_jump_start", "ft_land_hands", "ft_land_heavy", "ft_land_light", "ft_land_medium",
         "ft_pivot", "ft_run_cycle", "ft_slide", "ft_sneak_cycle", "ft_step_foot", "ft_step_toe",
         "ft_walk_cycle", "ft_walk_end", "ft_walk_start"]


class R:
    def __init__(s, b, o):
        s.b, s.o = b, o
    def u8(s):
        s.o += 1
        return s.b[s.o - 1]
    def u16(s):
        s.o += 2
        return struct.unpack_from('<H', s.b, s.o - 2)[0]
    def u32(s):
        s.o += 4
        return struct.unpack_from('<I', s.b, s.o - 4)[0]
    def f32(s):
        s.o += 4
        return struct.unpack_from('<f', s.b, s.o - 4)[0]
    def head(s):
        z = s.u8()
        oid = s.u32()
        cls = s.u32()
        return z, oid, cls


def ptr(r):
    """RE/09 §2: u8 0 inline object (returns its class), 2 ref to an object elsewhere, 3 null."""
    m = r.u8()
    if m == 0:
        r.u32()
        return r.u32()
    if m == 2:
        r.u32()
    return None


# Event serializers (vtable slot 2). Every Event starts with Event__Serialize 0x4B6770: u32 handle (Stream__ReadHandle 0x433D80).
# The rest, per class (u = u32, b = u8, E = embedded {u32 objId, u32 class, u32, u32 typed ref} (0x438EF0 / 0x524FD0)):
EVENT_FIELDS = {
    0x97840CBF: ('ContactEvent', 'ubb'),            # 0x5B1E20: ContactEventTypeHuman, kind bit 0, bit 1
    0xDF260DA5: ('AudioBaseEvent', 'uE'),           # 0x446120
    0xA7A64A34: ('AudioEvent', 'uEb'),              # 0x446210
    0x90D9EC94: ('SpeechEvent', 'uE'),              # 0x446570
    0x0EFFFE90: ('SoundSetEvent', 'uu'),            # 0x445CE0
    0x691A6344: ('EntityEvent', ''),                # 0x6AD6D0
    0x46D8B31E: ('DropObjectEvent', ''),            # 0xB35D50
    0x8ED28AE9: ('AnimSyncFallEvent', ''),          # 0xD3C020
    0x1B735FBF: ('AnimSyncDieEvent', ''),           # 0xD3C840
    0xD84E267F: ('BloodSplatterEvent', 'uuuu'),     # 0xB7EC90 (last: typed ref)
    0xF9BFCB7C: ('AIAnimationEvent', ''),           # 0x6F24A0
    0x28C6E4EE: ('AnimAssassinationEvent', 'u'),    # 0xB36770
}


def parse_event(r):
    cls = ptr(r)
    if cls is None:
        return None
    if cls not in EVENT_FIELDS:
        raise ValueError('event class %08x' % cls)
    name, fields = EVENT_FIELDS[cls]
    vals = [r.u32()]
    for f in fields:
        if f == 'u':
            vals.append(r.u32())
        elif f == 'b':
            vals.append(r.u8())
        else:
            vals.append((r.u32(), r.u32(), r.u32(), r.u32()))
    ev = dict(cls=cls, name=name, handle=vals[0], vals=vals[1:])
    if cls == C_CONTACT_EVENT:
        ev.update(type=vals[1], b0=vals[2], b1=vals[3])
    return ev


def parse_seed(r):
    cls = ptr(r)
    ev = parse_event(r)
    f = r.f32()
    flag = r.u8()
    lcls = ptr(r)
    link, lf, lb = None, 0.0, 0
    if lcls is not None:
        if lcls != C_LINK:
            raise ValueError('link class %08x' % lcls)
        link, lf, lb = r.u32(), r.f32(), r.u8()
    return dict(seed=cls, event=ev, f=f, flag=flag, link=link, link_f=lf, link_b=lb)


def parse_tracks(p):
    r = R(p, 0x12)
    n = r.u32()
    out = []
    for _ in range(n):
        z, oid, cls = r.head()
        if cls != C_TRACK_EVENT:
            raise ValueError('track class %08x' % cls)
        kind = r.u32()
        ns = r.u32()
        seeds = [parse_seed(r) for _ in range(ns)]
        tail = r.u32()
        k = r.u16()
        times = [r.u16() for _ in range(k)]
        out.append(dict(kind=kind, seeds=seeds, tail=tail, times=times))
    # must land on AnimTrackData
    z, oid, cls = r.head()
    if cls != 0x0181EFE8:
        raise ValueError('no AnimTrackData after events (%08x)' % cls)
    return out


def main():
    fg = Forge(os.path.join(GAME, "DataPC.forge"))
    e = [x for x in fg.entries if x.name == "Game Fix"][0]
    want = sys.argv[1:]
    stats = collections.Counter()
    kinds = collections.Counter()
    seeds = collections.Counter()
    flags = collections.Counter()
    names = collections.Counter()
    bad = 0
    dump = {}
    for res in fg.resources(e):
        p = res['payload']
        if struct.unpack_from('<I', p, 4)[0] != 0x0FA3067F:
            continue
        try:
            tr = parse_tracks(p)
        except Exception as ex:
            bad += 1
            if bad < 5:
                print('FAIL', res['name'], ex)
            continue
        stats['ok'] += 1
        dur = struct.unpack_from('<f', p, 8)[0]
        if not tr:
            continue
        ev = []
        for t in tr:
            kinds[t['kind']] += 1
            if len(t['times']) != len(t['seeds']):
                stats['count_mismatch'] += 1
            for tm, s in zip(t['times'], t['seeds']):
                seeds['%08x' % s['seed']] += 1
                e_ = s['event'] or dict(name='null', type=-1, b0=0, b1=0)
                names[e_['name']] += 1
                if 'type' not in e_:
                    continue
                flags[(e_['b0'], e_['b1'], s['flag'], s['link_b'])] += 1
                ev.append(dict(t=tm, type=e_['type'], b0=e_['b0'], b1=e_['b1'], flag=s['flag'], f=s['f'],
                               link_f=s['link_f'], link=s['link'] is not None, kind=t['kind']))
        dump[res['name']] = dict(duration=dur, events=ev)
        if want and any(w in res['name'] for w in want):
            print('==', res['name'], 'dur %.3f' % dur)
            a = ac_anim.decode(p)
            for x in ev:
                nm = HUMAN[x['type']] if x['type'] < len(HUMAN) else x['type']
                print('   t=%3d  %-15s b=%d%d flag=%d f=%.3f link_f=%.3f kind=%d' %
                      (x['t'], nm, x['b0'], x['b1'], x['flag'], x['f'], x['link_f'], x['kind']))
            for t in a['tracks']:
                if t['key'] == 2:
                    print('   contacts:', ' '.join('%.3f:%d' % (tm, v[0]) for tm, v in zip(t['times'], t['values'])))
    print('parsed', dict(stats), 'failed', bad)
    print('track kinds', dict(kinds))
    print('seed classes', dict(seeds))
    print('event classes', dict(names))
    print('flags (b0,b1,seedflag,linkb)', dict(flags))
    if not want:
        json.dump(dump, open(os.path.join(DATA, 'anim_events.json'), 'w'), indent=0)


if __name__ == '__main__':
    main()
