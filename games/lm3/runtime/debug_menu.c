/* Luigi's Mansion 3's debug menu (LM3_DEBUG=1): the game's own level menu switched back on, and commands for tests.
 *
 * What the retail game still has (main 79e5950f, offsets from its base):
 *   - DebugLevelMenu (object at +0x1c74780, made by the flow script's StartupLevelMenuState): a list of folders and of the
 *     272 levels of the table at +0x19c7638 (0x88 bytes each: story checkpoints, rooms, unit-test levels), drawn in
 *     LevelMenuState by the job "DebugLevelMenu::Render". It moves with the pad, A enters a folder or starts a level, B
 *     goes up. Its boxes and icons are drawn; the call that drew the names was compiled out.
 *   - The game starts in that menu instead of the title when the byte at +0x1c70608 is set; the game sets it from
 *     user/UseTestLevels of ini/special_build.ini, a file the retail RomFS does not have.
 * So here: the byte is set at start, and the names of the menu the game shows are drawn over it (pcmenu.c
 * pcmenu_debug_list), read from the game's own menu each frame. In a level, F2 (or Minus + L + R) opens a panel of this
 * file's own: back to the level menu, and the game's spawners near Luigi (see panel).
 *
 * LM3_DEBUG_CMD=<file>: commands for scripted tests and for looking at the game, one a line, run on the game's main
 * thread at its pad read and then the file is emptied; answers go to the log as "lm3dbg:" lines.
 *   levels                    the 272 levels: index, name, title
 *   level <index>             start it (from a level: back to the level menu first)
 *   states | goto <state>     the game's state stack; ask for a state by name (STATES)
 *   ents <component>          the objects that have a component (cogSpawner, cogLuigi, cogGhost, ...)
 *   obj <index> | spawn <index>   an object's name hash, zone, position and components; make its cogSpawner spawn
 *   peek <address> [quads] | call <address> [x0 .. x7]   guest memory, and a guest function (hex) */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "runtime.h"
#include "hostrt.h"

int cxxrt_readable(uint64_t p, size_t n);                  /* nncxxrt.c */
void pcmenu_debug_list(const char *title, const char *const *rows, int n, int sel, const char *foot);      /* pcmenu.c */

#define G(off) (GUEST_BASE + (uint64_t)(off))
#define MENU G(0x1c74780)              /* DebugLevelMenu *: +8 chosen row, +0x18 the menu shown */
#define GSM G(0x1cea920)               /* GameStateManager *: u32 count, +8 the stack of states */
#define LEVELS G(0x19c7638)            /* 272 x 0x88 */
#define NLEVELS 272
#define PROVIDER G(0x1c705f8)          /* the level menu's side of the game: vtable +0x38 choose(index), +0x40 start */
static const struct { const char *name; uint32_t off; } STATES[] = {      /* State *: +0x30 name, +0x38 id (the request heap's order) */
    {"ExitAppState", 0x1c68be8}, {"ShutdownGameplayState", 0x1c68c40}, {"RestartGameplayState", 0x1c68c98}, {"LoadProfileState", 0x1c68cf0},
    {"ReloadCheckpointState", 0x1c68d48}, {"RestartIntoLevelMenuState", 0x1c68da0}, {"RestartIntoStartScreenState", 0x1c68df8},
    {"PausedState", 0x1c68ec0}, {"StartupLevelMenuState", 0x1c690c8}, {"LevelMenuState", 0x1c69120}, {"ShutdownLevelMenuState", 0x1c69178},
    {"StartupGameplayState", 0x1c69228}, {"GameplayState", 0x1c69280},
};

/* the game's own objects are read as they are (a null pointer reads as 0); only `peek`, which is given any address, asks
 * first whether it can be read (cxxrt_readable costs too much for every frame) */
