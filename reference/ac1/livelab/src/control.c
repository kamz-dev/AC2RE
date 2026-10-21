// Control: hotkeys, the game clock (slow motion / pause / frame step), position bookmarks and the command set the
// bridge and the hotkeys share. Runs on the game thread (from the pad update hook) except the read-only commands.
//
// The clock is the Engine::BeginFrame task object (*0x1A1197C, sub_417470 -> sub_416420): the frame dt is
// (+12 scale) x (the mode's time), clamped to at least 0.0001 s, then g_FrameDt is set by 0x93E0F0.
#include "lab.h"
#include <tlhelp32.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUEUE 16
typedef struct { char cmd[1024]; char reply[8192]; HANDLE done; int used; } Job;

static Job g_jobs[MAX_QUEUE];
static CRITICAL_SECTION g_qcs;
static int g_qinit;
static float g_scale = 1.0f, g_user_scale = 1.0f;
static int g_paused, g_step;
static uint8_t g_keys[256];

typedef struct { int used; float pos[3], fwd[3]; } Bookmark;
static Bookmark g_marks[10];

void game_refresh(void);

static float *clock_scale_ptr(void)
{
    uintptr_t e = game_u32(G_ENGINE);
    return e ? (float *)(e + 12) : NULL;
}

float control_get_scale(void)
{
    float *p = clock_scale_ptr();
    float v = 1.0f;
    if (p) game_read((uintptr_t)p, &v, 4);
    return v;
}

static void apply_scale(void)
{
    float *p = clock_scale_ptr();
    float v = g_paused && g_step <= 0 ? 0.0f : g_user_scale;
    if (p && v != g_scale) {
        game_write((uintptr_t)p, &v, 4);
        g_scale = v;
    }
}

void control_set_scale(float s)
{
    g_user_scale = s < 0.01f ? 0.01f : s > 4.0f ? 4.0f : s;
    overlay_note("speed x%.2f", g_user_scale);
    lab_log("clock scale %.3f", g_user_scale);
}

void control_pause(int on)
{
    g_paused = on;
    g_step = 0;
    overlay_note(on ? "PAUSED (F10 steps a frame)" : "running");
}

int control_paused(void) { return g_paused; }

void control_step(int frames)
{
    if (!g_paused) control_pause(1);
    g_step = frames;
}

// ---- bookmarks (experimental teleport) ----
// The controller follows the entity with a sweep ((entity - proxy)/dt, CharacterController__BuildVelocity 0x52F150),
// so a jump of the entity alone would collide along the way: the proxy's own copies of the position are found by
// value (the vec4s in the controller equal to the entity translation) and moved too.

static void bookmark_save(int slot)
{
    LabFrame f;
    EnterCriticalSection(&g_lab.lock);
    f = g_lab.last;
    LeaveCriticalSection(&g_lab.lock);
    if (!f.entity) { overlay_note("no player"); return; }
    g_marks[slot].used = 1;
    memcpy(g_marks[slot].pos, f.pos, 12);
    memcpy(g_marks[slot].fwd, f.fwd, 12);
    trace_event("BOOKMARK save %d pos=(%.3f %.3f %.3f)", slot, f.pos[0], f.pos[1], f.pos[2]);
}

// Moves every copy of the position found by value (within 2 cm in x / y, 0.6 m in z) in `obj`'s first `len` bytes
// by `d`. Returns how many it moved.
static int shift_copies(uintptr_t obj, int len, const float cur[3], const float d[3], const char *what)
{
    int off, moved = 0;
    for (off = 0; off + 12 <= len; off += 4) {
        float v[3];
        if (!game_read(obj + off, v, 12)) continue;
        if (fabsf(v[0] - cur[0]) < 0.02f && fabsf(v[1] - cur[1]) < 0.02f && fabsf(v[2] - cur[2]) < 0.6f && fabsf(v[0]) > 1e-3f) {
            v[0] += d[0], v[1] += d[1], v[2] += d[2];
            game_write(obj + off, v, 12);
            moved++;
        }
    }
    if (moved) lab_log("teleport: %s %08X: %d copies", what, (unsigned)obj, moved);
    return moved;
}

