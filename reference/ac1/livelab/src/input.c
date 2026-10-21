// Pad recorder / player, run right after PadProxyPC__Update 0x909C90 (the proxy the game reads).
// Proxy layout (PadProxyPC__Update 0x909C90 and Pad::UpdateTimeStamps 0x93F990, EaglePatch's scimitar::Pad):
//   +8  bool last[16]   +24 bool this[16]   +40 u64 last stamp   +48 u64 this stamp   +56 u64 press stamp[16]
//   +184 float value[16]   +256 left stick x, y   +272 right stick x, y
// Buttons: 0 A (legs) 1 X 2 Y 3 B (empty hand) 4 down 5 left 6 up 7 right 8 back 9 start 10 LT 11 LB 12 RT (high
// profile) 13 RB 14 L3 15 R3 (RE/18 2).
//
// Scripts (scripts/*.txt), one command per line, run in game time:
//   wait <s>                    stick <x> <y> [for <s>]      rstick <x> <y> [for <s>]
//   hold <btn>  release <btn|all>   tap <btn> [<s>]   mark <text>   rec start <label> | rec stop
//   pause | scale <f>           (x right, y forward; 1 = full tilt)
//   goto <x> <y> [mag] [radius] [timeout]   walk / run to a game-space point (blocks until there or timed out)
//   dir <dx> <dy> [mag] [for <s>]           hold a game-space (world) direction
// The world-space commands steer through the camera: the interpreter's world move direction (+4688) for the stick
// sent the frame before gives the camera's yaw (stick space and world are a rotation apart).
// A pad recording ("# ac1lab pad v1", F12 / "inputrec on") replays frame by frame.
#include "lab.h"
#include <math.h>
#include <share.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t b[16]; float l[2], r[2]; } PadState;

static const char *BTN[16] = {"A", "X", "Y", "B", "DOWN", "LEFT", "UP", "RIGHT", "BACK", "START", "LT", "LB", "RT", "RB", "L3", "R3"};

static char **g_lines;
static int g_nlines, g_pc, g_playing, g_raw;
static double g_wait, g_tap_left[16];
static PadState g_state;
static FILE *g_rec;
// world steering: 0 none, 1 goto (blocking), 2 hold a world direction
static int g_steer;
static float g_target[2], g_wdir[2], g_smag, g_radius;
static double g_steer_left;
static float g_cam_yaw;          // stick angle + cam yaw = world angle
static int g_cam_known, g_learn_tries;
static float g_sent[2];          // the stick sent last frame
static uint32_t g_rec_frame;
static CRITICAL_SECTION g_cs;
static int g_init;

static void init_once(void)
{
    if (!g_init) { InitializeCriticalSection(&g_cs); g_init = 1; }
}

static int btn_index(const char *s)
{
    int i;
    for (i = 0; i < 16; i++) if (!_stricmp(s, BTN[i])) return i;
    if (!_stricmp(s, "LEGS")) return 0;
    if (!_stricmp(s, "HAND")) return 3;
    if (!_stricmp(s, "HIGH")) return 12;
    return atoi(s) >= 0 && atoi(s) < 16 && s[0] >= '0' && s[0] <= '9' ? atoi(s) : -1;
}

static void free_script(void)
{
    int i;
    for (i = 0; i < g_nlines; i++) free(g_lines[i]);
    free(g_lines);
    g_lines = NULL;
    g_nlines = g_pc = 0;
}

int input_play_file(const char *path)
{
    FILE *f;
    char line[512];
    int cap = 0;
    init_once();
    if (fopen_s(&f, path, "r")) { lab_log("input: cannot open %s", path); return 0; }
    EnterCriticalSection(&g_cs);
    free_script();
    g_raw = 0;
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = 0;
        if (!strncmp(line, "# ac1lab pad v1", 15)) g_raw = 1;
        if (!line[0] || line[0] == '#') continue;
        if (g_nlines == cap) { cap = cap ? cap * 2 : 256; g_lines = (char **)realloc(g_lines, cap * sizeof(char *)); }
        g_lines[g_nlines++] = _strdup(line);
    }
    fclose(f);
    memset(&g_state, 0, sizeof(g_state));
    memset(g_tap_left, 0, sizeof(g_tap_left));
    g_wait = 0;
    g_steer = 0;
    g_playing = 1;
    LeaveCriticalSection(&g_cs);
    lab_log("input: playing %s (%d lines%s)", path, g_nlines, g_raw ? ", pad recording" : "");
    trace_event("INPUT play %s", path);
    overlay_note("playing %s", strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path);
    return 1;
}

