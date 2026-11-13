/* PlayStation / Xbox / keyboard button prompts for Luigi's Mansion 3, no mod needed. The game's prompts are glyphs of its
 * fonts, at Nintendo's private-use code points (SEQ below): white, the shape in alpha, on BC3 pages of 512 or 1024 texels.
 * A font file holds no positions: the game packs the glyphs of the font's description in order, left to right, in rows of
 * its RenderHeight. So a page is known by its hash and by where its first prompt sits (PAGES, the 13 font sets of the one
 * supported dump). When the game makes such a texture (nvn_gl.c tex_initialize) the prompts are painted over in its
 * memory, and again whenever the choice changes: the renderers see an ordinary texture the game has written to. A page
 * that is not in PAGES (a mod's font) stays as it is.
 * Which family: controllers.ini button_prompts (nnpad_button_prompts). The pictures are drawn from plain shapes
 * (prompt_art.h), as large as the game's own; there is no artwork in the repository.
 *
 * Face buttons: by position (default layout) Switch A, the right button, is Circle / Xbox B, and so on; Xbox takes the
 * game's own letters, swapped. By printed letter (nnpad_prompts_by_letter) A is the bottom button: the letters are right,
 * but the pictures that show WHERE a button sits (four dots) swap the same way.
 * Keyboard: the keys bound in [keyboard] (the first of a binding), the mouse as a mouse. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "nnpad.h"
#define SS 12                                               /* the game's prompts are 68 texels across: 3.7 subsamples a texel */
#include "prompt_art.h"
#include "tex_decode.h"

int nnpad_mouse_cfg(float *sens, int *invert);              /* nnpad_sdl.c */
void nvn_gl_memory_written(uint64_t addr, uint64_t size);
void prompts_tick(void);

#define FMT_BC3 0x44
enum { DOTS = 0, DOT_R, DOT_B, DOT_T, DOT_L, G_A, G_B, G_X, G_Y, G_L, G_R, G_ZL, G_ZR, G_SL, G_SR, G_DPAD, G_UP, G_DOWN, G_LEFT, G_RIGHT,
       G_PLUS, G_MINUS, G_PLUS2, G_MINUS2, G_HOME, G_GOLD, G_STICK, G_STICK_L, G_STICK_R, G_PRESS, G_PRESS_L, G_PRESS_R, G_STICK_UD, G_STICK2, NSEQ };
static const uint16_t SEQ[NSEQ] = {0xe0aa, 0xe0ab, 0xe0ac, 0xe0ad, 0xe0ae, 0xe0e0, 0xe0e1, 0xe0e2, 0xe0e3, 0xe0e4, 0xe0e5, 0xe0e6, 0xe0e7, 0xe0e8, 0xe0e9, 0xe0ea, 0xe0eb,
    0xe0ec, 0xe0ed, 0xe0ee, 0xe0ef, 0xe0f0, 0xe0f1, 0xe0f2, 0xe0f4, 0xe0fc, 0xe100, 0xe101, 0xe102, 0xe103, 0xe104, 0xe105, 0xe115, 0xe116};
/* first: the page's first prompt (an index of SEQ) at x, y; gold: the font has U+E0FC; narrow: its U+E101..E105 are 66 wide,
 * not 72. A font's second page follows its first. */
