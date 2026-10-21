// Probes: log a code address each time it runs, with registers and memory, without stopping the game.
// An armed probe is an int3 byte; the vectored handler logs the hit, puts the original byte back, single-steps the
// instruction (trap flag) and re-arms. No limit of four like hardware breakpoints, and nothing waits on a debugger.
//
// Probe line:  <address> <name> [when=<expr>] [max=<n>] <field> <field> ...
//   e.g.  0xD93C60 LedgeStop_Enter when=[ecx+8]==$human  feet=[ecx+8]+0x40:vec3
#include "lab.h"
#include <stdlib.h>
#include <string.h>

static LabProbe g_probes[LAB_MAX_PROBES];
static volatile LONG g_nprobes;
static DWORD g_tls = TLS_OUT_OF_INDEXES;   // probe index + 1 waiting to be re-armed on this thread
static CRITICAL_SECTION g_cs;

static int write_byte(uintptr_t addr, uint8_t b)
{
    __try {
        *(volatile uint8_t *)addr = b;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    FlushInstructionCache(GetCurrentProcess(), (void *)addr, 1);
    return 1;
}

static LabProbe *find(uintptr_t addr)
{
    LONG i, n = g_nprobes;
    for (i = 0; i < n; i++)
        if (g_probes[i].addr == addr && g_probes[i].armed) return &g_probes[i];
    return NULL;
}

static void hit(LabProbe *p, CONTEXT *c)
{
    LabEnv env;
    uint64_t ok = 1;
    char text[1200];
    memset(&env, 0, sizeof(env));
    env.eax = c->Eax, env.ebx = c->Ebx, env.ecx = c->Ecx, env.edx = c->Edx;
    env.esi = c->Esi, env.edi = c->Edi, env.ebp = c->Ebp, env.esp = c->Esp, env.eip = p->addr;
    env.have_regs = 1;
    InterlockedIncrement(&p->hits);
    if (p->when[0] && (!expr_eval(p->when, &env, &ok) || !ok)) return;
    if (p->max_hits && p->logged >= p->max_hits) return;
    InterlockedIncrement(&p->logged);
    text[0] = 0;
    if (p->spec[0]) expr_format(p->spec, &env, text, sizeof(text));
    trace_event("probe %s @%08X #%ld %s", p->name, (unsigned)p->addr, p->logged, text);
}

static LONG CALLBACK handler(EXCEPTION_POINTERS *x)
{
    DWORD code = x->ExceptionRecord->ExceptionCode;
    CONTEXT *c = x->ContextRecord;
    if (code == EXCEPTION_BREAKPOINT) {
        uintptr_t at = (uintptr_t)x->ExceptionRecord->ExceptionAddress;
        LabProbe *p = find(at);
        if (!p) return EXCEPTION_CONTINUE_SEARCH;
        hit(p, c);
        write_byte(at, p->orig);
        c->Eip = (DWORD)at;
        c->EFlags |= 0x100;  // trap after the original instruction
        TlsSetValue(g_tls, (void *)(uintptr_t)(p - g_probes + 1));
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_SINGLE_STEP) {
        uintptr_t i = (uintptr_t)TlsGetValue(g_tls);
        if (!i) return EXCEPTION_CONTINUE_SEARCH;
        TlsSetValue(g_tls, NULL);
        {
            LabProbe *p = &g_probes[i - 1];
            if (p->armed && !(p->max_hits && p->logged >= p->max_hits)) write_byte(p->addr, 0xCC);
            else p->armed = 0;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// The lab's own CRT (the game's is separate): a too-long string to a *_s function leaves the target empty instead of
// ending the game (a 600-byte probe line once did).
static void ignore_invalid_parameter(const wchar_t *e, const wchar_t *f, const wchar_t *file, unsigned line, uintptr_t r)
{
    (void)e, (void)f, (void)file, (void)line, (void)r;
}

void probes_init(void)
{
    _set_invalid_parameter_handler(ignore_invalid_parameter);
    InitializeCriticalSection(&g_cs);
    g_tls = TlsAlloc();
    AddVectoredExceptionHandler(1, handler);
    lab_log("probes ready (VEH)");
}

static int arm(LabProbe *p)
{
    DWORD prot;
    if (p->armed) return 1;
    if (!game_read(p->addr, &p->orig, 1)) return 0;
    if (p->orig == 0xCC) return 0;  // already someone's breakpoint
    if (!VirtualProtect((void *)p->addr, 1, PAGE_EXECUTE_READWRITE, &prot)) return 0;
    p->armed = 1;
    return write_byte(p->addr, 0xCC);
}

static void disarm(LabProbe *p)
{
    if (!p->armed) return;
    p->armed = 0;
    write_byte(p->addr, p->orig);
}

// Adds the probes in `text` (one per line) and arms them.
int probes_load_text(const char *text, const char *source)
{
    char buf[4096], *ctx = NULL, *line;
    int added = 0;
    strcpy_s(buf, sizeof(buf), text);
    EnterCriticalSection(&g_cs);
    for (line = strtok_s(buf, "\n", &ctx); line; line = strtok_s(NULL, "\n", &ctx)) {
        char *tok, *c2 = NULL;
        LabProbe *p;
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#' || *line == ';') continue;
        if (g_nprobes >= LAB_MAX_PROBES) break;
        p = &g_probes[g_nprobes];
        memset(p, 0, sizeof(*p));
        tok = strtok_s(line, " \t\r", &c2);
        p->addr = strtoul(tok, NULL, 16);
        tok = strtok_s(NULL, " \t\r", &c2);
        strcpy_s(p->name, sizeof(p->name), tok ? tok : "probe");
        while ((tok = strtok_s(NULL, " \t\r", &c2))) {
            if (!strncmp(tok, "when=", 5)) strcpy_s(p->when, sizeof(p->when), tok + 5);
            else if (!strncmp(tok, "max=", 4)) p->max_hits = atoi(tok + 4);
            else if (strlen(p->spec) + strlen(tok) + 2 > sizeof(p->spec)) {
                lab_log("probe %s: field list longer than %u bytes, %s and later fields dropped", p->name,
                        (unsigned)sizeof(p->spec), tok);
                break;
            } else {
                if (p->spec[0]) strcat_s(p->spec, sizeof(p->spec), " ");
                strcat_s(p->spec, sizeof(p->spec), tok);
            }
        }
        if (p->addr < 0x401000 || p->addr > 0x1600000) {
            lab_log("probe %s: address %08X outside .text, skipped", p->name, (unsigned)p->addr);
            continue;
        }
        InterlockedIncrement(&g_nprobes);
        if (arm(p)) added++;
        lab_log("probe %s @%08X %s (%s) when=%s max=%d: %s", p->name, (unsigned)p->addr, p->armed ? "armed" : "FAILED",
                source, p->when, p->max_hits, p->spec);
    }
    LeaveCriticalSection(&g_cs);
    return added;
}

void probes_clear(void)
{
    LONG i;
    EnterCriticalSection(&g_cs);
    for (i = 0; i < g_nprobes; i++) disarm(&g_probes[i]);
    // a hit in flight on another thread may still read its slot; the slots stay valid memory
    g_nprobes = 0;
    LeaveCriticalSection(&g_cs);
}

void probes_arm_all(void)
{
    LONG i;
    for (i = 0; i < g_nprobes; i++) arm(&g_probes[i]);
}

int probes_count(void) { return g_nprobes; }

void probes_status(char *out, size_t n)
{
    LONG i;
    size_t k = 0;
    out[0] = 0;
    for (i = 0; i < g_nprobes && k + 64 < n; i++)
        k += sprintf_s(out + k, n - k, "%s %ld/%ld%s\n", g_probes[i].name, g_probes[i].logged, g_probes[i].hits,
                       g_probes[i].armed ? "" : " (off)");
}