void input_stop(void)
{
    init_once();
    EnterCriticalSection(&g_cs);
    if (g_playing) trace_event("INPUT stop");
    g_playing = 0;
    free_script();
    LeaveCriticalSection(&g_cs);
}

int input_playing(void) { return g_playing; }
int input_recording(void) { return g_rec != NULL; }

void input_record(int on)
{
    init_once();
    EnterCriticalSection(&g_cs);
    if (on && !g_rec) {
        char path[MAX_PATH], name[64];
        static int n;
        sprintf_s(name, sizeof(name), "pad_%03d.txt", ++n);
        lab_session_path(path, MAX_PATH, name);
        g_rec = _fsopen(path, "w", _SH_DENYNO);
        if (g_rec) {
            LabFrame f = g_lab.last;
            fprintf(g_rec, "# ac1lab pad v1: dt buttons(hex) lx ly rx ry\n# start pos %.4f %.4f %.4f fwd %.4f %.4f %.4f\n",
                    f.pos[0], f.pos[1], f.pos[2], f.fwd[0], f.fwd[1], f.fwd[2]);
            trace_event("INPUT recording %s", path);
        }
        g_rec_frame = 0;
    } else if (!on && g_rec) {
        fclose(g_rec);
        g_rec = NULL;
        trace_event("INPUT recording stopped");
    }
    LeaveCriticalSection(&g_cs);
}

