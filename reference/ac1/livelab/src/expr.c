// Tiny address expressions for probes and fields.
//   expr  := rel
//   rel   := sum [('==' | '!=' | '<' | '>' | '&') sum]
//   sum   := term (('+' | '-') term)*
//   term  := number | register | $variable | '[' expr ']'      ([x] reads the dword at x)
// A field is "name=expr:type" (or "expr:type"); with a type the value is read at the address, without one the
// expression's own value is printed. Types: u8 u16 u32 i32 f32 hex vec3 ctx act str bits.
// Variables: $human $entity $controller $hdata (HumanDataBundle) $ctx (current locomotion context) $hg (HumanGround)
//            $world (the current scimitar::World) $engine $player (= $entity) $interp (the player's
//            GoAssassinActionInterpreter)
#include "lab.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *p; const LabEnv *env; int ok; } P;

static void ws(P *s) { while (*s->p == ' ' || *s->p == '\t') s->p++; }

static uint64_t parse_rel(P *s);

static uint64_t var(P *s)
{
    char name[32];
    int n = 0;
    uintptr_t h = s->env->human ? s->env->human : game_player_human();
    while ((isalnum((unsigned char)*s->p) || *s->p == '_') && n < 31) name[n++] = *s->p++;
    name[n] = 0;
    if (!strcmp(name, "human")) return h;
    if (!strcmp(name, "entity") || !strcmp(name, "player")) return game_player_entity();
    if (!strcmp(name, "controller")) return game_entity_component_named(game_player_entity(), "CharacterComponent@");
    if (!strcmp(name, "hdata")) return h ? game_u32(h + 0x100 + 0x30) : 0;
    if (!strcmp(name, "ctx")) return h ? game_u32(h + 0x48) : 0;
    if (!strcmp(name, "hg")) return h ? game_u32(h + 0x480) : 0;
    if (!strcmp(name, "world")) return game_world();
    if (!strcmp(name, "interp")) return g_lab.interp;
    if (!strcmp(name, "engine")) return game_u32(G_ENGINE);
    s->ok = 0;
    return 0;
}