// Teleport (game coordinates, Z up; yaw in radians, the facing's angle from +X; NaN keeps it). The character
// controller is the entity's CharacterComponent (the proxy: position copies, velocity +0x90) with its Havok
// hkCachingShapePhantom at +0x70 (position +0xA0, the capsule lifted 0.37 m); all copies move by the same delta.
static int teleport(const float to[3], float yaw, char *reply, size_t n)
{
    uintptr_t e = game_player_entity(), c = e ? game_entity_component_named(e, "CharacterComponent@") : 0, ph = 0;
    float cur[3], d[3], zero[4] = {0, 0, 0, 0};
    int moved = 0;
    if (!e || !game_read(e + 0x40, cur, 12)) { sprintf_s(reply, n, "no player"); return 0; }
    d[0] = to[0] - cur[0], d[1] = to[1] - cur[1], d[2] = to[2] - cur[2];
    if (c) {
        moved += shift_copies(c, 0x800, cur, d, "controller");
        ph = game_u32(c + 0x70);
        if (ph) moved += shift_copies(ph, 0x200, cur, d, "phantom");
        game_write(c + 0x90, zero, 16);  // velocity
    }
    game_write(e + 0x40, to, 12);
    if (yaw == yaw) {
        // rows: 0 right, 1 forward (sub_4311C0's matrix at +0x10), Z up
        float right[3] = {sinf(yaw), -cosf(yaw), 0}, fwd[3] = {cosf(yaw), sinf(yaw), 0};
        game_write(e + 0x10, right, 12);
        game_write(e + 0x20, fwd, 12);
    }
    sprintf_s(reply, n, "teleported to (%.2f %.2f %.2f) from (%.2f %.2f %.2f), %d controller copies", to[0], to[1], to[2],
              cur[0], cur[1], cur[2], moved);
    trace_event("TELEPORT %s", reply);
    return 1;
}

static int bookmark_load(int slot, char *reply, size_t n)
{
    if (!g_marks[slot].used) { sprintf_s(reply, n, "no bookmark %d", slot); return 0; }
    return teleport(g_marks[slot].pos, atan2f(g_marks[slot].fwd[1], g_marks[slot].fwd[0]), reply, n);
}

// ---- commands ----

static int read_only(const char *cmd)
{
    return !strncmp(cmd, "status", 6) || !strncmp(cmd, "read ", 5) || !strncmp(cmd, "scan ", 5) || !strncmp(cmd, "eval ", 5) ||
           !strncmp(cmd, "probes", 6) || (!strncmp(cmd, "checks", 6) && strcmp(cmd, "checks reload")) || !strncmp(cmd, "frame", 5) || !strncmp(cmd, "shot", 4) || !strcmp(cmd, "overlay") || !strcmp(cmd, "stacks") || !strcmp(cmd, "modules") || !strncmp(cmd, "rtti ", 5) || !strncmp(cmd, "comps", 5);
}

static void status(char *r, size_t n)
{
    LabFrame f;
    const LabCheck *c = checks_current();
    EnterCriticalSection(&g_lab.lock);
    f = g_lab.last;
    LeaveCriticalSection(&g_lab.lock);
    sprintf_s(r, n,
              "frames=%u entity=%08X human=%08X controller=%08X pos=(%.3f %.3f %.3f) vel=(%.3f %.3f %.3f) ctx=%s(%d) "
              "prev=%d action=%08X %s word=%04X dt=%.5f scale=%.3f paused=%d rec=%d play=%d inrec=%d check=%s probes=%d "
              "pad_updates=%ld presents=%ld clock=%.3f",
              g_lab.frames, (unsigned)f.entity, (unsigned)f.human, (unsigned)f.controller, f.pos[0], f.pos[1], f.pos[2],
              f.vel[0], f.vel[1], f.vel[2], game_ctx_name(f.ctx), f.ctx, f.prev_ctx, f.action, game_action_name(f.action),
              f.item_word, f.dt, f.scale, g_paused, trace_active(), input_playing(), input_recording(),
              c ? c->id : "-", probes_count(), g_lab.pad_updates, g_lab.presents, control_get_scale());
}

