// AC1 Lab core: config, log, sessions, and the hooks.
//   exe IAT d3d9!Direct3DCreate9 -> IDirect3D9::CreateDevice -> IDirect3DDevice9::Present / Reset (overlay)
//   Human vftable slot 18 (Human__PostUpdate 0xB26FD0)   -> sample the player after its update
//   PadProxyPC vftable slot (PadProxyPC__Update 0x909C90) -> input record / inject, hotkeys, clock control
// Hooks chain to whatever was there before (ac1_windowed.dll may have been loaded first).
#include "lab.h"
#include <stdarg.h>
#include <share.h>
#include <time.h>

Lab g_lab;

typedef IDirect3D9 *(WINAPI *Direct3DCreate9_t)(UINT);
typedef HRESULT(STDMETHODCALLTYPE *CreateDevice_t)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD,
                                                   D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
typedef HRESULT(STDMETHODCALLTYPE *Reset_t)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
typedef HRESULT(STDMETHODCALLTYPE *Present_t)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef int(__fastcall *ThisNoArgs_t)(void *self, void *edx);

static Direct3DCreate9_t real_Direct3DCreate9;
static CreateDevice_t real_CreateDevice;
static Reset_t real_Reset;
static Present_t real_Present;
typedef HRESULT(STDMETHODCALLTYPE *SCPresent_t)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *, DWORD);
static SCPresent_t real_SCPresent;
static IDirect3DDevice9 *g_dev;
static ThisNoArgs_t real_HumanPostUpdate;
static ThisNoArgs_t real_PadUpdate;
static ThisNoArgs_t real_InterpUpdate;
static WNDPROC g_orig_wndproc;

void lab_log(const char *fmt, ...)
{
    va_list ap;
    char line[2048];
    int n;
    if (!g_lab.log) return;
    n = sprintf_s(line, sizeof(line), "[%9.3f] ", lab_now());
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    fputs(line, g_lab.log);
    fputc('\n', g_lab.log);
    fflush(g_lab.log);
}

double lab_now(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_lab.t0.QuadPart) / (double)g_lab.qpf.QuadPart;
}

void lab_session_path(char *out, size_t n, const char *name)
{
    sprintf_s(out, n, "%s\\%s", g_lab.session, name);
}

void *lab_patch_slot(void **slot, void *hook)
{
    void *old = *slot;
    DWORD prot;
    if (old == hook) return NULL;
    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &prot)) return NULL;
    *slot = hook;
    VirtualProtect(slot, sizeof(void *), prot, &prot);
    return old;
}

// Replaces one import of `mod` by name and returns the pointer that was there (possibly another hook).
static void *patch_iat(HMODULE mod, const char *dll, const char *name, void *hook)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d;
    if (!dir.VirtualAddress) return NULL;
    for (d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; d++) {
        IMAGE_THUNK_DATA *orig, *thunk;
        if (_stricmp((char *)(base + d->Name), dll) || !d->OriginalFirstThunk) continue;
        orig = (IMAGE_THUNK_DATA *)(base + d->OriginalFirstThunk);
        thunk = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; thunk->u1.Function; orig++, thunk++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            if (IMAGE_SNAP_BY_ORDINAL(orig->u1.Ordinal)) continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            if (!strcmp((char *)ibn->Name, name)) {
                void *old = (void *)thunk->u1.Function;
                DWORD prot;
                VirtualProtect(&thunk->u1.Function, sizeof(void *), PAGE_READWRITE, &prot);
                thunk->u1.Function = (ULONG_PTR)hook;
                VirtualProtect(&thunk->u1.Function, sizeof(void *), prot, &prot);
                return old;
            }
        }
    }
    return NULL;
}

// ---- game-thread hooks ----

static int __fastcall hk_HumanPostUpdate(void *self, void *edx)
{
    int r = real_HumanPostUpdate(self, edx);
    if (game_consider_human((uintptr_t)self)) {
        LabFrame f;
        game_sample(&f, (uintptr_t)self);
        trace_frame(&f);
        EnterCriticalSection(&g_lab.lock);
        g_lab.last = f;
        g_lab.frames++;
        LeaveCriticalSection(&g_lab.lock);
    }
    return r;
}

// GoAssassinActionInterpreter__Update 0xEEE700 (only the player has this interpreter): remember it
static int __fastcall hk_InterpUpdate(void *self, void *edx)
{
    g_lab.interp = (uintptr_t)self;
    abilities_frame((uintptr_t)self);
    return real_InterpUpdate(self, edx);
}

static int __fastcall hk_PadUpdate(void *self, void *edx)
{
    int r = real_PadUpdate(self, edx);
    InterlockedIncrement(&g_lab.pad_updates);
    input_pad_update((uintptr_t)self);
    control_frame();
    return r;
}

