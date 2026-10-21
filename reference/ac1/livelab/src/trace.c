// Recordings: frames.csv (one row per player update) and events.log (probe hits, marks, notes) per recording, in
// sessions/<date-time>/<nnn>_<label>/. Probe hits and marks also always go to the session's events.log.
#include "lab.h"
#include <stdarg.h>
#include <share.h>
#include <string.h>

static CRITICAL_SECTION g_cs;
static int g_init;
static FILE *g_frames, *g_events, *g_session_events;
static int g_count;
static char g_dir[MAX_PATH];

static void init_once(void)
{
    if (!g_init) {
        InitializeCriticalSection(&g_cs);
        g_init = 1;
    }
}

int trace_active(void) { return g_frames != NULL; }

void trace_start(const char *label)
{
    char path[MAX_PATH], safe[64];
    int i;
    const LabCheck *c = checks_current();
    init_once();
    trace_stop();
    for (i = 0; label && label[i] && i < 60; i++)
        safe[i] = (label[i] == ' ' || label[i] == '\\' || label[i] == '/' || label[i] == ':') ? '_' : label[i];
    safe[i] = 0;
    EnterCriticalSection(&g_cs);
    sprintf_s(g_dir, MAX_PATH, "%s\\%03d_%s", g_lab.session, ++g_count, safe[0] ? safe : "rec");
    CreateDirectoryA(g_dir, NULL);
    sprintf_s(path, MAX_PATH, "%s\\frames.csv", g_dir);
    g_frames = _fsopen(path, "w", _SH_DENYNO);
    sprintf_s(path, MAX_PATH, "%s\\events.log", g_dir);
    g_events = _fsopen(path, "w", _SH_DENYNO);
    if (g_frames) {
        char names[LAB_MAX_FIELDS][48], exprs[LAB_MAX_FIELDS][160];
        int n = 0;
        fprintf(g_frames, "frame,time,dt,scale,x,y,z,fx,fy,fz,vx,vy,vz,ctx,ctx_name,prev_ctx,action,action_name,"
                          "item_word,buttons,lx,ly,rx,ry,injected,mdx,mdy,mdz,mmag,action1,action1_name,"
                          "head_x,head_y,head_z,spine_x,spine_y,spine_z,lh_x,lh_y,lh_z,rh_x,rh_y,rh_z,lf_x,lf_y,lf_z,rf_x,rf_y,rf_z");
        checks_fields(names, exprs, &n);
        for (i = 0; i < n; i++) fprintf(g_frames, ",%s", names[i]);
        fputc('\n', g_frames);
    }
    if (g_events) {
        fprintf(g_events, "# recording %s, check %s %s\n", g_dir, c ? c->id : "-", c ? c->title : "");
        fflush(g_events);
    }
    LeaveCriticalSection(&g_cs);
    lab_log("recording -> %s", g_dir);
    overlay_note("REC %s", g_dir + strlen(g_lab.session) + 1);
}

void trace_stop(void)
{
    init_once();
    EnterCriticalSection(&g_cs);
    if (g_frames) fclose(g_frames);
    if (g_events) fclose(g_events);
    if (g_frames) {
        lab_log("recording stopped: %s", g_dir);
        overlay_note("recording saved");
    }
    g_frames = g_events = NULL;
    LeaveCriticalSection(&g_cs);
}

void trace_frame(const LabFrame *f)
{
    int i;
    unsigned b = 0;
    if (!g_frames) return;
    for (i = 0; i < 16; i++) b |= (f->buttons[i] ? 1u : 0u) << i;
    EnterCriticalSection(&g_cs);
    if (g_frames) {
        fprintf(g_frames, "%u,%.5f,%.6f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%s,%d,%08X,%s,%04X,%04X,"
                          "%.3f,%.3f,%.3f,%.3f,%d,%.4f,%.4f,%.4f,%.4f",
                f->frame, f->time, f->dt, f->scale, f->pos[0], f->pos[1], f->pos[2], f->fwd[0], f->fwd[1], f->fwd[2],
                f->vel[0], f->vel[1], f->vel[2], f->ctx, game_ctx_name(f->ctx), f->prev_ctx, f->action,
                game_action_name(f->action), f->item_word, b, f->lstick[0], f->lstick[1], f->rstick[0], f->rstick[1],
                f->injecting, f->move_dir[0], f->move_dir[1], f->move_dir[2], f->move_mag);
        fprintf(g_frames, ",%08X,%s", f->action1, game_action_name(f->action1));
        for (i = 0; i < 6; i++) fprintf(g_frames, ",%.4f,%.4f,%.4f", f->bones[i][0], f->bones[i][1], f->bones[i][2]);
        for (i = 0; i < f->nfields; i++) fprintf(g_frames, ",%s", f->fields[i]);
        fputc('\n', g_frames);
    }
    LeaveCriticalSection(&g_cs);
}

void trace_event(const char *fmt, ...)
{
    va_list ap;
    char line[1600];
    int n;
    init_once();
    n = sprintf_s(line, sizeof(line), "%u %.4f ", g_lab.frames, lab_now());
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n - 2, fmt, ap);
    va_end(ap);
    EnterCriticalSection(&g_cs);
    if (!g_session_events) {
        char path[MAX_PATH];
        lab_session_path(path, MAX_PATH, "events.log");
        g_session_events = _fsopen(path, "a", _SH_DENYNO);
    }
    if (g_session_events) {
        fputs(line, g_session_events);
        fputc('\n', g_session_events);
        fflush(g_session_events);
    }
    if (g_events) {
        fputs(line, g_events);
        fputc('\n', g_events);
        fflush(g_events);
    }
    LeaveCriticalSection(&g_cs);
    overlay_note("%s", line);
}

void trace_mark(const char *text)
{
    const LabCheck *c = checks_current();
    trace_event("MARK check=%s %s", c ? c->id : "-", text ? text : "");
}