static uint64_t r64(uint64_t a) { return a ? *(uint64_t *)(uintptr_t)a : 0; }
static uint32_t r32(uint64_t a) { return a ? *(uint32_t *)(uintptr_t)a : 0; }
static const char *rstr(uint64_t a) { return a ? (const char *)(uintptr_t)a : "?"; }
/* a guest function on the game's own thread, from inside an import: the caller's registers come back but x0 */
static uint64_t gcall(Ctx *c, uint64_t fn, const uint64_t *a, int n) {
    Ctx keep = *c;
    for (int i = 0; i < n && i < 8; i++) c->x[i] = a[i];
    call_guest(c, fn);
    uint64_t r = c->x[0];
    *c = keep;
    return r;
}
static uint64_t top_state(void) { uint64_t m = r64(GSM); uint32_t n = r32(m); return n ? r64(r64(m + 8) + (n - 1) * 8ull) : 0; }

/* the game's own way to change state (as the level menu's "start", +0x7ef60): {1, state, 0} into the top state's heap of
 * requests (+8: 0x18 bytes each, +0x14 their count), ordered by the state's id; the GameStateManager job takes them */
static void request_state(uint64_t st) {
    uint64_t top = top_state(), q = r64(top + 8);
    uint32_t n = r32(top + 0x14);
    if (!top || !q || !st || n >= 30) { printf("lm3dbg: no state change (top %llx, %u requests)\n", (unsigned long long)top, n); return; }
    struct Req { uint32_t cmd, pad; uint64_t st, arg; } *h = (struct Req *)(uintptr_t)q, v = {1, 0, st, 0};
    uint32_t i = n;
    h[i] = v;
    while (i) {
        uint32_t p = (i - 1) / 2;
        if (r32(h[p].st + 0x38) > r32(h[i].st + 0x38)) break;
        struct Req t = h[p]; h[p] = h[i]; h[i] = t; i = p;
    }
    *(uint32_t *)(uintptr_t)(top + 0x14) = n + 1;
}

/* ---- the names of the game's level menu ---- */
static uint64_t list_next(uint64_t tail, uint64_t node) { return !tail ? 0 : !node ? r64(tail) : node == tail ? 0 : r64(node); }      /* a ring with a tail pointer */
static int level_menu_names(void) {
    static char text[400][72];
    static const char *row[400];
    uint64_t dm = r64(MENU), m = dm && top_state() == r64(G(0x1c69120)) ? r64(dm + 0x18) : 0;
    if (!m) return 0;
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {                 /* the folders (+0x10), then the levels (+0x28) */
        uint64_t tail = r64(m + (pass ? 0x28 : 0x10));
        for (uint64_t e = list_next(tail, 0); e && n < 400; e = list_next(tail, e))
            snprintf(text[n], sizeof text[n], pass ? "%s" : "[%s]", rstr(r64(r64(e + 0x10)))), row[n] = text[n], n++;
    }
    pcmenu_debug_list(rstr(r64(m)), row, n, (int)r32(dm + 8), r64(m + 0x40) ? "A: open / start    B: back" : "A: open / start");
    return 1;
}

/* ---- the game's objects ----
 * The table at +0x18868e0: 0x20000 object pointers by the low 17 bits of a handle (its generations, u16 each, at
 * +0xffff8 of the table; a handle = generation << 17 | index). An object's type (+0x20) has an open hash of its
 * components: +0x30 the slot count, +0x20 the name hashes (h * 33 + c from 0xffffffff), +0x28 6 bytes a slot whose first
 * u16 is the component's offset in the object. A component: its vtable, and at +8 (u16) its offset back to the object;
 * the object's matrix (+0x58): three axes and the position, a vec4 each. */
