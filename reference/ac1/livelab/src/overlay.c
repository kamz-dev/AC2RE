// Overlay drawn at Present: a dark panel with the live state, the current check's instructions and hotkeys.
// Text comes from a font atlas made once with GDI (Consolas, ASCII 32..126) in a managed texture and drawn as quads;
// the game's own D3DX font stalled its renderer (tested 2026-10-12: DrawText alone stops the next Present).
// All device state is saved in a state block and put back.
#include "lab.h"
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static IDirect3DTexture9 *g_atlas;
static int g_font_h, g_cw, g_ch, g_aw, g_ah;   // glyph cell and atlas size
static IDirect3DStateBlock9 *g_sb;
static int g_failed;

#define NOTES 5
static char g_notes[NOTES][160];
static double g_note_t[NOTES];
static int g_note_i;
static CRITICAL_SECTION g_ncs;
static int g_ninit;

void overlay_note(const char *fmt, ...)
{
    va_list ap;
    if (!g_ninit) { InitializeCriticalSection(&g_ncs); g_ninit = 1; }
    EnterCriticalSection(&g_ncs);
    va_start(ap, fmt);
    vsnprintf(g_notes[g_note_i], sizeof(g_notes[0]), fmt, ap);
    va_end(ap);
    g_note_t[g_note_i] = lab_now();
    g_note_i = (g_note_i + 1) % NOTES;
    LeaveCriticalSection(&g_ncs);
}

void overlay_lost(void)
{
    if (g_sb) { IDirect3DStateBlock9_Release(g_sb); g_sb = NULL; }
}

void overlay_reset(void) {}

// 16 x 6 cells of ASCII 32..127, white glyphs whose coverage becomes the alpha.
static int ensure_font(IDirect3DDevice9 *dev, int h)
{
    HDC dc;
    HFONT font, old;
    HBITMAP bmp;
    BITMAPINFO bi;
    TEXTMETRICA tm;
    uint32_t *bits = NULL;
    D3DLOCKED_RECT lr;
    int c, y;
    if (g_atlas && g_font_h == h) return 1;
    if (g_failed) return 0;
    if (g_atlas) { IDirect3DTexture9_Release(g_atlas); g_atlas = NULL; }
    dc = CreateCompatibleDC(NULL);
    font = CreateFontA(h, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    old = (HFONT)SelectObject(dc, font);
    GetTextMetricsA(dc, &tm);
    g_cw = tm.tmAveCharWidth, g_ch = tm.tmHeight;
    for (g_aw = 64; g_aw < g_cw * 16; g_aw *= 2);
    for (g_ah = 64; g_ah < g_ch * 6; g_ah *= 2);
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = g_aw, bi.bmiHeader.biHeight = -g_ah, bi.bmiHeader.biPlanes = 1, bi.bmiHeader.biBitCount = 32;
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(dc, bmp);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, OPAQUE);
    for (c = 32; c < 128; c++) {
        char ch = (char)c;
        TextOutA(dc, ((c - 32) % 16) * g_cw, ((c - 32) / 16) * g_ch, &ch, 1);
    }
    GdiFlush();
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, g_aw, g_ah, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &g_atlas, NULL)) ||
        FAILED(IDirect3DTexture9_LockRect(g_atlas, 0, &lr, NULL, 0))) {
        lab_log("overlay: font texture failed");
        g_failed = 1;
    } else {
        for (y = 0; y < g_ah; y++) {
            uint32_t *dst = (uint32_t *)((uint8_t *)lr.pBits + y * lr.Pitch), *src = bits + y * g_aw;
            int x;
            for (x = 0; x < g_aw; x++) dst[x] = ((src[x] >> 8) & 0xFF) << 24 | 0x00FFFFFF;  // green channel -> alpha
        }
        IDirect3DTexture9_UnlockRect(g_atlas, 0);
        g_font_h = h;
        lab_log("overlay font %d px (cell %dx%d, atlas %dx%d)", h, g_cw, g_ch, g_aw, g_ah);
    }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteObject(font);
    DeleteDC(dc);
    return !g_failed;
}