// ---- D3D9 ----

static LRESULT CALLBACK hk_WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // keep simulating while another window (Claude, an editor) has focus
    if (g_lab.background) {
        if (msg == WM_ACTIVATEAPP && !wp) return 0;
        if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) return 0;
        if (msg == WM_KILLFOCUS) return 0;
    }
    return CallWindowProcA(g_orig_wndproc, wnd, msg, wp, lp);
}

static HRESULT STDMETHODCALLTYPE hk_Present(IDirect3DDevice9 *dev, const RECT *src, const RECT *dst, HWND wnd,
                                            const RGNDATA *dirty)
{
    InterlockedIncrement(&g_lab.presents);
    if (g_lab.shot_pending == 1) overlay_capture(dev);   // before the overlay: the game alone
    overlay_present(dev);
    if (g_lab.shot_pending == 2) overlay_capture(dev);   // "shot ui": with the overlay
    {
        static int dbg = 5;
        HRESULT hr = real_Present(dev, src, dst, wnd, dirty);
        if (dbg > 0 || FAILED(hr)) {
            if (dbg > -20) lab_log("Present -> 0x%08lX (coop 0x%08lX)", hr, IDirect3DDevice9_TestCooperativeLevel(dev));
            dbg--;
        }
        return hr;
    }
}

// the game presents through the implicit swap chain (IDirect3DSwapChain9::Present) after its first frame
static HRESULT STDMETHODCALLTYPE hk_SCPresent(IDirect3DSwapChain9 *sc, const RECT *src, const RECT *dst, HWND wnd,
                                              const RGNDATA *dirty, DWORD flags)
{
    if (g_dev) {
        InterlockedIncrement(&g_lab.presents);
        if (g_lab.shot_pending == 1) overlay_capture(g_dev);
        overlay_present(g_dev);
        if (g_lab.shot_pending == 2) overlay_capture(g_dev);
    }
    return real_SCPresent(sc, src, dst, wnd, dirty, flags);
}

static HRESULT STDMETHODCALLTYPE hk_Reset(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp)
{
    HRESULT hr;
    overlay_lost();
    hr = real_Reset(dev, pp);
    if (SUCCEEDED(hr)) overlay_reset();
    return hr;
}

static HRESULT STDMETHODCALLTYPE hk_CreateDevice(IDirect3D9 *d3d, UINT adapter, D3DDEVTYPE type, HWND focus,
                                                 DWORD flags, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    HRESULT hr = real_CreateDevice(d3d, adapter, type, focus, flags, pp, out);
    void *p;
    HWND w;
    lab_log("CreateDevice -> 0x%08lX (%ux%u windowed=%d)", hr, pp->BackBufferWidth, pp->BackBufferHeight, pp->Windowed);
    if (FAILED(hr) || !out || !*out) return hr;
    if ((p = lab_patch_slot((void **)&(*out)->lpVtbl->Reset, (void *)hk_Reset))) real_Reset = (Reset_t)p;
    if ((p = lab_patch_slot((void **)&(*out)->lpVtbl->Present, (void *)hk_Present))) real_Present = (Present_t)p;
    {
        IDirect3DSwapChain9 *sc = NULL;
        if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(*out, 0, &sc)) && sc) {
            if ((p = lab_patch_slot((void **)&sc->lpVtbl->Present, (void *)hk_SCPresent))) real_SCPresent = (SCPresent_t)p;
            IDirect3DSwapChain9_Release(sc);
        }
    }
    g_dev = *out;
    lab_log("hook Present %s, SwapChain Present %s, Reset %s", real_Present ? "ok" : "FAILED",
            real_SCPresent ? "ok" : "FAILED", real_Reset ? "ok" : "FAILED");
    w = pp->hDeviceWindow ? pp->hDeviceWindow : focus;
    if (g_lab.background && w && !g_orig_wndproc) {
        g_orig_wndproc = (WNDPROC)SetWindowLongA(w, GWL_WNDPROC, (LONG)(LONG_PTR)hk_WndProc);
        lab_log("wndproc subclassed (keeps running in the background)");
    }
    return hr;
}

static IDirect3D9 *WINAPI hk_Direct3DCreate9(UINT sdk)
{
    IDirect3D9 *d3d = real_Direct3DCreate9(sdk);
    void *p;
    if (d3d && (p = lab_patch_slot((void **)&d3d->lpVtbl->CreateDevice, (void *)hk_CreateDevice)))
        real_CreateDevice = (CreateDevice_t)p;
    lab_log("Direct3DCreate9 -> %p, CreateDevice hook %s", (void *)d3d, real_CreateDevice ? "ok" : "FAILED");
    return d3d;
}