#define OBJECTS G(0x18868e0)
static uint32_t hash33(const char *s) { uint32_t h = ~0u; while (*s) h = h * 33 + (uint8_t)*s++; return h; }
static uint64_t object_at(uint32_t i) {                    /* a live object: a pointer (a free slot holds a small number), whose own handle (+0x34) is this slot's */
    uint64_t o = i < 0x20000 ? r64(OBJECTS + i * 8ull) : 0;
    uint32_t gen = *(uint16_t *)(uintptr_t)(OBJECTS + 0xffff8 + (i & 0x1ffff) * 2ull);
    return o >> 32 && !(o & 7) && gen && r32(o + 0x34) == (gen << 17 | i) ? o : 0;
}
static uint64_t component(uint64_t obj, uint32_t hash) {
    uint64_t ti = r64(obj + 0x20), hs = r64(ti + 0x20), es = r64(ti + 0x28);
    uint32_t n = r32(ti + 0x30);
    for (uint32_t i = 0; hs && es && i < n && i < 4096; i++)
        if (r32(hs + i * 4ull) == hash && !*(uint16_t *)(uintptr_t)(es + i * 6ull + 2)) return obj + *(uint16_t *)(uintptr_t)(es + i * 6ull);
    return 0;
}

/* the name behind a hash, when the program's own text has it (its read-only data, +0x13c87cb .. +0x1692000, hashed once) */
static const char *name_of(uint32_t h) {
    enum { N = 1 << 17 };
    static struct { uint32_t h; const char *s; } *t;
    static char hex[16];
    if (!t && (t = calloc(N, sizeof *t)))
        for (const char *p = (const char *)(uintptr_t)G(0x13c87cb), *e = (const char *)(uintptr_t)G(0x1692000); p < e; p += strlen(p) + 1) {
            size_t n = strlen(p), k;
            for (k = 0; k < n && p[k] > 0x20 && p[k] < 0x7f; k++) ;
            if (n < 3 || n > 80 || k < n) continue;
            uint32_t v = hash33(p), i = v & (N - 1);
            while (t[i].s && t[i].h != v) i = (i + 1) & (N - 1);
            t[i].h = v; t[i].s = p;
        }
    for (uint32_t i = h & (N - 1); t && t[i].s; i = (i + 1) & (N - 1)) if (t[i].h == h) return t[i].s;
    snprintf(hex, sizeof hex, "#%08x", h);
    return hex;
}
static void describe(uint32_t i) {
    uint64_t o = object_at(i);
    if (!o) { printf("lm3dbg: no object %x\n", i); return; }
    uint64_t ti = r64(o + 0x20), hs = r64(ti + 0x20), m = r64(o + 0x58);
    const float *p = m ? (const float *)(uintptr_t)(m + 0x30) : NULL;
    printf("lm3dbg: object %x '%s' zone %u at %.1f %.1f %.1f:", i, name_of(r32(o + 0x30)), r32(o + 0x40), p ? p[0] : 0, p ? p[1] : 0, p ? p[2] : 0);
    for (uint32_t k = 0; hs && k < r32(ti + 0x30) && k < 4096; k++) if (r32(hs + k * 4ull)) printf(" %s", name_of(r32(hs + k * 4ull)));
    printf("\n");
}

/* the `level` command's level, once the game is in its level menu: as the menu's A on it (choose, then start) */
static int g_level = -1;
static void level_start(Ctx *c) {
    if (g_level < 0 || g_level >= NLEVELS || !r64(MENU) || top_state() != r64(G(0x1c69120))) return;
    uint64_t vt = r64(PROVIDER), a[2] = {PROVIDER, (uint64_t)g_level};
    g_level = -1;
    gcall(c, r64(vt + 0x38), a, 2);
    gcall(c, r64(vt + 0x40), a, 1);
}