// Runs script lines until one waits. dt = the game's last frame time.
static void step_script(double dt)
{
    int i;
    for (i = 0; i < 16; i++)
        if (g_tap_left[i] > 0 && (g_tap_left[i] -= dt) <= 0) g_state.b[i] = 0;
    if (g_raw) {
        // one recorded frame per pad update
        unsigned b;
        float l0, l1, r0, r1, d;
        if (g_pc >= g_nlines) { g_playing = 0; trace_event("INPUT done"); overlay_note("input done"); return; }
        if (sscanf_s(g_lines[g_pc++], "%f %x %f %f %f %f", &d, &b, &l0, &l1, &r0, &r1) == 6) {
            for (i = 0; i < 16; i++) g_state.b[i] = (b >> i) & 1;
            g_state.l[0] = l0, g_state.l[1] = l1, g_state.r[0] = r0, g_state.r[1] = r1;
        }
        return;
    }
    if (g_steer == 1) {
        float dx = g_target[0] - g_lab.last.pos[0], dy = g_target[1] - g_lab.last.pos[1];
        g_steer_left -= dt;
        if (dx * dx + dy * dy < g_radius * g_radius || g_steer_left <= 0) {
            trace_event("GOTO %s at (%.2f %.2f %.2f)", g_steer_left <= 0 ? "timed out" : "arrived", g_lab.last.pos[0],
                        g_lab.last.pos[1], g_lab.last.pos[2]);
            g_steer = 0;
            g_state.l[0] = g_state.l[1] = 0;
        } else return;
    }
    if (g_wait > 0) { g_wait -= dt; if (g_wait > 0) return; }
    while (g_pc < g_nlines) {
        char line[512], *ctx = NULL, *cmd, *a, *b2;
        strcpy_s(line, sizeof(line), g_lines[g_pc++]);
        cmd = strtok_s(line, " \t", &ctx);
        if (!cmd) continue;
        a = strtok_s(NULL, " \t", &ctx);
        if (!_stricmp(cmd, "wait")) { g_wait += a ? atof(a) : 0; if (g_wait > 0) return; }
        else if (!_stricmp(cmd, "goto")) {
            char *t;
            g_target[0] = a ? (float)atof(a) : 0;
            g_target[1] = (t = strtok_s(NULL, " \t", &ctx)) ? (float)atof(t) : 0;
            g_smag = (t = strtok_s(NULL, " \t", &ctx)) ? (float)atof(t) : 1.0f;
            g_radius = (t = strtok_s(NULL, " \t", &ctx)) ? (float)atof(t) : 0.4f;
            g_steer_left = (t = strtok_s(NULL, " \t", &ctx)) ? atof(t) : 20.0;
            g_steer = 1;
            g_cam_known = 0, g_learn_tries = 0;
            trace_event("GOTO %.2f %.2f mag %.2f", g_target[0], g_target[1], g_smag);
            return;
        }
        else if (!_stricmp(cmd, "dir")) {
            char *t;
            float dx = a ? (float)atof(a) : 0, dy = (t = strtok_s(NULL, " \t", &ctx)) ? (float)atof(t) : 0;
            float l = sqrtf(dx * dx + dy * dy);
            g_wdir[0] = l > 0 ? dx / l : 0, g_wdir[1] = l > 0 ? dy / l : 0;
            g_smag = (t = strtok_s(NULL, " \t", &ctx)) && _stricmp(t, "for") ? (float)atof(t) : 1.0f;
            if (t && !_stricmp(t, "for")) ; else t = strtok_s(NULL, " \t", &ctx);
            if (l > 0 && g_steer != 2) g_cam_known = 0, g_learn_tries = 0;
            g_steer = l > 0 ? 2 : 0;
            if (!g_steer) g_state.l[0] = g_state.l[1] = 0;
            if (t && !_stricmp(t, "for")) {
                char *u = strtok_s(NULL, " \t", &ctx);
                g_wait += u ? atof(u) : 0;
                if (g_wait > 0) return;
            }
        }
        else if (!_stricmp(cmd, "stick") || !_stricmp(cmd, "rstick")) {
            float *s = !_stricmp(cmd, "stick") ? g_state.l : g_state.r;
            if (s == g_state.l) g_steer = 0;
            char *f;
            b2 = strtok_s(NULL, " \t", &ctx);
            s[0] = a ? (float)atof(a) : 0, s[1] = b2 ? (float)atof(b2) : 0;
            if ((f = strtok_s(NULL, " \t", &ctx)) && !_stricmp(f, "for")) {
                char *t = strtok_s(NULL, " \t", &ctx);
                g_wait += t ? atof(t) : 0;
                if (g_wait > 0) return;
            }
        }
        else if (!_stricmp(cmd, "hold")) { int k = a ? btn_index(a) : -1; if (k >= 0) g_state.b[k] = 1, g_tap_left[k] = 0; }
        else if (!_stricmp(cmd, "release")) {
            if (a && !_stricmp(a, "all")) { memset(g_state.b, 0, 16); memset(g_tap_left, 0, sizeof(g_tap_left)); }
            else { int k = a ? btn_index(a) : -1; if (k >= 0) g_state.b[k] = 0; }
        }
        else if (!_stricmp(cmd, "tap")) {
            int k = a ? btn_index(a) : -1;
            char *t = strtok_s(NULL, " \t", &ctx);
            if (k >= 0) g_state.b[k] = 1, g_tap_left[k] = t ? atof(t) : 0.1;
        }
        else {
            // anything else is a lab command (mark, rec start, scale, pause, save/load, check ...)
            char reply[512], full[512];
            sprintf_s(full, sizeof(full), "%s%s%s%s%s", cmd, a ? " " : "", a ? a : "", ctx && *ctx ? " " : "",
                      ctx && *ctx ? ctx : "");
            control_command(full, reply, sizeof(reply));
            lab_log("script: %s -> %s", full, reply);
        }
    }
    if (g_pc >= g_nlines && g_wait <= 0) {
        int any = 0;
        for (i = 0; i < 16; i++) any |= g_tap_left[i] > 0;
        if (!any) { g_playing = 0; memset(&g_state, 0, sizeof(g_state)); trace_event("INPUT done"); overlay_note("script done"); }
    }
}

