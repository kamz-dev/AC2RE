// abilities.c - the player's ability stack (AssassinAbilitySet, RE/18 §6, RE/05 §5.7) and the lab's unlock.
//
// The interpreter asks `AbilityStack__Allows` 0xEF0320 before a request (e.g. the air grab, 0xEDBC31): the stack is at
// +0x290 of the object returned by vt176 (0xEDBC1E) of the actor's BhvAssassin (`sub_ED9790`, the interpreter's
// behaviour getter). The stack is {base set*, pushed list*, count & 0x3FFF}; an ability is allowed when every set has
// AllActions (+0x14 bit 1) and its flag (+8, a 64-bit set in reflection order: 0 Jump ... 6 Grasp ... 15 LeapOfFaith).
// The test save's base set lacks Grasp and six other flags (0x7FA9BFBF): its story has not unlocked them yet.
//
// lab.ini [Lab] UnlockAbilities=1 ORs AbilityMask (hex, default all 34 flags) into every set on the stack each frame
// and sets HighProfile + AllActions, so the save can do everything. Only memory changes; a save written while it is on
// may keep the unlocked set. Bridge: `abilities` (report), `abilities on|off`, `abilities mask <hex>`.
#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uintptr_t(__fastcall *ThisGet_t)(uintptr_t self, uintptr_t edx);   // a thiscall with no stack arguments

#define FN_INTERP_BHV 0xED9790u   // GoAssassinActionInterpreter -> the actor's BhvAssassin
#define VT_ABILITY_OWNER 176u     // BhvAssassin vtable slot: the object holding the stack (0xEDBC1E)
#define STACK_OFF 0x290u
#define SET_FLAGS 8u
#define SET_BITS 0x14u            // bit 0 HighProfile, bit 1 AllActions

static volatile LONG g_unlock;
static uint64_t g_mask = 0x3FFFFFFFFull;
static volatile uintptr_t g_stack;      // the last resolved stack (read by the bridge without calling game code)
static uintptr_t g_logged[8];

void abilities_init(const char *ini)
{
    char buf[64];
    g_unlock = GetPrivateProfileIntA("Lab", "UnlockAbilities", 0, ini);
    GetPrivateProfileStringA("Lab", "AbilityMask", "3FFFFFFFF", buf, sizeof(buf), ini);
    g_mask = _strtoui64(buf, NULL, 16);
    lab_log("abilities: unlock %s, mask %llX", g_unlock ? "on" : "off", (unsigned long long)g_mask);
}

static uintptr_t resolve(uintptr_t interp)
{
    uintptr_t bhv = 0, owner = 0, vt = 0, fn = 0;
    __try {
        bhv = ((ThisGet_t)FN_INTERP_BHV)(interp, 0);
        if (!bhv) return 0;
        vt = *(uintptr_t *)bhv;
        fn = *(uintptr_t *)(vt + VT_ABILITY_OWNER);
        owner = ((ThisGet_t)fn)(bhv, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return owner ? owner + STACK_OFF : 0;
}

static void unlock_set(uintptr_t set)
{
    uint64_t flags = 0, want;
    uint8_t bits = 0, want_bits;
    int i;
    if (!set || !game_read(set + SET_FLAGS, &flags, 8) || !game_read(set + SET_BITS, &bits, 1)) return;
    want = flags | g_mask;
    want_bits = bits | 3;
    if (want == flags && want_bits == bits) return;
    game_write(set + SET_FLAGS, &want, 8);
    game_write(set + SET_BITS, &want_bits, 1);
    for (i = 0; i < 8 && g_logged[i] != set; i++)
        if (!g_logged[i]) {
            g_logged[i] = set;
            lab_log("abilities: set %08X flags %llX -> %llX, bits %02X -> %02X", (unsigned)set,
                    (unsigned long long)flags, (unsigned long long)want, bits, want_bits);
            trace_event("ABILITIES set %08X flags %llX -> %llX", (unsigned)set, (unsigned long long)flags, (unsigned long long)want);
            break;
        }
}

// Game thread, from the interpreter's Update hook (before its update reads the stack).
void abilities_frame(uintptr_t interp)
{
    uintptr_t stack = resolve(interp), base = 0, list = 0, set;
    uint32_t count = 0, i;
    g_stack = stack;
    if (!stack || !g_unlock) return;
    game_read(stack, &base, 4);
    game_read(stack + 4, &list, 4);
    game_read(stack + 8, &count, 4);
    unlock_set(base);
    for (i = 0; i < (count & 0x3FFF); i++)
        if (game_read(list + 4 * i, &set, 4)) unlock_set(set);
}

void abilities_command(const char *args, char *r, size_t n)
{
    uintptr_t stack = g_stack, base = 0, list = 0, set;
    uint32_t count = 0, i;
    uint64_t flags = 0;
    uint8_t bits = 0;
    size_t k;
    while (*args == ' ') args++;
    if (!strcmp(args, "on")) InterlockedExchange(&g_unlock, 1);
    else if (!strcmp(args, "off")) InterlockedExchange(&g_unlock, 0);
    else if (!strncmp(args, "mask ", 5)) g_mask = _strtoui64(args + 5, NULL, 16);
    k = (size_t)sprintf_s(r, n, "unlock=%d mask=%llX stack=%08X", (int)g_unlock, (unsigned long long)g_mask, (unsigned)stack);
    if (!stack) return;
    game_read(stack, &base, 4);
    game_read(stack + 4, &list, 4);
    game_read(stack + 8, &count, 4);
    count &= 0x3FFF;
    if (base && game_read(base + SET_FLAGS, &flags, 8) && game_read(base + SET_BITS, &bits, 1))
        k += (size_t)sprintf_s(r + k, n - k, " base=%08X flags=%llX bits=%02X grasp=%d", (unsigned)base,
                               (unsigned long long)(flags & 0x3FFFFFFFFull), bits, (int)(flags >> 6 & 1));
    k += (size_t)sprintf_s(r + k, n - k, " pushed=%u", count);
    for (i = 0; i < count && i < 8 && k < n - 64; i++)
        if (game_read(list + 4 * i, &set, 4) && game_read(set + SET_FLAGS, &flags, 8) && game_read(set + SET_BITS, &bits, 1))
            k += (size_t)sprintf_s(r + k, n - k, " [%u]=%08X flags=%llX bits=%02X", i, (unsigned)set,
                                   (unsigned long long)(flags & 0x3FFFFFFFFull), bits);
}
