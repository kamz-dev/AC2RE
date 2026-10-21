"""Decoder for the animation graph: ActionKit / ActionBlock / Action / ActionItem / ActionTransition /
ActionBlend resources (DataPC.forge "Game Fix"). Read-only. Spec: RE/13_animation_graph.md.

Every read below mirrors a serializer in the exe (vftable slot 2):
  ActionBlock::Serialize 0x6E9BF0, Action::Serialize 0x507EE0, ActionItem::Serialize 0x5BADF0,
  ActionTransition::Serialize 0x564DB0, ActionBlend::Serialize 0x5BCDE0,
  ActionBlendFrankenstein::Serialize 0x6E8840.
Stream helpers:
  obj    (0x931780 / 0x9312F0): u8 flag; 0 = inline {u32 id, u32 class, fields}; 2 = u32 ref id; 3 = null
  handle (0x9311A0):            u8 flag; 0 = inline; 1/2 = u32 ref id; 3 = null
  typed reference (0x931410):   u32 id (no inline data unless the property flag 0x4000 is set)
  embedded object (0x438EF0):   u32 id, u32 class, fields (no flag byte)
  handle-to-object (0x433D80):  u32 id
  pod array (0x930B10):         u32 count, count * elemSize bytes

  AssociatedActionGroup::Serialize 0x6D8D20 (AssociatedAction 0x6D8850).
Usage: py ac_actions.py <ActionBlock name> [--json out.json]
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from forge import Forge, class_name  # noqa: E402

from paths import GAME  # noqa: E402

C_ACTIONBLOCK = 0xEF82FCE4
C_ACTION = 0x406089A4
C_ACTIONITEM = 0x80E50E4A
C_TRANSITION = 0x46ED6DF7
C_BLEND = 0xC7041AEE
C_FRANK = 0xFC06015C
C_ASSOC_GROUP = 0x1E6DDBE2
C_ANIMATION = 0x0FA3067F


class R:
    def __init__(self, b, o=0):
        self.b, self.o = b, o

    def u8(self):
        self.o += 1
        return self.b[self.o - 1]

    def u32(self):
        self.o += 4
        return struct.unpack_from("<I", self.b, self.o - 4)[0]

    def f32(self):
        self.o += 4
        return struct.unpack_from("<f", self.b, self.o - 4)[0]


class Graph:
    def __init__(self):
        self.objects = {}   # id -> dict (every inline object seen)


def read_obj(r, g, flag_kind="obj"):
    """Object field (0x931780) or handle field (0x9311A0)."""
    f = r.u8()
    if f == 3:
        return None
    if f == 2 or (flag_kind == "handle" and f == 1):
        return {"ref": r.u32()}
    if f != 0:
        raise ValueError(f"bad object flag {f} at {r.o - 1:#x}")
    oid, cls = r.u32(), r.u32()
    return read_body(r, g, oid, cls)


def read_embedded(r, g):
    oid, cls = r.u32(), r.u32()
    return read_body(r, g, oid, cls)


def read_body(r, g, oid, cls):
    fn = BODY.get(cls)
    if fn is None:
        raise ValueError(f"no reader for class {class_name(cls)} ({cls:08x}) at {r.o:#x}")
    o = {"id": oid, "class": class_name(cls)}
    fn(r, g, o)
    g.objects[oid] = o
    return o


def body_block(r, g, o):                       # 0x6E9BF0
    n = r.u32()
    o["actions"] = [read_obj(r, g, "handle") for _ in range(n)]
    o["body_part_template"] = r.u32()            # typed reference (0x931410, class BodyPartTemplate)
    o["associated_group"] = read_obj(r, g)


def body_action(r, g, o):                      # 0x507EE0
    o["action_id"] = r.u32()                     # +8 (== object id in all samples)
    o["channel"] = read_obj(r, g, "handle")      # +12 BodyPartChannel
    o["transition_a"] = read_obj(r, g)           # +16 ActionTransition
    o["transition_b"] = read_obj(r, g)           # +20 ActionTransition
    o["flags"] = [r.u8() & 1 for _ in range(5)]  # +48 bits 0..4
    o["enum_24"] = r.u32()                       # +24 enum 0x2E159EED
    o["u28"] = r.u32()
    o["u32"] = r.u32()
    o["associated_group"] = read_obj(r, g)       # +36
    n = r.u32()
    o["items"] = [read_obj(r, g) for _ in range(n)]


def body_item(r, g, o):                        # 0x5BADF0
    n = r.u32()
    o["animations"] = [r.u32() for _ in range(n)]          # typed refs to Animation
    n = r.u32()
    o["transitions"] = [read_obj(r, g) for _ in range(n)]  # ActionTransition objects
    o["blend"] = read_embedded(r, g)                       # +36 ActionBlend
    o["enum_56"] = r.u32()                                 # enum 0xE1590BDD
    o["bits_0_1"] = r.u32() & 3                            # enum 0xEE1C00F2 (2 bits)
    o["bits_2_3"] = r.u32() & 3                            # enum 0xEE1C00F2 (2 bits)
    o["flags"] = [r.u8() & 1 for _ in range(12)]           # bits 4..15 of +60
    o["f20"] = r.f32()
    o["u24"] = r.u32()
    o["b62"] = r.u8()
    n = r.u32()
    o["u32_array"] = [r.u32() for _ in range(n)]           # +28 SmallArray (4-byte elements)


def body_transition(r, g, o):                  # 0x564DB0
    o["blend_a"] = read_embedded(r, g)
    o["target_a"] = r.u32()                      # handle to Action
    o["u8"] = r.u32()
    o["blend_b"] = read_embedded(r, g)
    o["target_b"] = r.u32()
    o["u16"] = r.u32()


def body_blend(r, g, o):                       # 0x5BCDE0
    o["type"] = r.u32() & 7                      # enum 0xD20C01B7 (ACTBlendType?)
    o["b3"], o["b4"], o["b5"] = r.u8() & 1, r.u8() & 1, r.u8() & 1
    o["e6"], o["e8"], o["e10"] = r.u32() & 3, r.u32() & 3, r.u32() & 3
    o["b12"] = r.u8() & 1
    o["f0"], o["f4"], o["f8"] = r.f32(), r.f32(), r.f32()
    o["b18"] = r.u8()
    o["frankenstein"] = read_obj(r, g)


def body_assoc_group(r, g, o):                 # 0x6D8D20
    n = r.u32()
    o["associated"] = []
    for _ in range(n):                           # embedded AssociatedAction (0x6D8850)
        aid, acls = r.u32(), r.u32()
        o["associated"].append({"id": aid, "action": r.u32(), "f4": r.f32(), "f8": r.f32()})
    o["f4"] = r.f32()
    o["flags"] = [r.u8() & 1 for _ in range(3)]


def body_frank(r, g, o):                       # 0x6E8840
    raise ValueError("ActionBlendFrankenstein present: decode 0x6E84C0 / 0x6E8610 first")


BODY = {
    C_ACTIONBLOCK: body_block,
    C_ACTION: body_action,
    C_ACTIONITEM: body_item,
    C_TRANSITION: body_transition,
    C_BLEND: body_blend,
    C_FRANK: body_frank,
    C_ASSOC_GROUP: body_assoc_group,
}


def load_resources(names):
    fg = Forge(os.path.join(GAME, "DataPC.forge"))
    e = [x for x in fg.entries if x.name == "Game Fix"][0]
    res = list(fg.resources(e))
    by_id = {r["id"]: r for r in res}
    return {r["name"]: r for r in res if r["name"] in names}, by_id


def parse_block(payload):
    g = Graph()
    r = R(payload)
    rid, cls = r.u32(), r.u32()
    blk = read_body(r, g, rid, cls)
    if r.o != len(payload):
        raise ValueError(f"trailing bytes: parsed {r.o} of {len(payload)}")
    return blk, g


if __name__ == "__main__":
    name = sys.argv[1]
    want = {name}
    found, by_id = load_resources(want)
    blk, g = parse_block(found[name]["payload"])
    anim_names = {rid: r["name"] for rid, r in by_id.items() if r["class_hash"] == C_ANIMATION}
    print(f"{name}: {len(blk['actions'])} actions, {len(g.objects)} objects, all {len(found[name]['payload'])} bytes parsed")
    if "--json" in sys.argv:
        json.dump(blk, open(sys.argv[sys.argv.index("--json") + 1], "w"), indent=1, default=str)


def index_all():
    """Parse every ActionBlock in Game Fix. Returns (actions by id, objects by id, animation names by id,
    block name per action id)."""
    fg = Forge(os.path.join(GAME, "DataPC.forge"))
    e = [x for x in fg.entries if x.name == "Game Fix"][0]
    res = list(fg.resources(e))
    anim = {r["id"]: r["name"] for r in res if r["class_hash"] == C_ANIMATION}
    actions, objects, owner = {}, {}, {}
    for r in res:
        if r["class_hash"] != C_ACTIONBLOCK:
            continue
        blk, g = parse_block(r["payload"])
        objects.update(g.objects)
        for oid, o in g.objects.items():
            if o["class"] == "Action":
                actions[oid] = o
                owner[oid] = r["name"]
    return actions, objects, anim, owner


def describe(aid, actions, objects, anim, owner):
    a = actions.get(aid)
    if a is None:
        return f"{aid:#010x}: not an Action"
    lines = [f"{aid:#010x} [{owner[aid]}] flags={a['flags']} enum24={a['enum_24']} u28={a['u28']} u32={a['u32']}"]
    for it in a["items"]:
        if it is None:
            continue
        if "ref" in it:
            it = objects.get(it["ref"], it)
        names = [anim.get(x, f"?{x:08x}") for x in it.get("animations", [])]
        b = it.get("blend", {})
        lines.append(f"   item {it.get('id', 0):#010x}: {names} blend(type={b.get('type')} t={b.get('f0', 0):.3f},{b.get('f4', 0):.3f},{b.get('f8', 0):.3f})"
                     f" enum56={it.get('enum_56')} f20={it.get('f20', 0):.3f} arr={it.get('u32_array')}")
    return "\n".join(lines)


ENUMS = {
    "disp": ["FROMANIM", "FROMPHYSICS", "FROMAI"],
    "feet": ["NOTSET", "LEFTAHEAD", "RIGHTAHEAD", "PARALLEL"],
    "blend": ["NONE", "AROLLBROLL", "ASTOPBROLL", "AROLLBSTOP", "ASTOPBSTOP", "ASTOPBSTOPCUBICQUAT", "6", "7"],
    "bpos": ["FROMSTART", "PROGRESSIVE", "INVPROGRESSIVE", "FROMTARGETTIME"],
    "dispsrc": ["BLENDAB", "FROMAONLY", "FROMBONLY", "3"],
    "acuator": ["FROMA", "FROMB", "2", "3"],
    "torso": ["NO_USE", "FROMACTION", "FROMAI"],
}


def fbits(u):
    return struct.unpack("<f", struct.pack("<I", u))[0]


def fmt_blend(b):
    if not b:
        return "-"
    return (f"{ENUMS['blend'][b['type']]} {b['f0']:.3f}s bpos={ENUMS['bpos'][b['e6']]} disp={ENUMS['dispsrc'][b['e8']]} "
            f"acuator={ENUMS['acuator'][b['e10']]} f4={b['f4']:.3f} f8={b['f8']:.3f}")


def dump_block_text(name, actions, objects, anim, owner):
    out = [f"### {name}"]
    for aid in sorted(k for k, v in owner.items() if v == name):
        a = actions[aid]
        out.append(f"{aid:#010x} torso={ENUMS['torso'][a['enum_24'] % 3]} flags={''.join(map(str, a['flags']))} u28={a['u28']} u32={a['u32']}")
        for tn in ("transition_a", "transition_b"):
            t = a[tn]
            if t and "ref" not in t:
                out.append(f"    {tn}: -> {t['target_a']:#x} [{fmt_blend(t['blend_a'])}]  -> {t['target_b']:#x} [{fmt_blend(t['blend_b'])}]")
        for it in a["items"]:
            if it is None:
                continue
            if "ref" in it:
                it = objects.get(it["ref"], it)
            clips = [anim.get(x, f"?{x:08x}") for x in it.get("animations", [])]
            w = [round(fbits(x), 3) for x in it.get("u32_array", [])]
            out.append(f"    item {it['id']:#010x}: {clips} weights={w} disp={ENUMS['disp'][it['enum_56'] % 3]} "
                       f"feet={ENUMS['feet'][it['bits_0_1']]}->{ENUMS['feet'][it['bits_2_3']]} f20={it['f20']:.3f} "
                       f"flags={''.join(map(str, it['flags']))} b62={it['b62']}")
            out.append(f"         blend: {fmt_blend(it['blend'])}")
            for t in it.get("transitions", []):
                if t and "ref" not in t:
                    out.append(f"         transition -> {t['target_a']:#x} [{fmt_blend(t['blend_a'])}] / {t['target_b']:#x} [{fmt_blend(t['blend_b'])}]")
    return "\n".join(out)