// ---- start-up ----

static void make_session(void)
{
    char t[64];
    time_t now = time(NULL);
    struct tm lt;
    localtime_s(&lt, &now);
    strftime(t, sizeof(t), "%Y-%m-%d_%H%M%S", &lt);
    sprintf_s(g_lab.session, MAX_PATH, "%s\\sessions\\%s", g_lab.dir, t);
    {
        char p[MAX_PATH];
        sprintf_s(p, MAX_PATH, "%s\\sessions", g_lab.dir);
        CreateDirectoryA(p, NULL);
    }
    CreateDirectoryA(g_lab.session, NULL);
}

static void init(HMODULE self)
{
    char ini[MAX_PATH], path[MAX_PATH], *slash;
    HMODULE exe = GetModuleHandleA(NULL);
    void *p;

    QueryPerformanceFrequency(&g_lab.qpf);
    QueryPerformanceCounter(&g_lab.t0);
    InitializeCriticalSection(&g_lab.lock);
    GetModuleFileNameA(self, g_lab.dir, MAX_PATH);
    if ((slash = strrchr(g_lab.dir, '\\'))) *slash = 0;
    sprintf_s(ini, MAX_PATH, "%s\\lab.ini", g_lab.dir);
    g_lab.bridge_port = GetPrivateProfileIntA("Lab", "BridgePort", 27015, ini);
    g_lab.background = GetPrivateProfileIntA("Lab", "KeepRunningInBackground", 1, ini);
    g_lab.hotkeys = GetPrivateProfileIntA("Lab", "Hotkeys", 1, ini);
    g_lab.overlay_on = GetPrivateProfileIntA("Lab", "Overlay", 1, ini);
    g_lab.overlay_mode = GetPrivateProfileIntA("Lab", "OverlayMode", 31, ini);

    make_session();
    lab_session_path(path, MAX_PATH, "lab.log");
    g_lab.log = _fsopen(path, "w", _SH_DENYNO);
    lab_log("AC1 Lab loaded from %s; session %s", g_lab.dir, g_lab.session);
    abilities_init(ini);

    // the exe must be the build the addresses come from (EaglePatch's check: 0x401376 == 0x42D6)
    if (game_u32(0x401376) != 0x42D6 && (game_u32(0x401376) & 0xFFFF) != 0x42D6) {
        lab_log("unexpected exe build (0x401376 = 0x%08X): hooks NOT installed", game_u32(0x401376));
        return;
    }

    // SkipIntro: the start-up videos are played by sub_41D8A0 / sub_41D930 unless the jnz at 0x405495 is taken
    // (EaglePatch's skip); make it a jmp in memory
    if (GetPrivateProfileIntA("Lab", "SkipIntro", 1, ini)) {
        uint8_t b = 0, jmp = 0xEB;
        if (game_read(0x405495, &b, 1) && b == 0x75 && game_write(0x405495, &jmp, 1)) lab_log("intro videos skipped");
        else lab_log("SkipIntro: unexpected byte %02X at 0x405495, left alone", b);
    }
    if ((p = patch_iat(exe, "d3d9.dll", "Direct3DCreate9", (void *)hk_Direct3DCreate9)))
        real_Direct3DCreate9 = (Direct3DCreate9_t)p;
    lab_log("hook Direct3DCreate9: %s", real_Direct3DCreate9 ? "ok" : "FAILED");
    if ((p = lab_patch_slot((void **)VT_HUMAN_POSTUPDATE, (void *)hk_HumanPostUpdate)))
        real_HumanPostUpdate = (ThisNoArgs_t)p;
    lab_log("hook Human__PostUpdate (was %p): %s", p, real_HumanPostUpdate ? "ok" : "FAILED");
    if ((p = lab_patch_slot((void **)VT_PADPROXY_UPDATE, (void *)hk_PadUpdate))) real_PadUpdate = (ThisNoArgs_t)p;
    lab_log("hook PadProxyPC__Update (was %p): %s", p, real_PadUpdate ? "ok" : "FAILED");
    if ((p = lab_patch_slot((void **)VT_INTERP_UPDATE, (void *)hk_InterpUpdate))) real_InterpUpdate = (ThisNoArgs_t)p;
    lab_log("hook GoAssassinActionInterpreter__Update (was %p): %s", p, real_InterpUpdate ? "ok" : "FAILED");

    game_load_names();
    probes_init();
    checks_load();
    bridge_start(g_lab.bridge_port);
    g_lab.ready = 1;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        init(self);
    }
    return TRUE;
}