void control_command(const char *cmd, char *r, size_t n)
{
    char a[1024];
    r[0] = 0;
    while (*cmd == ' ') cmd++;
    strcpy_s(a, sizeof(a), cmd);
    if (!strncmp(a, "status", 6)) status(r, n);
    else if (!strncmp(a, "abilities", 9)) abilities_command(a + 9, r, n);
    else if (!strncmp(a, "frame", 5)) {
        LabFrame f;
        int i;
        size_t k;
        EnterCriticalSection(&g_lab.lock);
        f = g_lab.last;
        LeaveCriticalSection(&g_lab.lock);
        k = sprintf_s(r, n, "fields:");
        for (i = 0; i < f.nfields; i++) k += sprintf_s(r + k, n - k, " %s", f.fields[i]);
    }
    else if (!strncmp(a, "read ", 5)) {
        char *end;
        uintptr_t addr = strtoul(a + 5, &end, 16);
        int len = atoi(end), i;
        uint8_t buf[256];
        size_t k = 0;
        if (len <= 0 || len > 256) len = 16;
        if (!game_read(addr, buf, len)) { sprintf_s(r, n, "unreadable"); return; }
        for (i = 0; i < len; i++) k += sprintf_s(r + k, n - k, "%02X%s", buf[i], (i % 16 == 15) ? "\n" : " ");
    }
    else if (!strncmp(a, "scan ", 5)) {
        // scan <hex dword> [max]: the 4-aligned addresses in committed readable memory holding that dword (e.g. a
        // vtable, to list every object of a class)
        char *end;
        uint32_t want = strtoul(a + 5, &end, 16);
        int max = atoi(end), hits = 0;
        uintptr_t p = 0x10000;
        size_t k = 0;
        MEMORY_BASIC_INFORMATION mi;
        if (max <= 0 || max > 400) max = 100;
        while (p < 0x7FFF0000 && VirtualQuery((void *)p, &mi, sizeof(mi)) && hits < max) {
            uintptr_t lo = (uintptr_t)mi.BaseAddress, hi = lo + mi.RegionSize, q;
            DWORD pr = mi.Protect & 0xFF;
            if (mi.State == MEM_COMMIT && !(mi.Protect & PAGE_GUARD) && pr != PAGE_NOACCESS && pr != PAGE_EXECUTE) {
                __try {
                    for (q = lo; q + 4 <= hi && hits < max; q += 4)
                        if (*(const uint32_t *)q == want) { k += sprintf_s(r + k, n - k, "%08X ", (unsigned)q); hits++; }
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            p = hi;
        }
        if (!hits) sprintf_s(r, n, "none");
    }
    else if (!strncmp(a, "eval ", 5)) {
        LabEnv env;
        memset(&env, 0, sizeof(env));
        if (!expr_format(a + 5, &env, r, n)) strcat_s(r, n, "  (some parts unreadable)");
    }
    else if (!strncmp(a, "write ", 6)) {
        // write <addr> <hex bytes...>
        char *p, *ctx = NULL;
        uintptr_t addr;
        uint8_t buf[256];
        int len = 0;
        p = strtok_s(a + 6, " ", &ctx);
        addr = strtoul(p ? p : "0", NULL, 16);
        while ((p = strtok_s(NULL, " ", &ctx)) && len < 256) buf[len++] = (uint8_t)strtoul(p, NULL, 16);
        sprintf_s(r, n, game_write(addr, buf, len) ? "wrote %d bytes" : "write failed", len);
        trace_event("WRITE %08X %d bytes", (unsigned)addr, len);
    }
    else if (!strncmp(a, "writef ", 7)) {
        char *end;
        uintptr_t addr = strtoul(a + 7, &end, 16);
        float v = (float)atof(end);
        sprintf_s(r, n, game_write(addr, &v, 4) ? "ok" : "write failed");
        trace_event("WRITEF %08X %f", (unsigned)addr, v);
    }
    else if (!strncmp(a, "scale ", 6)) { control_set_scale((float)atof(a + 6)); sprintf_s(r, n, "scale %.3f", g_user_scale); }
    else if (!strcmp(a, "pause")) { control_pause(1); strcpy_s(r, n, "paused"); }
    else if (!strcmp(a, "resume")) { control_pause(0); strcpy_s(r, n, "running"); }
    else if (!strncmp(a, "step", 4)) { int k = atoi(a + 4); control_step(k > 0 ? k : 1); sprintf_s(r, n, "step %d", k > 0 ? k : 1); }
    else if (!strncmp(a, "rec start", 9)) { trace_start(a[9] ? a + 10 : "rec"); strcpy_s(r, n, "recording"); }
    else if (!strncmp(a, "rec stop", 8)) { trace_stop(); strcpy_s(r, n, "stopped"); }
    else if (!strncmp(a, "mark", 4)) { trace_mark(a + 4); strcpy_s(r, n, "marked"); }
    else if (!strncmp(a, "check ", 6)) {
        if (!strcmp(a + 6, "next")) checks_select(1);
        else if (!strcmp(a + 6, "prev")) checks_select(-1);
        else checks_select_id(a + 6);
        sprintf_s(r, n, "check %s", checks_current() ? checks_current()->id : "-");
    }
    else if (!strcmp(a, "checks reload")) { checks_load(); sprintf_s(r, n, "%d checks", checks_count()); }
    else if (!strncmp(a, "field add ", 10)) { checks_field_add(a + 10); strcpy_s(r, n, "field added (recordings started from now carry it)"); }
    else if (!strcmp(a, "field clear")) { checks_field_clear(); strcpy_s(r, n, "fields cleared"); }
    else if (!strncmp(a, "checks", 6)) sprintf_s(r, n, "%d checks, current %s", checks_count(), checks_current() ? checks_current()->id : "-");
    else if (!strncmp(a, "probe add ", 10)) sprintf_s(r, n, "%d armed", probes_load_text(a + 10, "bridge"));
    else if (!strcmp(a, "probe clear")) { probes_clear(); strcpy_s(r, n, "cleared"); }
    else if (!strncmp(a, "probes", 6)) probes_status(r, n);
    else if (!strncmp(a, "play ", 5)) {
        char path[MAX_PATH];
        if (a[5] && a[6] == ':') strcpy_s(path, MAX_PATH, a + 5);
        else sprintf_s(path, MAX_PATH, "%s\\%s", g_lab.dir, a + 5);
        sprintf_s(r, n, input_play_file(path) ? "playing %s" : "cannot play %s", path);
    }
    else if (!strcmp(a, "stop")) { input_stop(); strcpy_s(r, n, "stopped"); }
    else if (!strcmp(a, "inputrec on")) { input_record(1); strcpy_s(r, n, "pad recording on"); }
    else if (!strcmp(a, "inputrec off")) { input_record(0); strcpy_s(r, n, "pad recording off"); }
    else if (!strncmp(a, "tp ", 3)) {
        // tp x y z [yaw degrees]   (game coordinates; yaw = the facing's angle from +X)
        float p[3] = {0, 0, 0}, yaw = NAN, deg;
        int k = sscanf_s(a + 3, "%f %f %f %f", &p[0], &p[1], &p[2], &deg);
        if (k < 3) sprintf_s(r, n, "tp x y z [yaw degrees]");
        else {
            if (k == 4) yaw = deg * 3.14159265f / 180.0f;
            teleport(p, yaw, r, n);
        }
    }
    else if (!strncmp(a, "save ", 5)) { bookmark_save(atoi(a + 5) % 10); sprintf_s(r, n, "saved %d", atoi(a + 5) % 10); }
    else if (!strncmp(a, "load ", 5)) bookmark_load(atoi(a + 5) % 10, r, n);
    else if (!strncmp(a, "shot", 4)) {
        // shot [path]: the next frame's back buffer as a BMP (half size); default sessions/<..>/shot_<n>.bmp
        // "shot ui [path]" includes the overlay
        static int k;
        int ui = !strncmp(a, "shot ui", 7);
        const char *rest = a + (ui ? 7 : 4);
        if (*rest == ' ' && rest[1]) strcpy_s(g_lab.shot_path, MAX_PATH, rest + 1);
        else { char name[32]; sprintf_s(name, sizeof(name), "shot_%03d.bmp", ++k); lab_session_path(g_lab.shot_path, MAX_PATH, name); }
        InterlockedExchange(&g_lab.shot_pending, ui ? 2 : 1);
        sprintf_s(r, n, "%s", g_lab.shot_path);
    }
    else if (!strncmp(a, "rtti ", 5)) {
        LabEnv env;
        uint64_t v;
        char cls[96];
        memset(&env, 0, sizeof(env));
        if (!expr_eval(a + 5, &env, &v)) sprintf_s(r, n, "bad expression");
        else sprintf_s(r, n, "%08X %s", (unsigned)v, game_rtti((uintptr_t)v, cls, sizeof(cls)));
    }
    else if (!strncmp(a, "comps", 5)) {
        // the components of an entity (default: the player's): index, address, class
        LabEnv env;
        uint64_t v = game_player_entity();
        uintptr_t arr;
        uint32_t cnt, i;
        size_t k = 0;
        char cls[96];
        memset(&env, 0, sizeof(env));
        if (a[5] == ' ') expr_eval(a + 6, &env, &v);
        arr = game_u32((uintptr_t)v + 116);
        cnt = game_u32((uintptr_t)v + 120) & 0x3FFF;
        k += sprintf_s(r, n, "entity %08X: %u components\n", (unsigned)v, cnt);
        for (i = 0; i < cnt && i < 64 && k + 120 < n; i++) {
            uintptr_t c = game_u32(arr + 4 * i);
            k += sprintf_s(r + k, n - k, "%2u %08X %s\n", i, (unsigned)c, game_rtti(c, cls, sizeof(cls)));
        }
    }
    else if (!strcmp(a, "modules")) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        MODULEENTRY32 me;
        size_t k = 0;
        me.dwSize = sizeof(me);
        if (snap != INVALID_HANDLE_VALUE && Module32First(snap, &me)) {
            do k += sprintf_s(r + k, n - k, "%08X %08lX %s\n", (unsigned)(uintptr_t)me.modBaseAddr, me.modBaseSize, me.szModule);
            while (Module32Next(snap, &me) && k + 100 < n);
            CloseHandle(snap);
        }
    }
    else if (!strcmp(a, "stacks")) {
        // every other thread: EIP and the game-code return addresses found on its stack (for hangs)
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te;
        size_t k = 0;
        te.dwSize = sizeof(te);
        if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
            do {
                HANDLE t;
                CONTEXT c;
                if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
                if (!(t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID))) continue;
                SuspendThread(t);
                c.ContextFlags = CONTEXT_FULL;
                if (GetThreadContext(t, &c)) {
                    int i, found = 0;
                    k += sprintf_s(r + k, n - k, "tid %lu eip %08lX:", te.th32ThreadID, c.Eip);
                    for (i = 0; i < 512 && found < 10 && k + 16 < n; i++) {
                        uint32_t v;
                        if (!game_read(c.Esp + 4 * i, &v, 4)) break;
                        if (v > 0x401000 && v < 0x15A0000) { k += sprintf_s(r + k, n - k, " %08X", v); found++; }
                    }
                    k += sprintf_s(r + k, n - k, "\n");
                }
                ResumeThread(t);
                CloseHandle(t);
            } while (Thread32Next(snap, &te) && k + 200 < n);
            CloseHandle(snap);
        }
    }
    else if (!strcmp(a, "overlay")) { g_lab.overlay_on = !g_lab.overlay_on; sprintf_s(r, n, "overlay %d", g_lab.overlay_on); }
    else sprintf_s(r, n, "unknown command: %s", a);
}