typedef struct { float x, y, z, rhw; DWORD c; } V;
typedef struct { float x, y, z, rhw; DWORD c; float u, v; } VT;

static void box(IDirect3DDevice9 *dev, float x0, float y0, float x1, float y1, DWORD c)
{
    V v[4] = {{x0, y0, 0, 1, c}, {x1, y0, 0, 1, c}, {x0, y1, 0, 1, c}, {x1, y1, 0, 1, c}};
    IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, v, sizeof(V));
}

// Lays out `s` in rows of `cols` columns, breaking at spaces; calls emit(col, row, char) for each glyph.
static int layout(const char *s, int cols, void (*emit)(int, int, char, void *), void *u)
{
    int col = 0, row = 0;
    if (cols < 8) cols = 8;
    while (*s) {
        if (*s == '\n') { col = 0, row++, s++; continue; }
        if (*s != ' ') {
            int len = 0;
            while (s[len] && s[len] != ' ' && s[len] != '\n') len++;
            if (col + len > cols && col > 0) col = 0, row++;
            while (len--) {
                if (col >= cols) col = 0, row++;
                if (emit) emit(col, row, *s, u);
                col++, s++;
            }
        } else {
            if (col > 0) col++;   // no leading space on a wrapped row
            s++;
        }
    }
    return row + 1;
}

typedef struct { VT v[6 * 512]; int n; float x, y; DWORD color; IDirect3DDevice9 *dev; } Batch;

static void flush(Batch *b)
{
    if (b->n) IDirect3DDevice9_DrawPrimitiveUP(b->dev, D3DPT_TRIANGLELIST, b->n * 2, b->v, sizeof(VT));
    b->n = 0;
}

static void emit_glyph(int col, int row, char ch, void *u)
{
    Batch *b = (Batch *)u;
    int c = (unsigned char)ch;
    float x0, y0, x1, y1, u0, v0, u1, v1;
    VT *q;
    if (c < 33 || c > 126) return;
    if (b->n == 512) flush(b);
    x0 = b->x + col * g_cw - 0.5f, y0 = b->y + row * g_ch - 0.5f, x1 = x0 + g_cw, y1 = y0 + g_ch;
    u0 = (float)(((c - 32) % 16) * g_cw) / g_aw, v0 = (float)(((c - 32) / 16) * g_ch) / g_ah;
    u1 = u0 + (float)g_cw / g_aw, v1 = v0 + (float)g_ch / g_ah;
    q = &b->v[b->n * 6];
    q[0].x = x0, q[0].y = y0, q[0].u = u0, q[0].v = v0;
    q[1].x = x1, q[1].y = y0, q[1].u = u1, q[1].v = v0;
    q[2].x = x0, q[2].y = y1, q[2].u = u0, q[2].v = v1;
    q[3] = q[1];
    q[4].x = x1, q[4].y = y1, q[4].u = u1, q[4].v = v1;
    q[5] = q[2];
    {
        int i;
        for (i = 0; i < 6; i++) q[i].z = 0, q[i].rhw = 1, q[i].c = b->color;
    }
    b->n++;
}

static IDirect3DDevice9 *g_text_dev;

static int text(int x, int y, int w, const char *s, DWORD color)
{
    static Batch b;
    IDirect3DDevice9 *dev = g_text_dev;
    int rows;
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)g_atlas);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    b.dev = dev, b.n = 0;
    // shadow, then the text
    b.x = (float)x + 1, b.y = (float)y + 1, b.color = 0xC0000000;
    layout(s, w / g_cw, emit_glyph, &b);
    flush(&b);
    b.x = (float)x, b.y = (float)y, b.color = color;
    rows = layout(s, w / g_cw, emit_glyph, &b);
    flush(&b);
    IDirect3DDevice9_SetTexture(dev, 0, NULL);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    return rows * g_ch;
}

static int measure(int w, const char *s)
{
    return layout(s, w / g_cw, NULL, NULL) * g_ch;
}