/* ---- commands ---- */
static void run(Ctx *c, char *line) {
    char cmd[32] = "", arg[96] = "";
    unsigned long long v[9] = {0};
    int n = sscanf(line, "%31s %95[^\n]", cmd, arg);
    if (n < 1 || cmd[0] == '#') return;
    printf("lm3dbg: > %s", line);
    if (!strcmp(cmd, "states")) {
        uint64_t m = r64(GSM);
        for (uint32_t i = 0; i < r32(m); i++) { uint64_t s = r64(r64(m + 8) + i * 8ull); printf("lm3dbg: state %u: %s (%llx)\n", i, rstr(r64(s + 0x30)), (unsigned long long)s); }
    } else if (!strcmp(cmd, "goto")) {
        for (size_t i = 0; i < sizeof STATES / sizeof STATES[0]; i++) if (!strcmp(arg, STATES[i].name)) { request_state(r64(G(STATES[i].off))); return; }
        printf("lm3dbg: no state '%s'\n", arg);
    } else if (!strcmp(cmd, "levels")) {
        for (int i = 0; i < NLEVELS; i++) {
            uint64_t e = LEVELS + i * 0x88ull, vt = r64(e), a[1] = {e};
            printf("lm3dbg: level %3d: %-48s | %s\n", i, rstr(gcall(c, r64(vt), a, 1)), rstr(gcall(c, r64(vt + 8), a, 1)));
        }
    } else if (!strcmp(cmd, "level")) {                    /* started from the level menu (level_start), the game's only way: from a level, by way of the menu */
        g_level = atoi(arg);
        if (top_state() != r64(G(0x1c69120))) request_state(r64(G(0x1c68da0)));     /* RestartIntoLevelMenuState */
    } else if (!strcmp(cmd, "ents")) {                     /* ents <component name>: the objects that have it */
        uint32_t h = hash33(arg);
        int found = 0;
        for (uint32_t i = 0; i < 0x20000; i++) {
            uint64_t o = object_at(i);
            if (!o) continue;
            uint64_t cp = component(o, h), m = r64(o + 0x58);
            if (!cp) continue;
            const float *p = m ? (const float *)(uintptr_t)(m + 0x30) : NULL;
            printf("lm3dbg: object %5x '%s' zone %u at %.1f %.1f %.1f, %s %llx: %08x %08x %08x %08x", i, name_of(r32(o + 0x30)), r32(o + 0x40), p ? p[0] : 0, p ? p[1] : 0, p ? p[2] : 0,
                   arg, (unsigned long long)cp, r32(cp + 0x10), r32(cp + 0x14), r32(cp + 0x18), r32(cp + 0x1c));
            if (h == hash33("cogSpawner")) printf(" spawns '%s'", name_of(r32(cp + 0x10) != ~0u ? r32(cp + 0x10) : r32(cp + 0x14)));
            printf("\n");
            found++;
        }
        printf("lm3dbg: %d object(s) with %s (%08x)\n", found, arg, h);
    } else if (!strcmp(cmd, "spawn")) {                    /* spawn <object index, hex>: that object's cogSpawner spawns (cogSpawner::Spawn, +0x2e4980) */
        uint64_t o = object_at((uint32_t)strtoul(arg, NULL, 16)), a[1] = {o ? component(o, hash33("cogSpawner")) : 0};
        if (!a[0]) { printf("lm3dbg: no spawner there\n"); return; }
        describe((uint32_t)gcall(c, G(0x2e4980), a, 1) & 0x1ffff);
    } else if (!strcmp(cmd, "obj")) {                      /* obj <object index, hex>: its name, place and components */
        describe((uint32_t)strtoul(arg, NULL, 16));
    } else if (!strcmp(cmd, "peek")) {
        int k = sscanf(arg, "%llx %llu", &v[0], &v[1]);
        for (unsigned long long i = 0; k >= 1 && i < (k > 1 ? v[1] : 8); i++) {
            uint64_t a = v[0] + i * 8, q = cxxrt_readable(a, 8) ? r64(a) : 0;
            const char *s = cxxrt_readable(q, 64) ? rstr(q) : "";
            int text = s[0] >= 0x20 && s[1] >= 0x20 && s[2] >= 0x20;
            printf("lm3dbg: %llx: %016llx%s%.60s\n", (unsigned long long)a, (unsigned long long)q, cxxrt_readable(a, 8) ? text ? "  \"" : "" : " (not readable)", text ? s : "");
        }
    } else if (!strcmp(cmd, "call")) {
        int k = sscanf(arg, "%llx %llx %llx %llx %llx %llx %llx %llx %llx", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]);
        if (k < 1) return;
        uint64_t a[8]; for (int i = 0; i < 8; i++) a[i] = v[i + 1];
        printf("lm3dbg: x0 = %llx\n", (unsigned long long)gcall(c, v[0], a, k - 1));
    } else printf("lm3dbg: unknown command\n");
}
static void commands(Ctx *c) {
    static const char *path; static int looked;
    if (!looked) { looked = 1; path = getenv("LM3_DEBUG_CMD"); }
    FILE *f = path ? fopen(path, "r") : NULL;
    if (!f) return;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (!n) return;
    buf[n] = 0;
    if ((f = fopen(path, "w"))) fclose(f);                 /* taken */
    for (char *l = strtok(buf, "\n"); l; l = strtok(NULL, "\n")) { char one[256]; snprintf(one, sizeof one, "%s\n", l); run(c, one); }
    fflush(stdout);
}

