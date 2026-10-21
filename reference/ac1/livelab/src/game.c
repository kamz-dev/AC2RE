// Reading the game: the player, its entity, contexts, action and controller.
#include "lab.h"
#include <stdlib.h>
#include <string.h>

typedef uintptr_t(__fastcall *This0_t)(uintptr_t self, void *edx);
typedef uintptr_t(__fastcall *This1_t)(uintptr_t self, void *edx, uintptr_t a);

static uintptr_t g_entity, g_human, g_controller;
static uintptr_t seen[1024];   // Humans already tested by game_consider_human (cleared when the player goes)
static int next;

int game_read(uintptr_t addr, void *out, size_t n)
{
    if (addr < 0x10000) return 0;
    __try {
        memcpy(out, (const void *)addr, n);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int game_write(uintptr_t addr, const void *src, size_t n)
{
    DWORD prot;
    if (addr < 0x10000) return 0;
    __try {
        memcpy((void *)addr, src, n);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    // read-only page (code / vtables): unprotect for the write
    if (!VirtualProtect((void *)addr, n, PAGE_EXECUTE_READWRITE, &prot)) return 0;
    __try {
        memcpy((void *)addr, src, n);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualProtect((void *)addr, n, prot, &prot);
        return 0;
    }
    VirtualProtect((void *)addr, n, prot, &prot);
    FlushInstructionCache(GetCurrentProcess(), (void *)addr, n);
    return 1;
}

uint32_t game_u32(uintptr_t addr)
{
    uint32_t v = 0;
    game_read(addr, &v, 4);
    return v;
}

float game_f32(uintptr_t addr)
{
    float v = 0;
    game_read(addr, &v, 4);
    return v;
}

// MSVC RTTI: vtable[-1] = complete object locator, +12 = type descriptor, +8 = ".?AVName@scimitar@@".
const char *game_rtti(uintptr_t obj, char *out, size_t n)
{
    uintptr_t vt = game_u32(obj), col = vt ? game_u32(vt - 4) : 0, td = col ? game_u32(col + 12) : 0;
    char raw[96];
    char *at;
    out[0] = 0;
    if (!td || !game_read(td + 8, raw, sizeof(raw))) return out;
    raw[sizeof(raw) - 1] = 0;
    if (strncmp(raw, ".?AV", 4) && strncmp(raw, ".?AU", 4)) return out;
    strncpy_s(out, n, raw + 4, _TRUNCATE);
    if ((at = strstr(out, "@@"))) *at = 0;
    return out;
}

// The first component of the entity whose class name starts with `name` (RTTI, "CharacterController@scimitar").
uintptr_t game_entity_component_named(uintptr_t entity, const char *name)
{
    uintptr_t arr = game_u32(entity + 116);
    uint32_t n = game_u32(entity + 120) & 0x3FFF, i;
    char cls[96];
    if (!arr || n > 256) return 0;
    for (i = 0; i < n; i++) {
        uintptr_t c = game_u32(arr + 4 * i);
        if (c && !strncmp(game_rtti(c, cls, sizeof(cls)), name, strlen(name))) return c;
    }
    return 0;
}

// Game__GetWorld 0x419880: *(0x1A1198C)+0x48 is the current scimitar::World (+128 its death height), not the player.
uintptr_t game_world(void)
{
    uintptr_t game = game_u32(G_WORLD);
    return game ? game_u32(game + 0x48) : 0;
}

// The player's entity, found by game_consider_human.
uintptr_t game_player_entity(void) { return g_entity; }

// Human vt20 (+80) -> the component handle whose +4 is the owner entity (Human__CharacterLayer 0xB0EAA0).
static uintptr_t human_entity(uintptr_t human)
{
    uintptr_t vt = game_u32(human), h = 0;
    if (!vt) return 0;
    __try {
        h = ((This0_t)game_u32(vt + 80))(human, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        h = 0;
    }
    return h ? game_u32(h + 4) : 0;
}

// Called from the Human__PostUpdate hook with every Human (game thread). The player is the main character: its
// entity's +168 & 7 == 1 (Human__CharacterLayer 0xB0EAA0 maps its layers 5 / 12 / 20 to 21 / 22 / 23).
int game_consider_human(uintptr_t h)
{
    static uint32_t checked;
    uintptr_t e;
    uint8_t flags = 0;
    if (h == g_human) {
        // re-validate now and then (a level load frees the old Human)
        if (++checked % 120 == 0) {
            e = human_entity(h);
            if (!e || !game_read(e + 168, &flags, 1) || (flags & 7) != 1) {
                lab_log("player Human %08X no longer valid", (unsigned)h);
                g_human = g_entity = g_controller = 0;
                memset(seen, 0, sizeof(seen));
                return 0;
            }
        }
        return 1;
    }
    {
        // each other Human is tested once (a ring of the ones already seen); during a load a placeholder main
        // character exists next to the real one, so the newest main character wins
        int i;
        for (i = 0; i < 1024; i++)
            if (seen[i] == h) return 0;
        seen[next] = h;
        next = (next + 1) % 1024;
    }
    e = human_entity(h);
    if (!e || !game_read(e + 168, &flags, 1) || (flags & 7) != 1) return 0;
    g_human = h;
    g_entity = e;
    g_controller = game_entity_component_named(e, "CharacterComponent@");
    lab_log("player: Human %08X entity %08X controller %08X", (unsigned)h, (unsigned)e, (unsigned)g_controller);
    return 1;
}

// The entity's component array is at +116 (sub_44F8E0 / sub_7183F0 index it); the scimitar array keeps its count
// in the low 14 bits of the next dword.
uintptr_t game_entity_component(uintptr_t entity, uintptr_t vtable)
{
    uintptr_t arr = game_u32(entity + 116);
    uint32_t n = game_u32(entity + 120) & 0x3FFF, i;
    if (!arr) return 0;
    if (n == 0 || n > 256) n = 64;
    for (i = 0; i < n; i++) {
        uintptr_t c = game_u32(arr + 4 * i);
        if (c && game_u32(c) == vtable) return c;
    }
    return 0;
}

uintptr_t game_player_human(void) { return g_human; }

// once per pad update: drop a freed Human (its vtable is gone)
void game_refresh(void)
{
    if (g_human && game_u32(g_human) != VT_HUMAN) {
        lab_log("player Human %08X freed", (unsigned)g_human);
        g_human = g_entity = g_controller = 0;
        memset(seen, 0, sizeof(seen));
    }
    if (g_entity && !g_controller) g_controller = game_entity_component_named(g_entity, "CharacterComponent@");
}

// Human vt20 (+80) -> component handle; sub_7183F0 -> the anim component (as in sub_DD0B60 / HumanGround__CodeTurnsBody)
static uintptr_t anim_component(uintptr_t human)
{
    uintptr_t vt = game_u32(human), h, c = 0;
    if (!vt) return 0;
    __try {
        h = ((This0_t)game_u32(vt + 80))(human, 0);
        if (h) c = ((This0_t)FN_ENTITY_COMPONENT_BY_SLOT)(h, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        c = 0;
    }
    return c;
}

void game_sample(LabFrame *f, uintptr_t human)
{
    uintptr_t ctx, data, comp;
    float m[16];
    memset(f, 0, sizeof(*f));
    f->frame = g_lab.frames;
    f->time = lab_now();
    f->dt = game_f32(G_FRAME_DT);
    f->scale = control_get_scale();
    f->human = human;
    f->entity = g_entity;
    f->controller = g_controller;
    memcpy(f->buttons, g_lab.pad_b, 16);
    memcpy(f->lstick, g_lab.pad_l, 8);
    memcpy(f->rstick, g_lab.pad_r, 8);
    f->injecting = g_lab.pad_inj;
    {
        // the six movement handle bones (Human__SetupBoneHandles 0xB11020), as the contexts read them
        int i;
        for (i = 0; i < 6; i++) {
            __declspec(align(16)) float out[4] = {0, 0, 0, 0};
            __try {
                ((This1_t)FN_BONE_WORLD)(human + 0x280 + 8 * i, 0, (uintptr_t)out);
                f->bones[i][0] = out[0], f->bones[i][1] = out[1], f->bones[i][2] = out[2];
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }
    if (g_lab.interp) {
        game_read(g_lab.interp + 4688, f->move_dir, 12);
        game_read(g_lab.interp + 4672, &f->move_mag, 4);
    }
    // entity world matrix at +0x10 (sub_4311C0 reads row +48 as the translation) unless parented (+0x50)
    if (game_read(g_entity + 0x10, m, sizeof(m))) {
        f->right[0] = m[0], f->right[1] = m[1], f->right[2] = m[2];
        f->fwd[0] = m[4], f->fwd[1] = m[5], f->fwd[2] = m[6];
        f->pos[0] = m[12], f->pos[1] = m[13], f->pos[2] = m[14];
    }
    if (g_controller) game_read(g_controller + 0x90, f->vel, 12);
    else {
        // no controller found: the position change over the frame
        static float last[3];
        static double last_t;
        if (last_t > 0 && f->time > last_t)
            for (int i = 0; i < 3; i++) f->vel[i] = (f->pos[i] - last[i]) / (float)(f->time - last_t);
        memcpy(last, f->pos, 12);
        last_t = f->time;
    }
    // locomotion slot (AIActor slot 1): current context +0x48, previous id +0x44; context +0x10 = its data, data+4 = id
    ctx = game_u32(human + 0x48);
    data = ctx ? game_u32(ctx + 0x10) : 0;
    f->ctx = data ? (int)game_u32(data + 4) : -1;
    f->prev_ctx = (int)game_u32(human + 0x44);
    comp = anim_component(human);
    if (comp) {
        __try {
            uintptr_t act = ((This1_t)FN_SLOT_SELECTED_ACTION)(comp, 0, 0);
            uintptr_t ap = ((This1_t)FN_FIND_SLOT_INSTANCE)(comp, 0, 0);
            if (act) f->action = *(uint32_t *)(act + 8);
            act = ((This1_t)FN_SLOT_SELECTED_ACTION)(comp, 0, 1);
            if (act) f->action1 = *(uint32_t *)(act + 8);
            if (ap) f->item = ((This0_t)FN_PLAYING_ITEM)(ap, 0);
            if (f->item) f->item_word = *(uint16_t *)(f->item + 60);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    {
        // the check's fields
        static char names[LAB_MAX_FIELDS][48], exprs[LAB_MAX_FIELDS][160];
        int n = 0, i;
        LabEnv env;
        memset(&env, 0, sizeof(env));
        env.human = human;
        checks_fields(names, exprs, &n);
        for (i = 0; i < n && i < LAB_MAX_FIELDS; i++)
            if (!expr_format(exprs[i], &env, f->fields[i], sizeof(f->fields[i]))) strcpy_s(f->fields[i], 48, "?");
        f->nfields = n;
    }
}

static const char *CTX[] = {"NoTransition", "Init", "NotDefined", "Debug", "Ground", "Ladder", "Pole", "Rope",
                            "InAir", "Ledge", "Climb", "Walling", "NarrowObject", "Riding", "Pushable",
                            "RagdollGround", "Vocalization", "CustomAction", "LookAt", "Kiosk", "Dead",
                            "HayStack", "UpperBody"};

const char *game_ctx_name(int id)
{
    return id >= 0 && id < (int)(sizeof(CTX) / sizeof(CTX[0])) ? CTX[id] : "?";
}

// names/actions.tsv: "id<TAB>name" (dumped from the install by the port's probe, see README)
typedef struct { uint32_t id; char *name; } Name;
static Name *g_names;
static int g_nnames;

static int cmp_name(const void *a, const void *b)
{
    uint32_t x = ((const Name *)a)->id, y = ((const Name *)b)->id;
    return x < y ? -1 : x > y;
}

void game_load_names(void)
{
    char path[MAX_PATH], line[512];
    FILE *f;
    int cap = 0;
    sprintf_s(path, MAX_PATH, "%s\\names\\actions.tsv", g_lab.dir);
    if (fopen_s(&f, path, "r")) {
        lab_log("no %s: actions show as ids", path);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char *tab = strchr(line, '\t'), *nl;
        if (!tab || line[0] == '#') continue;
        *tab = 0;
        if ((nl = strpbrk(tab + 1, "\r\n"))) *nl = 0;
        if (g_nnames == cap) {
            cap = cap ? cap * 2 : 1024;
            g_names = (Name *)realloc(g_names, cap * sizeof(Name));
        }
        g_names[g_nnames].id = strtoul(line, NULL, 16);
        g_names[g_nnames].name = _strdup(tab + 1);
        g_nnames++;
    }
    fclose(f);
    qsort(g_names, g_nnames, sizeof(Name), cmp_name);
    lab_log("%d action names", g_nnames);
}

const char *game_action_name(uint32_t id)
{
    Name key, *r;
    key.id = id;
    if (!g_nnames) return "";
    r = (Name *)bsearch(&key, g_names, g_nnames, sizeof(Name), cmp_name);
    return r ? r->name : "";
}