static uint64_t reg(P *s, int *matched)
{
    static const char *R[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip"};
    const LabEnv *e = s->env;
    const uint32_t v[] = {e->eax, e->ebx, e->ecx, e->edx, e->esi, e->edi, e->ebp, e->esp, e->eip};
    int i;
    *matched = 0;
    for (i = 0; i < 9; i++)
        if (!_strnicmp(s->p, R[i], 3) && !isalnum((unsigned char)s->p[3])) {
            *matched = 1;
            s->p += 3;
            if (!e->have_regs) s->ok = 0;
            return v[i];
        }
    return 0;
}

static uint64_t term(P *s)
{
    int m;
    uint64_t v;
    ws(s);
    if (*s->p == '[') {
        s->p++;
        v = parse_rel(s);
        ws(s);
        if (*s->p == ']') s->p++; else s->ok = 0;
        {
            uint32_t d = 0;
            if (!game_read((uintptr_t)v, &d, 4)) s->ok = 0;
            return d;
        }
    }
    if (*s->p == '(') {
        s->p++;
        v = parse_rel(s);
        ws(s);
        if (*s->p == ')') s->p++; else s->ok = 0;
        return v;
    }
    if (*s->p == '$') {
        s->p++;
        return var(s);
    }
    v = reg(s, &m);
    if (m) return v;
    if (isdigit((unsigned char)*s->p)) {
        char *end;
        v = _strtoui64(s->p, &end, 0);
        s->p = end;
        return v;
    }
    s->ok = 0;
    return 0;
}

static uint64_t sum(P *s)
{
    uint64_t v = term(s);
    for (;;) {
        ws(s);
        if (*s->p == '+') { s->p++; v += term(s); }
        else if (*s->p == '-') { s->p++; v -= term(s); }
        else return v & 0xFFFFFFFFull;
    }
}

static uint64_t parse_rel(P *s)
{
    uint64_t a = sum(s), b;
    ws(s);
    if (s->p[0] == '=' && s->p[1] == '=') { s->p += 2; b = sum(s); return a == b; }
    if (s->p[0] == '!' && s->p[1] == '=') { s->p += 2; b = sum(s); return a != b; }
    if (s->p[0] == '<') { s->p++; b = sum(s); return a < b; }
    if (s->p[0] == '>') { s->p++; b = sum(s); return a > b; }
    if (s->p[0] == '&') { s->p++; b = sum(s); return a & b; }
    return a;
}

int expr_eval(const char *str, const LabEnv *env, uint64_t *value)
{
    P s;
    s.p = str;
    s.env = env;
    s.ok = 1;
    *value = parse_rel(&s);
    ws(&s);
    return s.ok && (*s.p == 0 || *s.p == ':');
}

// Formats one "expr[:type]" into out.
static int format_one(const char *ex, const LabEnv *env, char *out, size_t n)
{
    const char *colon = strrchr(ex, ':');
    char e[256];
    uint64_t v;
    size_t len = colon ? (size_t)(colon - ex) : strlen(ex);
    if (len >= sizeof(e)) return 0;
    memcpy(e, ex, len);
    e[len] = 0;
    if (!expr_eval(e, env, &v)) return 0;
    if (!colon) { sprintf_s(out, n, "0x%X", (unsigned)v); return 1; }
    colon++;
    {
        uintptr_t a = (uintptr_t)v;
        union { uint8_t b[64]; uint32_t u; int32_t i; float f[3]; uint16_t w; } m;
        memset(&m, 0, sizeof(m));
        if (!strcmp(colon, "u8")) { if (!game_read(a, m.b, 1)) return 0; sprintf_s(out, n, "%u", m.b[0]); }
        else if (!strcmp(colon, "u16")) { if (!game_read(a, m.b, 2)) return 0; sprintf_s(out, n, "%u", m.w); }
        else if (!strcmp(colon, "u32")) { if (!game_read(a, m.b, 4)) return 0; sprintf_s(out, n, "%u", m.u); }
        else if (!strcmp(colon, "i32")) { if (!game_read(a, m.b, 4)) return 0; sprintf_s(out, n, "%d", m.i); }
        else if (!strcmp(colon, "hex")) { if (!game_read(a, m.b, 4)) return 0; sprintf_s(out, n, "0x%08X", m.u); }
        else if (!strcmp(colon, "f32")) { if (!game_read(a, m.b, 4)) return 0; sprintf_s(out, n, "%.4f", m.f[0]); }
        else if (!strcmp(colon, "vec3")) {
            if (!game_read(a, m.b, 12)) return 0;
            sprintf_s(out, n, "(%.3f %.3f %.3f)", m.f[0], m.f[1], m.f[2]);
        }
        else if (!strcmp(colon, "ctx")) { if (!game_read(a, m.b, 4)) return 0; sprintf_s(out, n, "%s", game_ctx_name(m.i)); }
        else if (!strcmp(colon, "act")) {
            if (!game_read(a, m.b, 4)) return 0;
            sprintf_s(out, n, "%08X %s", m.u, game_action_name(m.u));
        }
        else if (!strcmp(colon, "bits")) {
            int i, k = 0;
            if (!game_read(a, m.b, 1)) return 0;
            for (i = 7; i >= 0 && k + 1 < (int)n; i--) out[k++] = (m.b[0] >> i) & 1 ? '1' : '0';
            out[k] = 0;
        }
        else if (!strcmp(colon, "str")) {
            int i;
            if (!game_read(a, m.b, 32)) return 0;
            for (i = 0; i < 32 && m.b[i] >= 32 && m.b[i] < 127; i++);
            m.b[i] = 0;
            sprintf_s(out, n, "%s", (char *)m.b);
        }
        else return 0;
    }
    return 1;
}

// "a=ecx+4:f32 [esp+8]:u32" -> "a=1.0000 [esp+8]=3"
int expr_format(const char *spec, const LabEnv *env, char *out, size_t n)
{
    char buf[1024], *ctx = NULL, *tok;
    size_t k = 0;
    int all = 1;
    strcpy_s(buf, sizeof(buf), spec);
    out[0] = 0;
    for (tok = strtok_s(buf, " \t", &ctx); tok; tok = strtok_s(NULL, " \t", &ctx)) {
        char val[96], *eq = strchr(tok, '=');
        const char *ex = tok;
        // "name=expr" (a lone '=' only; '==' belongs to the expression)
        if (eq && eq[1] != '=' && (eq == tok || eq[-1] != '!' )) {
            *eq = 0;
            ex = eq + 1;
        } else eq = NULL;
        if (!format_one(ex, env, val, sizeof(val))) {
            strcpy_s(val, sizeof(val), "?");
            all = 0;
        }
        if (k && k + 1 < n) out[k++] = ' ';
        if (eq) k += sprintf_s(out + k, n - k, "%s=%s", tok, val);
        else k += sprintf_s(out + k, n - k, "%s", val);
        if (k >= n - 1) break;
    }
    return all;
}

// Splits "name=expr" lines into the two arrays (name defaults to the expression).
int expr_split_fields(const char *lines, char names[][48], char exprs[][160], int max)
{
    char buf[2048], *ctx = NULL, *line;
    int n = 0;
    strcpy_s(buf, sizeof(buf), lines);
    for (line = strtok_s(buf, "\n", &ctx); line && n < max; line = strtok_s(NULL, "\n", &ctx)) {
        char *eq;
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#') continue;
        eq = strchr(line, '=');
        if (eq && eq[1] != '=') {
            *eq = 0;
            strncpy_s(names[n], 48, line, _TRUNCATE);
            strncpy_s(exprs[n], 160, eq + 1, _TRUNCATE);
        } else {
            strncpy_s(names[n], 48, line, _TRUNCATE);
            strncpy_s(exprs[n], 160, line, _TRUNCATE);
        }
        n++;
    }
    return n;
}