typedef struct { uint64_t hash; int size, rh, first, x, y, gold, narrow; } Page;
static const Page PAGES[] = {
    {0x7a073b6d4df1c007ull,  512, 97,  0, 137,  97, 0, 1},   /* AlorNormal48, all 13 font sets */
    {0x3dece9435b776130ull,  512, 97, 27,   0,   0, 0, 1},
    {0xd9df57957dffa3a3ull,  512, 88,  0, 124,  88, 1, 1},   /* hanzelbold48, all 13 */
    {0x4e5dc2829b64690full,  512, 88, 26,   0,   0, 1, 1},
    {0x15f2bd7ee70d3563ull,  512, 76,  0, 197, 228, 1, 1},   /* hanzelnormal48, 9 sets */
    {0x89fc14d0832f2d46ull,  512, 76, 18,   0,   0, 1, 1},
    {0x71ebb44794ba2351ull,  512, 65,  0, 253, 325, 0, 1},   /* DFPPOPMix-W448 */
    {0xa2f6973d790256f4ull,  512, 65, 10,   0,   0, 0, 1},
    {0xafd400962c196f34ull,  512, 67,  0,  65, 335, 1, 1},   /* dfpsogei-w748 */
    {0x93de23f7e3906256ull,  512, 67, 13,   0,   0, 1, 1},
    {0x66b4cff4d264e07bull,  512, 65,  0, 327, 325, 1, 1},   /* dfpsogei-w948 */
    {0x2bf11810bbbd6be9ull,  512, 65,  9,   0,   0, 1, 1},
    {0xc9a3fc578e719bd0ull,  512, 65,  0,   0, 195, 1, 0},   /* KoreanNGLR248 */
    {0xd551179cf49bd0a0ull,  512, 65, 28,   0,   0, 1, 0},
    {0x8a1570f125f49115ull,  512, 65,  0, 155, 130, 1, 0},   /* KoreanUJSNR48 */
    {0xf024c0b2c2da9facull,  512, 65, 32,   0,   0, 1, 0},
    {0x775cdd9bd67b4843ull, 1024, 77,  0, 707, 924, 1, 0},   /* DFHuaYiW4B548 */
    {0xab6675d9282ec940ull, 1024, 77,  4,   0,   0, 1, 0},
    {0xef212ac5bfe9d3dbull, 1024, 77,  0, 650, 308, 1, 0},   /* DFPZongYiW7-GB48 */
    {0x8ae9eb65fd5372e1ull, 1024, 77,  0,   0, 308, 1, 0},   /* DFYuanGB-W548 */
    {0x535a1dfe43227d02ull, 1024, 76,  0, 651, 304, 1, 0},   /* DFZongYiW5B548 */
};
static const Page *font_of(const Page *p) { return p->first ? p - 1 : p; }

typedef struct { int x, y, w; } Cell;                       /* w 0: not on this page */
typedef struct {
    const Page *p;
    uint8_t *mem, *orig, *shown;                            /* the texture's memory; the game's own blocks; what we left there */
    uint8_t *rgba, *was, *touched;                          /* while painting: the page, the game's page, the blocks to encode */
    Cell cell[NSEQ];
} Live;
enum { LIVE_MAX = 16 };
static Live g_live[LIVE_MAX];
static SRWLOCK g_lock = SRWLOCK_INIT;
static int g_family = 1, g_dirty;                           /* dirty: a page came since the last paint */
static uint64_t g_sig = 1;                                 /* prompts_tick's, of the game's own */

static size_t bytes_of(const Page *p) { return (size_t)p->size * p->size; }      /* BC3: a byte a texel */
static void cells_of(Live *l) {
    const Page *p = l->p;
    int x = p->x, y = p->y;
    for (int i = p->first; i < NSEQ; i++) {
        if (i == G_GOLD && !p->gold) continue;
        int w = p->narrow && i >= G_STICK_L && i <= G_PRESS_R ? 66 : 72;
        if (x + w > p->size) { x = 0; y += p->rh; if (y + p->rh > p->size) break; }
        l->cell[i] = (Cell){x, y, w};
        x += w;
    }
}
/* the live page of the same font that holds this glyph */
static Live *page_with(const Live *of, int g) {
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) if (l->mem && l->rgba && font_of(l->p) == font_of(of->p) && l->cell[g].w) return l;
    return NULL;
}
static void touch(Live *l, Cell c) {
    int n = l->p->size / 4;
    for (int by = c.y / 4; by <= (c.y + l->p->rh - 1) / 4; by++) for (int bx = c.x / 4; bx <= (c.x + c.w - 1) / 4; bx++) l->touched[by * n + bx] = 1;
}
/* where the game's glyph has ink: 0 = nowhere */
static int ink(const Live *l, int g, float *cx, float *cy, float *d) {
    Cell c = l->cell[g];
    int x0 = c.w, x1 = -1, y0 = l->p->rh, y1 = -1;
    for (int y = 0; y < l->p->rh; y++) for (int x = 0; x < c.w; x++) if (l->was[((size_t)(c.y + y) * l->p->size + c.x + x) * 4 + 3] > 127) {
        x0 = x < x0 ? x : x0; x1 = x > x1 ? x : x1; y0 = y < y0 ? y : y0; y1 = y > y1 ? y : y1;
    }
    if (x1 < 0) return 0;
    *cx = (x0 + x1 + 1) / 2.f; *cy = (y0 + y1 + 1) / 2.f;
    *d = (float)(x1 - x0 > y1 - y0 ? x1 - x0 + 1 : y1 - y0 + 1);
    return 1;
}
/* s, a picture of prompt_art.h, in place of glyph g on every page that has it: its disc as large as the game's A, on the
 * line of A (a font's stick glyphs are small and high), as far as the cell lets it */