static void setup_states(IDirect3DDevice9 *dev)
{
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    IDirect3DDevice9_SetTexture(dev, 0, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
}

static void compose(char *main, size_t n, char *check, size_t cn)
{
    LabFrame f;
    const LabCheck *c = checks_current();
    char probes[1024];
    size_t k;
    int i;
    float hs;
    EnterCriticalSection(&g_lab.lock);
    f = g_lab.last;
    LeaveCriticalSection(&g_lab.lock);
    hs = sqrtf(f.vel[0] * f.vel[0] + f.vel[1] * f.vel[1]);
    k = sprintf_s(main, n, "AC1 LAB%s%s%s%s  x%.2f\n", trace_active() ? "  [REC]" : "", control_paused() ? "  [PAUSED]" : "",
                  input_playing() ? "  [SCRIPT]" : "", input_recording() ? "  [PAD REC]" : "", f.scale);
    if (!f.human) {
        k += sprintf_s(main + k, n - k, "no player yet (load a save)\n");
    } else {
        k += sprintf_s(main + k, n - k,
                       "%s  (prev %s)\naction %08X %s  word %04X\npos %.2f %.2f %.2f  speed h %.2f v %.2f\ndt %.4f  frame %u\n",
                       game_ctx_name(f.ctx), game_ctx_name(f.prev_ctx), f.action, game_action_name(f.action), f.item_word,
                       f.pos[0], f.pos[1], f.pos[2], hs, f.vel[2], f.dt, f.frame);
    }
    if (c) {
        char names[LAB_MAX_FIELDS][48], exprs[LAB_MAX_FIELDS][160];
        int nf = 0;
        checks_fields(names, exprs, &nf);
        for (i = 0; i < f.nfields && i < nf && k + 80 < n; i++) k += sprintf_s(main + k, n - k, "%s = %s\n", names[i], f.fields[i]);
        probes_status(probes, sizeof(probes));
        if (probes[0]) k += sprintf_s(main + k, n - k, "probes (logged/hits):\n%s", probes);
    }
    if (g_ninit) {
        double now = lab_now();
        EnterCriticalSection(&g_ncs);
        for (i = 0; i < NOTES; i++) {
            int j = (g_note_i + i) % NOTES;
            if (g_notes[j][0] && now - g_note_t[j] < 6.0 && k + 170 < n) k += sprintf_s(main + k, n - k, "> %s\n", g_notes[j]);
        }
        LeaveCriticalSection(&g_ncs);
    }
    if (c) sprintf_s(check, cn, "CHECK %s / %d: %s\n\n%s", c->id, checks_count(), c->title, c->steps);
    else sprintf_s(check, cn, "no checks loaded");
}

void overlay_present(IDirect3DDevice9 *dev)
{
    IDirect3DSurface9 *bb = NULL, *old_rt = NULL;
    D3DSURFACE_DESC d;
    D3DVIEWPORT9 vp, old_vp;
    static char main_text[LAB_TEXT], check_text[LAB_TEXT];
    static const char *keys =
        "F1 overlay  F2/Shift+F2 check  F3 record  F4 mark (Shift: odd)  F5/F6 save/load pos (Ctrl: slot 2)  "
        "F7 speed  F8 pause  F10 step  F11 run check script  F12 pad rec";
    int fh, w, x, y, hm, hc, pad;
    static int dbg = 3;   // log the steps of the first frames
#define STEP(m) do { if (dbg > 0) lab_log("overlay: %s", m); } while (0)
    if (!g_lab.overlay_on || g_failed) return;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    IDirect3DSurface9_GetDesc(bb, &d);
    fh = (int)d.Height / 54;
    if (fh < 12) fh = 12;
    STEP("font");
    g_text_dev = dev;
    if (!ensure_font(dev, fh)) { IDirect3DSurface9_Release(bb); return; }
    {
        int mode = g_lab.overlay_mode, began = 0;
        STEP("state block");
        if ((mode & 1) && !g_sb && FAILED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &g_sb))) mode &= ~1;
        if (mode & 1) IDirect3DStateBlock9_Capture(g_sb);
        if (mode & 8) {
            IDirect3DDevice9_GetRenderTarget(dev, 0, &old_rt);
            IDirect3DDevice9_GetViewport(dev, &old_vp);
            IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
            vp.X = 0, vp.Y = 0, vp.Width = d.Width, vp.Height = d.Height, vp.MinZ = 0, vp.MaxZ = 1;
            IDirect3DDevice9_SetViewport(dev, &vp);
        }
        STEP("begin scene");
        if (mode & 16) began = SUCCEEDED(IDirect3DDevice9_BeginScene(dev));
        STEP("compose");
        compose(main_text, sizeof(main_text), check_text, sizeof(check_text));
        STEP("draw");
        pad = fh / 2;
        w = (int)d.Width * 34 / 100;
        x = pad, y = pad;
        hm = measure(w, main_text);
        if (mode & 2) { setup_states(dev); box(dev, (float)x - pad, (float)y - pad, (float)(x + w + pad), (float)(y + hm + pad), 0xB0000000); }
        if (mode & 4) text(x, y, w, main_text, 0xFFE8E8E8);
        // the check panel on the right
        {
            int cw = (int)d.Width * 34 / 100, cx = (int)d.Width - cw - pad;
            hc = measure(cw, check_text);
            if (mode & 2) { setup_states(dev); box(dev, (float)cx - pad, 0, (float)(cx + cw + pad), (float)(pad + hc + pad), 0xB0101828); }
            if (mode & 4) text(cx, pad, cw, check_text, 0xFFFFE7A0);
        }
        // the hotkeys along the bottom
        {
            int kh = measure((int)d.Width - 2 * pad, keys), ky = (int)d.Height - kh - pad;
            if (mode & 2) { setup_states(dev); box(dev, 0, (float)ky - pad / 2, (float)d.Width, (float)d.Height, 0x90000000); }
            if (mode & 4) text(pad, ky, (int)d.Width - 2 * pad, keys, 0xFFB0D0FF);
        }
        STEP("end scene");
        if (began) IDirect3DDevice9_EndScene(dev);
        if (mode & 8) {
            IDirect3DDevice9_SetRenderTarget(dev, 0, old_rt);
            IDirect3DDevice9_SetViewport(dev, &old_vp);
            if (old_rt) IDirect3DSurface9_Release(old_rt);
        }
        if (mode & 1) IDirect3DStateBlock9_Apply(g_sb);
    }
    IDirect3DSurface9_Release(bb);
    STEP("done");
    if (dbg > 0) dbg--;
}