// Bridge entry: read-only commands run at once, the rest on the game thread (2 s timeout).
void control_queue_wait(const char *cmd, char *reply, size_t n)
{
    int i, slot = -1;
    if (read_only(cmd)) { control_command(cmd, reply, n); return; }
    if (!g_qinit) { strcpy_s(reply, n, "not ready"); return; }
    EnterCriticalSection(&g_qcs);
    for (i = 0; i < MAX_QUEUE; i++)
        if (!g_jobs[i].used) { slot = i; break; }
    if (slot >= 0) {
        g_jobs[slot].used = 1;
        strcpy_s(g_jobs[slot].cmd, sizeof(g_jobs[slot].cmd), cmd);
        g_jobs[slot].reply[0] = 0;
        ResetEvent(g_jobs[slot].done);
    }
    LeaveCriticalSection(&g_qcs);
    if (slot < 0) { strcpy_s(reply, n, "queue full"); return; }
    if (WaitForSingleObject(g_jobs[slot].done, 2000) != WAIT_OBJECT_0) {
        strcpy_s(reply, n, "timeout (is the game running a frame?)");
        g_jobs[slot].used = 2;   // abandoned: the game thread runs it later and frees the slot
        return;
    }
    strcpy_s(reply, n, g_jobs[slot].reply);
    EnterCriticalSection(&g_qcs);
    g_jobs[slot].used = 0;
    LeaveCriticalSection(&g_qcs);
}