static void draw(int g, const uint8_t *s) {
    int sx0 = S, sx1 = -1, sy0 = S, sy1 = -1;
    for (int j = 0; j < S; j++) for (int i = 0; i < S; i++) if (s[j * S + i]) { sx0 = i < sx0 ? i : sx0; sx1 = i > sx1 ? i : sx1; sy0 = j < sy0 ? j : sy0; sy1 = j > sy1 ? j : sy1; }
    if (sx1 < 0) return;
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) {
        Cell c = l->cell[g];
        float cx, cy, d, ax, ay, ad;
        if (!l->mem || !l->rgba || !c.w || !ink(l, g, &cx, &cy, &d)) continue;
        Live *a = page_with(l, G_A);
        if (a && ink(a, G_A, &ax, &ay, &ad)) { cy = ay; d = ad; }
        float k = d / 21, half_w = (sx1 - sx0 + 1) / (2.f * SS), half_h = (sy1 - sy0 + 1) / (2.f * SS), rh = (float)l->p->rh;      /* texels a unit of the art: its disc is 21 */
        if (k * half_w * 2 > c.w - 6) k = (c.w - 6) / (half_w * 2);
        if (k * half_h * 2 > rh - 2) k = (rh - 2) / (half_h * 2);
        float mx = (sx0 + sx1 + 1) / (2.f * SS), my = (sy0 + sy1 + 1) / (2.f * SS);      /* the picture's middle goes to cx, cy, inside the cell */
        cx = cx < k * half_w + 1 ? k * half_w + 1 : cx > c.w - 1 - k * half_w ? c.w - 1 - k * half_w : cx;
        cy = cy < k * half_h + 1 ? k * half_h + 1 : cy > rh - 1 - k * half_h ? rh - 1 - k * half_h : cy;
        for (int y = 0; y < l->p->rh; y++) for (int x = 0; x < c.w; x++) {
            int i0 = (int)floorf(((x - cx) / k + mx) * SS + .5f), i1 = (int)floorf(((x + 1 - cx) / k + mx) * SS + .5f);
            int j0 = (int)floorf(((y - cy) / k + my) * SS + .5f), j1 = (int)floorf(((y + 1 - cy) / k + my) * SS + .5f), n = 0;
            if (i1 <= i0) i1 = i0 + 1;
            if (j1 <= j0) j1 = j0 + 1;
            for (int j = j0; j < j1; j++) for (int i = i0; i < i1; i++) n += i >= 0 && i < S && j >= 0 && j < S && s[j * S + i];
            uint8_t *px = l->rgba + ((size_t)(c.y + y) * l->p->size + c.x + x) * 4;
            px[0] = px[1] = px[2] = 255; px[3] = (uint8_t)(n * 255 / ((i1 - i0) * (j1 - j0)));
        }
        touch(l, c);
    }
}
/* the game's own glyph `from` in place of glyph g */
static void copy(int g, int from) {
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) {
        Cell c = l->cell[g];
        Live *f = l->mem && l->rgba && c.w ? page_with(l, from) : NULL;
        if (!f || f->cell[from].w != c.w) continue;
        for (int y = 0; y < l->p->rh; y++)
            memcpy(l->rgba + ((size_t)(c.y + y) * l->p->size + c.x) * 4, f->was + ((size_t)(f->cell[from].y + y) * f->p->size + f->cell[from].x) * 4, (size_t)c.w * 4);
        touch(l, c);
    }
}