// Saves the back buffer (half size, 24-bit BMP) to g_lab.shot_path. Render thread, at Present.
void overlay_capture(IDirect3DDevice9 *dev)
{
    IDirect3DSurface9 *bb = NULL, *sys = NULL;
    D3DSURFACE_DESC d;
    D3DLOCKED_RECT lr;
    FILE *f = NULL;
    InterlockedExchange(&g_lab.shot_pending, 0);
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    IDirect3DSurface9_GetDesc(bb, &d);
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, bb, sys)) &&
        SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
        if (!fopen_s(&f, g_lab.shot_path, "wb")) {
            int w = (int)d.Width / 2, h = (int)d.Height / 2, row = (w * 3 + 3) & ~3, x, y;
            BITMAPFILEHEADER fh = {0x4D42, (DWORD)(54 + row * h), 0, 0, 54};
            BITMAPINFOHEADER ih = {40, w, h, 1, 24, BI_RGB, (DWORD)(row * h), 0, 0, 0, 0};
            uint8_t *line = (uint8_t *)calloc(1, row);
            fwrite(&fh, sizeof(fh), 1, f);
            fwrite(&ih, sizeof(ih), 1, f);
            for (y = h - 1; y >= 0; y--) {
                const uint8_t *src = (const uint8_t *)lr.pBits + (size_t)(y * 2) * lr.Pitch;
                for (x = 0; x < w; x++) memcpy(line + x * 3, src + x * 8, 3);   // X8R8G8B8 -> BGR
                fwrite(line, 1, row, f);
            }
            free(line);
            fclose(f);
            lab_log("screenshot %s (%ux%u fmt %d)", g_lab.shot_path, d.Width, d.Height, d.Format);
        }
        IDirect3DSurface9_UnlockRect(sys);
    } else lab_log("screenshot failed (fmt %d)", d.Format);
    if (sys) IDirect3DSurface9_Release(sys);
    IDirect3DSurface9_Release(bb);
}
