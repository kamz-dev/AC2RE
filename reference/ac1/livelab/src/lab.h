// AC1 Lab: live-analysis mod for Assassin's Creed (Steam v1.02 b86610, AssassinsCreed_Dx9.exe).
// Loaded into the game by ac1lab.exe; nothing on disk in the game folder is changed.
// Every game address below comes from the IDA database (bin/AssassinsCreed_Dx9.exe.i64) and is cited in
// livelab/README.md.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>

// ---- game addresses (v1.02, image base 0x400000, not relocated) ----
#define G_WORLD            0x1A1198C   // Game__GetWorld 0x419880: *(this)+0x48 = the current scimitar::World
#define G_ENGINE           0x1A1197C   // Engine::BeginFrame task object (sub_417470): clock at +4.. (sub_416420)
#define G_FRAME_DT         0x192DDA8   // g_FrameDt, written by the frame-time setter 0x93E0F0
#define VT_HUMAN           0x16D9314   // Human vftable (RE/01 2.2)
#define VT_HUMAN_POSTUPDATE (VT_HUMAN + 18 * 4)  // slot 18 = Human__PostUpdate 0xB26FD0
#define VT_PADPROXY_UPDATE 0x16B1420   // PadProxyPC vftable slot -> PadProxyPC__Update 0x909C90
#define VT_INTERP_UPDATE   (0x16F52AC + 18 * 4)  // GoAssassinActionInterpreter vftable slot 18 = __Update 0xEEE700
#define VT_CONTROLLER      0x168C484   // CharacterController vftable (RE/01 7)
#define FN_ENTITY_COMPONENT_BY_SLOT 0x7183F0 // sub_7183F0: component handle -> component (anim component)
#define FN_SLOT_SELECTED_ACTION 0x5015C0     // sub_5015C0(comp, slot): the slot's selected Action (+8 = id)
#define FN_FIND_SLOT_INSTANCE 0x5014C0       // AnimComponent__FindSlotInstance(comp, slot) -> ActionPlayer
#define FN_BONE_WORLD      0x4EC0E0          // Bone__GetWorldPosition(handle, out vec4): rebuilds a dirty pose first
#define FN_PLAYING_ITEM    0x723E00          // ActionPlayer__GetPlayingItem(player) -> ActionItem (+60 word)

#define LAB_MAX_FIELDS 48
#define LAB_MAX_PROBES 64
#define LAB_MAX_CHECKS 128
#define LAB_TEXT 8192

// ---- one sampled frame of the player ----
typedef struct {
    uint32_t frame;
    double time;            // seconds since the lab started (QPC)
    float dt;               // g_FrameDt
    float scale;            // engine clock scale (+12)
    uintptr_t entity, human, controller;
    float pos[3];           // entity matrix translation (Z up)
    float fwd[3], right[3]; // entity matrix rows
    float vel[3];           // controller +0x90
    int ctx, prev_ctx;      // locomotion slot: current / previous ActorContextID
    uint32_t action, item_word; // main slot's selected action id, playing item's word (+60)
    uint32_t action1;           // slot 1's selected action (the upper-body channel group: fidgets, cycle breakers)
    uintptr_t item;
    uint8_t buttons[16];
    float lstick[2], rstick[2];
    float move_dir[3], move_mag;
    float bones[6][3];             // handle bones Head, Spine, LHand, RHand, LFoot, RFoot (Human +0x280 + 8i), world   // interpreter +4688 (world move direction) / +4672 (stick magnitude), ReadInput 0xEEDE60
    int injecting;          // 1 while the input player drives the pad
    int nfields;
    char fields[LAB_MAX_FIELDS][48];
} LabFrame;

typedef struct {
    char name[64];
    uintptr_t addr;
    char spec[512];          // what to log at a hit: "name=expr ..."
    char when[256];          // optional condition
    int max_hits;            // 0 = no limit
    volatile LONG hits, logged;
    uint8_t orig;
    int armed;
} LabProbe;

typedef struct {
    char id[16];             // "01" .. "67"
    char title[160];
    char steps[1536];        // instructions shown in the overlay, '\n' separated
    char probes[1024];       // probe lines ('\n' separated), armed while the check is selected
    char fields[1024];       // field lines "name=expr" sampled every frame while selected
    char auto_script[MAX_PATH]; // optional input script this check can run itself
} LabCheck;