/* the name of a virtual key on a keycap, as in [keyboard]; 0 (unbound) = "" */
static void key_label(int vk, char *out) {
    static const struct { int vk; const char *s; } name[] = {{0x05, "M4"}, {0x06, "M5"}, {0x08, "BKS"}, {0x09, "TAB"}, {0x0d, "ENT"}, {0x10, "SFT"},
        {0x11, "CTL"}, {0x12, "ALT"}, {0x1b, "ESC"}, {0x20, "SPC"}, {0x25, "LFT"}, {0x26, "UP"}, {0x27, "RGT"}, {0x28, "DWN"}, {0xa0, "SFT"},
        {0xa1, "SFT"}, {0xa2, "CTL"}, {0xa3, "CTL"}, {0xa4, "ALT"}, {0xa5, "ALT"}};
    out[0] = 0;
    if (!vk) return;
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) { out[0] = (char)vk; out[1] = 0; return; }
    if (vk >= 0x60 && vk <= 0x69) { snprintf(out, 4, "N%d", vk - 0x60); return; }
    if (vk >= 0x70 && vk <= 0x7b) { snprintf(out, 4, "F%d", vk - 0x6f); return; }
    for (size_t i = 0; i < sizeof name / sizeof name[0]; i++) if (name[i].vk == vk) { strcpy(out, name[i].s); return; }
    snprintf(out, 4, "%02X", vk & 0xff);
}
static int bound(int idx) { int v = nnpad_kb_vk(idx); return v & 0xff ? v & 0xff : v >> 8 & 0xff; }
static void key(uint8_t *s, int idx) {
    int vk = bound(idx);
    char t[4];
    if (vk == 1 || vk == 2) { mouse(s, vk); return; }
    if (vk == 4) { mouse(s, 3); return; }
    key_label(vk, t);
    keycap(s, t);
}
static void keys(uint8_t *s, int first) {                   /* the first letters of four keys, up left down right: "WASD" */
    char t[5] = "", l[4];
    for (int i = 0; i < 4; i++) { key_label(bound(first + (int[]){1, 0, 3, 2}[i]), l); t[i] = l[0] ? l[0] : ' '; }
    keycap(s, t);
}

/* every live page from the game's own blocks: family 2 playstation, 3 xbox, 4 keyboard; else as the game made it */
static void paint(int family, int by_letter) {
    typedef void Face(uint8_t *);
    uint8_t *s = malloc(S * S);
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) {
        if (!l->mem) continue;
        size_t n = bytes_of(l->p);
        int blocks = l->p->size / 4;
        memcpy(l->shown, l->orig, n);
        l->rgba = family > 1 ? malloc(n * 4 * 2 + n / 16) : NULL;
        if (!l->rgba) continue;
        l->was = l->rgba + n * 4; l->touched = l->was + n * 4;
        tex_bl_to_linear(l->rgba, l->orig, blocks * 16, blocks, 1, tex_rule_block_height_log2(blocks), 0);      /* its blocks in rows, for the decode */
        tex_decode_rgba8(FMT_BC3, l->rgba, l->p->size, l->p->size, l->was);
        memcpy(l->rgba, l->was, n * 4);
        memset(l->touched, 0, n / 16);
    }
    #define PIC(g, picture) (memset(s, 0, S * S), picture, draw(g, s))
    if (!s) family = 1;
    if (family == 4) {
        static const int pad[][2] = {{G_A, 0}, {G_B, 1}, {G_X, 2}, {G_Y, 3}, {DOT_R, 0}, {DOT_B, 1}, {DOT_T, 2}, {DOT_L, 3}, {G_PRESS_L, 4}, {G_PRESS_R, 5}, {G_L, 6}, {G_R, 7},
            {G_ZL, 8}, {G_ZR, 9}, {G_PLUS, 10}, {G_PLUS2, 10}, {G_MINUS, 11}, {G_MINUS2, 11}, {G_LEFT, 12}, {G_UP, 13}, {G_RIGHT, 14}, {G_DOWN, 15}};
        for (size_t i = 0; i < sizeof pad / sizeof pad[0]; i++) PIC(pad[i][0], key(s, pad[i][1]));
        PIC(G_DPAD, keys(s, 12));
        PIC(G_STICK_L, keys(s, 16));
        PIC(G_STICK_R, nnpad_mouse_cfg(NULL, NULL) ? mouse(s, 0) : keys(s, 20));
    } else if (family == 2) {
        Face *a = by_letter ? ps_cross : ps_circle, *b = by_letter ? ps_circle : ps_cross;
        Face *x = by_letter ? ps_square : ps_triangle, *y = by_letter ? ps_triangle : ps_square;
        PIC(G_A, a(s)); PIC(G_B, b(s)); PIC(G_X, x(s)); PIC(G_Y, y(s));
        PIC(G_PLUS, bars_across(s)); PIC(G_PLUS2, bars_across(s)); PIC(G_MINUS, bars_up(s)); PIC(G_MINUS2, bars_up(s));
        PIC(G_L, bumper(s, "L1")); PIC(G_R, bumper(s, "R1")); PIC(G_ZL, trigger(s, "L2")); PIC(G_ZR, trigger(s, "R2"));
    } else if (family == 3) {
        if (!by_letter) { copy(G_A, G_B); copy(G_B, G_A); copy(G_X, G_Y); copy(G_Y, G_X); }      /* the game's letters are already right */
        PIC(G_PLUS, bars_across(s)); PIC(G_PLUS2, bars_across(s)); PIC(G_MINUS, view(s)); PIC(G_MINUS2, view(s));
        PIC(G_L, bumper(s, "LB")); PIC(G_R, bumper(s, "RB")); PIC(G_ZL, trigger(s, "LT")); PIC(G_ZR, trigger(s, "RT"));
    }
    if (by_letter && (family == 2 || family == 3)) { copy(DOT_R, DOT_B); copy(DOT_B, DOT_R); copy(DOT_T, DOT_L); copy(DOT_L, DOT_T); }
    free(s);
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) {
        if (!l->mem) continue;
        size_t n = bytes_of(l->p);
        int size = l->p->size, blocks = size / 4, bh = tex_rule_block_height_log2(blocks);
        for (int by = 0; l->rgba && by < blocks; by++) for (int bx = 0; bx < blocks; bx++) if (l->touched[by * blocks + bx]) {
            uint8_t px[64];
            for (int y = 0; y < 4; y++) memcpy(px + y * 16, l->rgba + ((size_t)(by * 4 + y) * size + bx * 4) * 4, 16);
            tex_encode_bc3(px, 4, 4, l->shown + tex_bl_offset(bx * 16, by, 0, blocks * 16, blocks, bh, 0));
        }
        free(l->rgba); l->rgba = NULL;
        memcpy(l->mem, l->shown, n);                /* the renderers are told after the copy is complete, so they cannot keep half of it */
        nvn_gl_memory_written((uint64_t)(uintptr_t)l->mem, n);
    }
}