/* ---- the panel in a level: F2, or Minus + L + R ----
 * The game's own spawners (cogSpawner, 1000 and more in the hotel) that name what they spawn, nearest to Luigi first: A
 * makes one spawn where it stands (cogSpawner::Spawn, +0x2e4980: the room's ghosts, nests, props), as the room's script
 * would. What a spawner makes has no name in the game, only a hash; the row says what came out once it has spawned.
 * While the panel is open the game reads an idle pad. */
enum { PA = 1 << 0, PB = 1 << 1, PL = 1 << 6, PR = 1 << 7, PMINUS = 1 << 11, PUP = 1 << 13, PDOWN = 1 << 15, SPAWNERS = 16 };
int nnhid_menu_key(int vk);                                /* nnhid.c */
static struct { int open, sel, n; struct { uint32_t obj, makes; float dist; const char *got; } sp[SPAWNERS]; } g_panel;
static void panel_fill(void) {
    uint32_t luigi = hash33("cogLuigi"), spawner = hash33("cogSpawner");
    const float *at = NULL;
    g_panel.n = 0;
    for (uint32_t i = 0; i < 0x20000 && !at; i++) { uint64_t o = object_at(i);      /* the first cogLuigi is Luigi (Gooigi's object comes later) */ if (o && component(o, luigi) && r64(o + 0x58)) at = (const float *)(uintptr_t)(r64(o + 0x58) + 0x30); }
    for (uint32_t i = 0; at && i < 0x20000; i++) {
        uint64_t o = object_at(i), cp = o ? component(o, spawner) : 0, m = o ? r64(o + 0x58) : 0;
        uint32_t makes = cp ? r32(cp + 0x10) != ~0u ? r32(cp + 0x10) : r32(cp + 0x14) : ~0u;
        if (makes == ~0u || !m) continue;
        const float *p = (const float *)(uintptr_t)(m + 0x30);
        float d = sqrtf((p[0] - at[0]) * (p[0] - at[0]) + (p[1] - at[1]) * (p[1] - at[1]) + (p[2] - at[2]) * (p[2] - at[2]));
        int k = g_panel.n;                                 /* the nearest SPAWNERS, in order */
        if (k == SPAWNERS) { if (d >= g_panel.sp[k - 1].dist) continue; k--; } else g_panel.n++;
        for (; k > 0 && g_panel.sp[k - 1].dist > d; k--) g_panel.sp[k] = g_panel.sp[k - 1];
        g_panel.sp[k].obj = i; g_panel.sp[k].makes = makes; g_panel.sp[k].dist = d; g_panel.sp[k].got = "";
    }
}
static void panel_show(void) {
    static char text[SPAWNERS + 1][72];
    const char *row[SPAWNERS + 1];
    snprintf(text[0], sizeof text[0], "Back to the level menu");
    for (int i = 0; i < g_panel.n; i++) snprintf(text[i + 1], sizeof text[i + 1], "%5.1f m   spawner %05x   makes %08x  %s", g_panel.sp[i].dist, g_panel.sp[i].obj, g_panel.sp[i].makes, g_panel.sp[i].got);
    for (int i = 0; i <= g_panel.n; i++) row[i] = text[i];
    pcmenu_debug_list("Debug: spawners near Luigi", row, g_panel.n + 1, g_panel.sel, "A: spawn    B / F2: close");
}
/* 1 = the game gets an idle pad */
static int panel(Ctx *c, uint64_t pad) {
    static uint64_t was = ~0ull;
    static int key_was = 1;
    int key = nnhid_menu_key(VK_F1 + 1), combo = (pad & (PMINUS | PL | PR)) == (PMINUS | PL | PR), in_level = top_state() == r64(G(0x1c69280));
    uint64_t hit = pad & ~was;
    int toggle = (key && !key_was) || (combo && (hit & (PMINUS | PL | PR)));
    was = pad; key_was = key;
    if (!g_panel.open) {
        if (!toggle || !in_level) return 0;
        g_panel.open = 1; g_panel.sel = 0;
        panel_fill(); panel_show();
        return 1;
    }
    if (toggle || (hit & PB) || !in_level) { g_panel.open = 0; pcmenu_debug_list("", NULL, -1, 0, NULL); return 1; }
    if (hit & PUP) g_panel.sel = (g_panel.sel + g_panel.n) % (g_panel.n + 1);
    if (hit & PDOWN) g_panel.sel = (g_panel.sel + 1) % (g_panel.n + 1);
    if ((hit & PA) && !g_panel.sel) { g_panel.open = 0; pcmenu_debug_list("", NULL, -1, 0, NULL); request_state(r64(G(0x1c68da0))); return 1; }      /* RestartIntoLevelMenuState */
    if (hit & PA) {
        uint64_t o = object_at(g_panel.sp[g_panel.sel - 1].obj), a[1] = {o ? component(o, hash33("cogSpawner")) : 0};
        uint64_t made = a[0] ? object_at((uint32_t)gcall(c, G(0x2e4980), a, 1) & 0x1ffff) : 0;
        g_panel.sp[g_panel.sel - 1].got = !made ? "(nothing)" : component(made, hash33("cogBoo")) ? "-> a Boo" : component(made, hash33("cogGhost")) ? "-> a ghost" : component(made, hash33("cogNest")) ? "-> a nest" : "-> an object";
        printf("lm3dbg: spawner %x %s\n", g_panel.sp[g_panel.sel - 1].obj, g_panel.sp[g_panel.sel - 1].got); fflush(stdout);
    }
    panel_show();
    return 1;
}

/* ---- the hooks (lm3_imports.c) ---- */
int lm3_debug_on(void) { static int on = -1; if (on < 0) { const char *e = getenv("LM3_DEBUG"); on = e && atoi(e); } return on; }
/* before the game starts: it comes up in its level menu, not the title */
void lm3_debug_boot(void) {
    if (!lm3_debug_on()) return;
    if (hash33("cogSpawner") != 0x9cacf4d8u) { printf("lm3dbg: self-check failed: the name hash is not the game's\n"); abort(); }      /* the constant main compares with (+0x1eafe8) */
    *(uint8_t *)(uintptr_t)G(0x1c70608) = 1;
    printf("lm3dbg: LM3_DEBUG: the game starts in its debug level menu\n");
}
/* at player 1's pad read (its buttons), once a frame or more, on the game's main thread; 1 = the game gets an idle pad */
int lm3_debug_tick(Ctx *c, uint64_t pad) {
    static unsigned t;
    if (!(t++ & 7)) commands(c);
    level_start(c);
    if (level_menu_names()) { g_panel.open = 0; return 0; }
    if (panel(c, pad)) return 1;
    if (!g_panel.open) pcmenu_debug_list("", NULL, -1, 0, NULL);
    return 0;
}
