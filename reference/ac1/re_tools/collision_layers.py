"""Dump the collision layer matrix the exe builds in CollisionFilterHook::Init (sub_4DC8A0).

Every layer pair starts enabled (sub_4DC560: 50 x u64 = -1); each call to sub_4DBD20(a, b) disables a <-> b.
The calls are `push b; push a; mov ecx, reg; call 0x4DBD20` (push imm8 / imm32), found by scanning the function body.
Writes RE/data/collision_layers.json: names, the disabled pairs and, per layer, the layers it collides with.
"""
import json
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paths import DATA  # noqa: E402
import pe  # noqa: E402

FUNC, SIZE, DISABLE = 0x4DC8A0, 0x2379, 0x4DBD20
NAMES = ['LayerNone', 'Static', 'Dynamic', 'Debris', 'DetectionZone', 'Character', 'Ragdoll', 'Raycast',
         'NoCharacterCollision', 'RaycastOnlyCharacters', 'Nothing', 'Interactive', 'CharacterNoInteractive',
         'OnlyStatic', 'RaycastNoCharacters', 'Horse', 'HorseBarrier', 'Camera', 'CameraBarrier', 'StaticNoCamera',
         'CharacterNoStatic', 'MainCharacter', 'MainCharacterNoInteractive', 'MainCharacterNoStatic', 'FuzzyZone',
         'FuzzyZoneNoCamera', 'FuzzyZoneAllCharacter', 'OnlyStaticAndCameraRaycast', 'HollywoodMode', 'StaticNoLOS',
         'StaticNoCameraNoLOS', 'StaticNoHorse', 'OnlyLOS', 'FuzzyZoneAllCharacterNoCamera', 'NPCBarrier',
         'OcclusionHeightTest', 'CameraBarrierPlusLOS', 'Projectile', 'ProjectileBarrier', 'ProjectileCameraBarrier',
         'OnlyLOSAndFuzzy', 'FuzzyZoneNoCameraAllowAssassination', 'HorseNoMainCharacterCollision',
         'RaycastNoCharactersFuzzyZone', 'OnlyDynamic', 'OnlyFuzzy', 'NPCSpawning', 'RaycastFuzzyZone',
         'NoHorseCollision', 'RagdollPenetrationSolver']

code = bytes(pe.u8(FUNC + i) for i in range(SIZE)) if hasattr(pe, 'u8') else None
if code is None:
    code = b''.join(struct.pack('<I', pe.u32(FUNC + i)) for i in range(0, SIZE + 4, 4))[:SIZE]


def pushes_before(i):
    """The immediates of the pushes before offset i, nearest first (skips a 2-byte mov ecx, reg)."""
    j = i - 2 if code[i - 2] in (0x8B, 0x89) else i
    out = []
    while len(out) < 2:
        if code[j - 2] == 0x6A:
            out.append(code[j - 1])
            j -= 2
        elif code[j - 5] == 0x68:
            out.append(struct.unpack_from('<I', code, j - 4)[0])
            j -= 5
        elif 0x53 <= code[j - 1] <= 0x57 and code[j - 1] != 0x54:
            # push ebx / ebp / esi / edi holding a constant: the nearest `mov reg, imm32` before it
            r = code[j - 1] - 0x50
            k = j - 1
            while k >= 5 and not (code[k - 5] == 0xB8 + r and struct.unpack_from('<I', code, k - 4)[0] < 50):
                k -= 1
            if k < 5:
                return None
            out.append(struct.unpack_from('<I', code, k - 4)[0])
            j -= 1
        else:
            return None
    return out


pairs, misses = [], 0
for i in range(SIZE - 5):
    if code[i] == 0xE8 and FUNC + i + 5 + struct.unpack_from('<i', code, i + 1)[0] == DISABLE:
        p = pushes_before(i)
        if p is None:
            misses += 1
            continue
        a, b = p  # nearest push = first argument
        if max(a, b) >= 50:
            print("bad", hex(FUNC + i), a, b, code[i - 16:i].hex(" "))
            misses += 1
            continue
        pairs.append((a, b))

mask = [(1 << 50) - 1] * 50
for a, b in pairs:
    mask[a] &= ~(1 << b)
    mask[b] &= ~(1 << a)
out = {
    'source': 'sub_4DC8A0 -> sub_4DBD20(a, b) disables a <-> b; every other pair collides',
    'names': NAMES,
    'disabled_pairs': [[a, b] for a, b in pairs],
    'collides_with': {NAMES[l]: [NAMES[m] for m in range(50) if mask[l] >> m & 1] for l in range(50)},
}
json.dump(out, open(DATA / 'collision_layers.json', 'w'), indent=1)
print(len(pairs), 'pairs,', misses, 'unparsed calls')
for l in (1, 5, 7, 13, 14, 17, 21):
    print(NAMES[l], '->', out['collides_with'][NAMES[l]])