/* From player 1's pad poll: repaint when the device in use, the bindings or the settings change, or a page came. */
void prompts_tick(void) {
    int family = nnpad_button_prompts(), by_letter = family > 1 && family < 4 && nnpad_prompts_by_letter(), any = 0;
    uint64_t sig = (uint64_t)by_letter + 1;
    if (family == 4) { for (int i = 0; i < KB_N; i++) sig = sig * 31 + (uint64_t)nnpad_kb_vk(i); sig = sig * 2 + (uint64_t)nnpad_mouse_cfg(NULL, NULL); }
    AcquireSRWLockExclusive(&g_lock);
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) {
        if (l->mem && memcmp(l->mem, l->shown, bytes_of(l->p))) { free(l->orig); *l = (Live){0}; }      /* not what we left there: the game has reused the memory */
        any |= l->mem != NULL;
    }
    if (any && (g_dirty || family != g_family || sig != g_sig)) {
        int said = family != g_family || sig != g_sig;
        paint(family, by_letter);
        g_family = family; g_sig = sig; g_dirty = 0;
        if (said) printf("button prompts: %s%s\n", family == 2 ? "PlayStation" : family == 3 ? "Xbox" : family == 4 ? "keyboard" : "Switch", by_letter ? ", face buttons by printed letter" : "");
    }
    ReleaseSRWLockExclusive(&g_lock);
}

/* nvn_gl.c tex_initialize: is this a font page with prompts? */
void prompts_texture(uint8_t *mem, const uint8_t *limit, int format, int width, int height) {
    if (format != FMT_BC3 || width != height || (width != 512 && width != 1024) || !mem) return;
    size_t n = (size_t)width * width;
    if (limit && mem + n > limit) return;
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ mem[i]) * 0x100000001b3ull;
    for (const Page *p = PAGES; p < PAGES + sizeof PAGES / sizeof PAGES[0]; p++) if (p->hash == h && p->size == width) {
        AcquireSRWLockExclusive(&g_lock);
        Live *l = g_live;
        while (l < g_live + LIVE_MAX && l->mem) l++;
        uint8_t *copy = l < g_live + LIVE_MAX ? malloc(n * 2) : NULL;
        if (copy) {
            *l = (Live){.p = p, .mem = mem, .orig = copy, .shown = copy + n};
            memcpy(l->orig, mem, n); memcpy(l->shown, mem, n);
            cells_of(l);
            g_dirty = 1;
        }
        ReleaseSRWLockExclusive(&g_lock);
        prompts_tick();
        return;
    }
}
/* nvn_gl.c tex_finalize */
void prompts_texture_gone(const uint8_t *mem) {
    AcquireSRWLockExclusive(&g_lock);
    for (Live *l = g_live; l < g_live + LIVE_MAX; l++) if (l->mem == mem) { free(l->orig); *l = (Live){0}; }
    ReleaseSRWLockExclusive(&g_lock);
}