// World steering: learn the camera yaw from last frame's stick and the interpreter's world move direction, then
// turn the wanted world direction into this frame's stick.
static void steer(void)
{
    float sl = sqrtf(g_sent[0] * g_sent[0] + g_sent[1] * g_sent[1]);
    float *m = g_lab.last.move_dir, ml = sqrtf(m[0] * m[0] + m[1] * m[1]);
    float want[2], a;
    if (sl > 0.15f && ml > 0.5f && g_lab.interp) {
        g_cam_yaw = atan2f(m[1], m[0]) - atan2f(g_sent[1], g_sent[0]);
        if (!g_cam_known) lab_log("steer: camera learnt with stick %.2f", sl);
        g_cam_known = 1;
    }
    if (g_steer == 1) {
        float dx = g_target[0] - g_lab.last.pos[0], dy = g_target[1] - g_lab.last.pos[1], l = sqrtf(dx * dx + dy * dy);
        want[0] = l > 0 ? dx / l : 0, want[1] = l > 0 ? dy / l : 0;
    } else if (g_steer == 2) {
        want[0] = g_wdir[0], want[1] = g_wdir[1];
    } else {
        g_sent[0] = g_state.l[0], g_sent[1] = g_state.l[1];
        return;
    }
    if (!g_cam_known) {
        // learn the camera first with a push inside the dead zone (0.35): the interpreter still reports its world
        // direction (ReadInput 0xEEDE60 copies +1024 to +4688 for any non-zero stick) but nothing moves
        g_state.l[0] = 0, g_state.l[1] = ++g_learn_tries > 6 ? g_smag : 0.25f;   // no answer inside the dead zone: push
    } else {
        a = atan2f(want[1], want[0]) - g_cam_yaw;
        g_state.l[0] = g_smag * cosf(a), g_state.l[1] = g_smag * sinf(a);
    }
    g_sent[0] = g_state.l[0], g_sent[1] = g_state.l[1];
}

// Writes the scripted state into one scimitar::Pad: buttons, analog values, sticks and the press stamps of new
// presses (as Pad::UpdateTimeStamps 0x93F990 does).
static void inject(uintptr_t pad)
{
    uint8_t prev[16];
    uint64_t stamp;
    float v[16];
    int i;
    game_read(pad + 8, prev, 16);
    game_read(pad + 48, &stamp, 8);
    game_write(pad + 24, g_state.b, 16);
    for (i = 0; i < 16; i++) {
        v[i] = g_state.b[i] ? 1.0f : 0.0f;
        if (g_state.b[i] && !prev[i]) game_write(pad + 56 + 8 * i, &stamp, 8);
    }
    game_write(pad + 184, v, sizeof(v));
    game_write(pad + 256, g_state.l, 8);
    game_write(pad + 272, g_state.r, 8);
}

void input_pad_update(uintptr_t pad)
{
    static double last;
    double now = lab_now(), dt;
    int i;
    init_once();
    dt = game_f32(G_FRAME_DT);
    if (dt <= 0 || dt > 0.5) dt = now - last;
    last = now;
    EnterCriticalSection(&g_cs);
    if (g_playing) {
        // the proxy, and the active source pad it copied from (PadProxyPC__Update: selected index +1284, pads at
        // +1292 with a 536-byte stride; the menus read the source pad)
        uint32_t sel = game_u32(pad + 1284);
        uintptr_t src = game_u32(pad + 1292 + 536 * (sel < 4 ? sel : 0));
        step_script(dt);
        steer();
        inject(pad);
        if (src && src != pad) inject(src);
    }
    if (g_rec) {
        uint8_t b[16];
        float l[2], r[2];
        unsigned bits = 0;
        game_read(pad + 24, b, 16);
        game_read(pad + 256, l, 8);
        game_read(pad + 272, r, 8);
        for (i = 0; i < 16; i++) bits |= (b[i] ? 1u : 0u) << i;
        fprintf(g_rec, "%.6f %04X %.4f %.4f %.4f %.4f\n", dt, bits, l[0], l[1], r[0], r[1]);
        g_rec_frame++;
    }
    LeaveCriticalSection(&g_cs);
    // the sampled frame shows the pad as the game sees it
    game_read(pad + 24, g_lab.pad_b, 16);
    game_read(pad + 256, g_lab.pad_l, 8);
    game_read(pad + 272, g_lab.pad_r, 8);
    g_lab.pad_inj = g_playing;
}