void control_queue(const char *cmd)
{
    char r[256];
    control_queue_wait(cmd, r, sizeof(r));
}

static int pressed(int vk)
{
    int down = (GetAsyncKeyState(vk) & 0x8000) != 0, was = g_keys[vk];
    g_keys[vk] = (uint8_t)down;
    return down && !was;
}

static int foreground(void)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static void hotkeys(void)
{
    char r[512];
    int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0, ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    static const float speeds[] = {1.0f, 0.5f, 0.25f, 0.1f};
    static int speed_i;
    if (pressed(VK_F1)) g_lab.overlay_on = !g_lab.overlay_on;
    if (pressed(VK_F2)) checks_select(shift ? -1 : 1);
    if (pressed(VK_F3)) {
        if (trace_active()) trace_stop();
        else {
            const LabCheck *c = checks_current();
            char label[64];
            sprintf_s(label, sizeof(label), "check%s", c ? c->id : "");
            trace_start(label);
        }
    }
    if (pressed(VK_F4)) trace_mark(shift ? "odd" : "");
    if (pressed(VK_F5)) bookmark_save(ctrl ? 1 : 0);
    if (pressed(VK_F6)) { bookmark_load(ctrl ? 1 : 0, r, sizeof(r)); overlay_note("%s", r); }
    if (pressed(VK_F7)) { speed_i = (speed_i + (shift ? 3 : 1)) % 4; control_set_scale(speeds[speed_i]); }
    if (pressed(VK_F8)) control_pause(!g_paused);
    if (pressed(VK_F10)) control_step(1);
    if (pressed(VK_F11)) {
        const LabCheck *c = checks_current();
        if (input_playing()) input_stop();
        else if (c && c->auto_script[0]) {
            if (!trace_active()) {
                char label[64];
                sprintf_s(label, sizeof(label), "check%s_auto", c->id);
                trace_start(label);
            }
            input_play_file(c->auto_script);
        } else overlay_note("this check has no script");
    }
    if (pressed(VK_F12)) { input_record(!input_recording()); overlay_note(input_recording() ? "pad recording ON" : "pad recording off"); }
}

void control_frame(void)
{
    int i;
    if (!g_qinit) {
        InitializeCriticalSection(&g_qcs);
        for (i = 0; i < MAX_QUEUE; i++) g_jobs[i].done = CreateEventA(NULL, TRUE, FALSE, NULL);
        g_qinit = 1;
        g_user_scale = control_get_scale();
        if (g_user_scale <= 0.0f) g_user_scale = 1.0f;
        g_scale = g_user_scale;
    }
    game_refresh();
    if (g_lab.hotkeys && foreground()) hotkeys();
    // queued commands
    for (i = 0; i < MAX_QUEUE; i++) {
        if (g_jobs[i].used && WaitForSingleObject(g_jobs[i].done, 0) != WAIT_OBJECT_0) {
            control_command(g_jobs[i].cmd, g_jobs[i].reply, sizeof(g_jobs[i].reply));
            SetEvent(g_jobs[i].done);
        }
        if (g_jobs[i].used == 2 && WaitForSingleObject(g_jobs[i].done, 0) == WAIT_OBJECT_0) g_jobs[i].used = 0;
    }
    apply_scale();                          // a pending step runs this frame at the normal speed
    if (g_paused && g_step > 0) g_step--;
}