typedef struct {
    char dir[MAX_PATH];      // livelab folder (the DLL's folder)
    char session[MAX_PATH];  // sessions/<date-time>
    FILE *log;
    CRITICAL_SECTION lock;   // guards `last` and the overlay text
    LabFrame last;
    uint32_t frames;
    LARGE_INTEGER qpf, t0;
    int overlay_on, ready;
    uint8_t pad_b[16];       // the pad as the game saw it this frame (input.c)
    float pad_l[2], pad_r[2];
    int pad_inj;
    volatile LONG pad_updates, presents;
    uintptr_t interp;          // the player's GoAssassinActionInterpreter (captured by its Update hook)
    char shot_path[MAX_PATH]; // set by the "shot" command, written at the next Present
    volatile LONG shot_pending;
    // config
    int bridge_port, background, hotkeys;
    int overlay_mode;        // debug bits: 1 state block, 2 panels, 4 text, 8 own render target, 16 own scene
} Lab;

extern Lab g_lab;

// main.c
void lab_log(const char *fmt, ...);
void *lab_patch_slot(void **slot, void *hook);
double lab_now(void);
void lab_session_path(char *out, size_t n, const char *name);

// abilities.c
void abilities_init(const char *ini);
void abilities_frame(uintptr_t interp);        // game thread: resolve the stack, apply UnlockAbilities
void abilities_command(const char *args, char *reply, size_t n);

// game.c
int game_read(uintptr_t addr, void *out, size_t n);           // SEH-protected read, 1 on success
uint32_t game_u32(uintptr_t addr);
float game_f32(uintptr_t addr);
uintptr_t game_player_entity(void);
uintptr_t game_world(void);
const char *game_rtti(uintptr_t obj, char *out, size_t n);   // class name from the object's vtable
uintptr_t game_entity_component_named(uintptr_t entity, const char *name);                 // Game__GetWorld 0x419880
int game_consider_human(uintptr_t human);   // PostUpdate hook: 1 if this Human is the player
uintptr_t game_entity_component(uintptr_t entity, uintptr_t vtable);
uintptr_t game_player_human(void);
void game_sample(LabFrame *f, uintptr_t human);              // game thread only (calls game functions)
const char *game_ctx_name(int id);
const char *game_action_name(uint32_t id);
void game_load_names(void);
void game_refresh(void);
int game_write(uintptr_t addr, const void *src, size_t n);

// expr.c
typedef struct { uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip; int have_regs; uintptr_t human; } LabEnv;
int expr_eval(const char *s, const LabEnv *env, uint64_t *value);       // integer value of an expression
int expr_format(const char *spec, const LabEnv *env, char *out, size_t n); // "name=expr:type ..." -> text
int expr_split_fields(const char *lines, char names[][48], char exprs[][160], int max);

// probes.c
void probes_init(void);
int probes_load_text(const char *text, const char *source);   // returns probes added
void probes_clear(void);
void probes_arm_all(void);
void probes_status(char *out, size_t n);
int probes_count(void);

// trace.c
void trace_frame(const LabFrame *f);
void trace_start(const char *label);
void trace_stop(void);
int trace_active(void);
void trace_mark(const char *text);
void trace_event(const char *fmt, ...);   // probe hits and notes into events.log of the current check

// checks.c
void checks_load(void);
int checks_count(void);
const LabCheck *checks_current(void);
void checks_select(int delta);
void checks_select_id(const char *id);
void checks_fields(char names[][48], char exprs[][160], int *n);
void checks_field_add(const char *line);
void checks_field_clear(void);

// control.c
void control_frame(void);       // game thread, once per pad update: hotkeys, clock, queued commands
void control_set_scale(float s);
float control_get_scale(void);
void control_pause(int on);
void control_step(int frames);
int control_paused(void);
void control_queue(const char *cmd);   // run a command on the game thread
void control_command(const char *cmd, char *reply, size_t n);
void control_queue_wait(const char *cmd, char *reply, size_t n);   // bridge: run on the game thread

// input.c
void input_pad_update(uintptr_t pad);   // after PadProxyPC__Update: record or inject
int input_play_file(const char *path);
void input_stop(void);
int input_playing(void);
void input_record(int on);
int input_recording(void);

// overlay.c
void overlay_present(IDirect3DDevice9 *dev);
void overlay_lost(void);
void overlay_reset(void);
void overlay_note(const char *fmt, ...);   // short message shown for a few seconds
void overlay_capture(IDirect3DDevice9 *dev);  // writes the pending screenshot (BMP)

// bridge.c
void bridge_start(int port);
