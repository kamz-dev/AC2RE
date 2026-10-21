"""Recover hashed reflection property names (CRC32 of plain name) by dictionary combination."""
import re, zlib, io, contextlib, json, sys, itertools, collections
from classes import *

from paths import DATA
OUT = str(DATA / "recovered_property_names.json")
names = ["HumanData", "HumanGroundData", "HumanInAirData", "HumanLedgeData", "HumanClimbData", "HumanLadderData",
         "HumanPoleData", "HumanRopeData", "HumanWallingData", "HumanNarrowObjectData", "HumanLookAtData",
         "HumanPushableContextData", "StPadControlledData", "NavigationContextData", "MainNavigationData",
         "AssassinAbilitySet", "HumanHayStackData", "HumanKioskData", "HumanDeadData", "HumanCustomActionData",
         "HumanDataBundle"]
want = {}
for n in names:
    with contextlib.redirect_stdout(io.StringIO()):
        r = dump_class(n, verbose=False)
    if r:
        for w in r["props"]:
            want.setdefault(w[1], []).append((n, w[4] >> 18, w[3] >> 16))
wantset = set(want)

# vocabulary: CamelCase words from identifiers in the exe + hand-picked movement words
idents = set(m.group().decode() for m in re.finditer(rb"[A-Za-z][A-Za-z0-9_]{2,60}", D))
cnt = collections.Counter()
for s in idents:
    for w in re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])|\d+", s.replace("_", " ")):
        if 1 < len(w) < 16:
            cnt[w[0].upper() + w[1:]] += 1
extra = """Min Max Speed Distance Dist Height Angle Time Timer Duration Delay Offset Position Pos Direction Dir
Velocity Vel Accel Acceleration Decel Deceleration Turn Rate Radius Length Width Target Current Previous Last
Next Start End Entry Exit Jump Fall Land Landing Climb Ledge Hand Foot Feet Left Right Up Down Forward Back
Side Wall Beam Pole Rope Ladder Grab Grasp Hang Free Swing Rebound Reception Pull Push Pullup Drop Reach
Normal Up Vector Matrix Anim Animation Blend Weight IK Hips Pelvis Root Head Is Has Can Use Enable Enabled
Allow Force Count Nb Num Index Id ID Type State SubState Mode Flag Flags Valid Invalid Threshold Tolerance
Gravity Impulse Momentum Slope Step Stair Stairs Ground Air Surface Contact Point Normal Edge Corner Grid
Cell Size Range Precision Speed Walk Run Jog Sprint Sneak Crouch Stop Pivot Lean Balance Unbalanced
Obstacle Collision Crowd Npc Target Object Entity Guidance Report Handle Link Chain Connector Vertex
Height Low High Top Bottom Middle Center Front Rear Inner Outer Local World Global Delta Ratio Factor
Scale Coef Coefficient Smooth Smoothing Damping Stiffness Spring Desired Wanted Requested Input Stick
Pad Button Pressed Camera Orientation Rotation Yaw Pitch Roll Heading Facing Look Rebound Parallel
Pose Pos Knee Waist Shoulder Arm Elbow Wrist Toe Heel Leg Body Upper Lower Spine""".split()
for w in extra:
    cnt[w] += 1000
vocab = [w for w, c in cnt.most_common(2500)]
small = [w for w, c in cnt.most_common(400)]
prefixes = ["", "m_", "m", "b", "f", "i", "Is", "Use", "Max", "Min", "Nb"]

found = {}
try:
    found = {int(k, 16): v for k, v in json.load(open(OUT)).items()}
except Exception:
    pass


def test(s):
    h = zlib.crc32(s.encode()) & 0xffffffff
    if h in wantset and h not in found:
        found[h] = s
        print("found", f"{h:08x}", s, want[h][0], flush=True)


for w in vocab:
    for p in prefixes:
        test(p + w)
        test(p + w.lower())
for a in vocab:
    for b in vocab:
        s = a + b
        test(s)
        test("m_" + s)
        test(a + "_" + b)
        test(a.lower() + b)
        test("m" + s)
for a in small:
    for b in small:
        ab = a + b
        for c in small:
            test(ab + c)
            test("m_" + ab + c)

json.dump({f"{h:08x}": n for h, n in found.items()}, open(OUT, "w"), indent=1)
print(len(found), "of", len(want), "recovered")
