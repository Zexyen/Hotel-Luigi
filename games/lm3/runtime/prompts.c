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
 * Keyboard: the keys bound in [keyboard] (the first of a binding), the mouse as a mouse.
 *
 * In the rooms and on dialog buttons the game shows pictures instead: small ASTC 4x4 images with mip levels, one or two
 * buttons each, a dark letter on a shaded cap (ICONS: their hashes and where the letter is). The cap stays; the letter is
 * painted over with the cap's shade and the family's symbol or name is drawn there, every level, as luminance + alpha
 * ASTC blocks (astc_la). Not replaced: SL / SR, sticks, HOME, the four-dot pictures; and the pictures of the level archives. */
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

/* ---------------------------------------------------------------- the buttons that are pictures */
#define FMT_ASTC4 0x87
typedef struct { uint64_t hash; uint8_t g, x, y, w, h, cut; } Icon;      /* the letter's box; cut: the symbol is a hole in a white disc */
static const Icon ICONS[] = {                               /* the entries of one picture follow each other */
    {0x06ffb48253c64b6eull, G_Y, 21, 19, 20, 28, 0},
    {0x110fa4f6150eeba1ull, G_ZL, 16, 23, 32, 22, 0},
    {0x29e3cfb888e1fa74ull, G_PLUS, 16, 16, 32, 32, 1},
    {0x2c9b06afafd95a0cull, G_B, 20, 17, 26, 28, 0},
    {0x3d723ea118afe615ull, G_ZR, 15, 23, 35, 23, 0},
    {0x51c7827dffdf48deull, G_L, 21, 17, 19, 29, 0},
    {0x63b1add11b8b6cecull, G_A, 17, 16, 29, 31, 0},
    {0x8d9abb1d6af23e93ull, G_X, 18, 18, 28, 27, 0},
    {0x99ed12cbe733e6f9ull, G_MINUS, 16, 16, 32, 32, 1},
    {0xa3156c54b8131a61ull, G_X, 19, 14, 27, 28, 0},
    {0xa3daadea0789d2beull, G_A, 19, 17, 25, 27, 0},
    {0xb6fba4e09c2bfbf6ull, G_A, 21, 19, 22, 25, 0},
    {0xb750fb0a60e39214ull, G_ZR, 14, 26, 35, 18, 0},
    {0xddbac903937c712dull, G_ZR, 14, 18, 35, 18, 0},
    {0xf0b15025669b8505ull, G_A, 22, 20, 19, 21, 0},
    {0x7185d291b97d43dcull, G_ZR, 27, 52, 71, 37, 0},
    {0xa15e384387c2c1bbull, G_ZR, 27, 36, 71, 37, 0},
    {0xc4c00a35bdb769cdull, G_ZL, 15, 19, 28, 17, 0},
    {0xc4c00a35bdb769cdull, G_ZR, 85, 19, 31, 17, 0},
    {0xc4c00a35bdb769cdull, G_ZL, 16, 91, 28, 16, 0},
    {0xc4c00a35bdb769cdull, G_ZR, 84, 91, 31, 16, 0},
    {0xccaee50316dd20b8ull, G_ZL, 37, 52, 64, 37, 0},
    {0xceec63d09a72089dull, G_ZL, 37, 36, 64, 37, 0},
    {0x1bb4596939ef7fc8ull, G_A, 19, 12, 25, 27, 0},
    {0x1bb4596939ef7fc8ull, G_A, 86, 28, 19, 21, 0},
    {0x69dbb3c01f7c2ff5ull, G_ZR, 14, 17, 35, 18, 0},
    {0x69dbb3c01f7c2ff5ull, G_ZR, 79, 24, 35, 18, 0},
    {0xc27171f7f4a24923ull, G_X, 19, 14, 27, 28, 0},
    {0xc27171f7f4a24923ull, G_X, 86, 31, 20, 20, 0},
    {0xc757c1e66b8f892bull, G_ZL, 17, 21, 32, 18, 0},
    {0xc757c1e66b8f892bull, G_ZR, 80, 21, 35, 18, 0},
    {0xccf9faeae1859cdaull, G_Y, 21, 13, 23, 29, 0},
    {0xccf9faeae1859cdaull, G_Y, 88, 29, 17, 21, 0},
    {0xd938fc0b76712c0eull, G_ZL, 19, 17, 32, 18, 0},
    {0xd938fc0b76712c0eull, G_ZL, 84, 25, 32, 18, 0},
    {0x17c28e08ebe051adull, G_A, 74, 46, 104, 111, 0},
    {0x76d20b5feb0ada12ull, G_A, 74, 68, 106, 113, 0},
};
#define ICON_N (sizeof ICONS / sizeof ICONS[0])
typedef struct { const Icon *first; uint8_t *mem, *orig, *shown; TexLayout L; } LiveIcon;
enum { ICON_MAX = 96 };
static LiveIcon g_icon[ICON_MAX];

/* 4x4 texels (RGBA, grey) as one ASTC block: one partition, luminance + alpha endpoints of 8 bits, 16 weights of 3 bits */
static void astc_la(const uint8_t *px, uint8_t *out) {
    int a = 0, b = 0, best = 0;
    for (int i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) {      /* the two texels farthest apart are the endpoints */
        int dl = px[i * 4] - px[j * 4], da = px[i * 4 + 3] - px[j * 4 + 3], d = dl * dl + da * da;
        if (d > best) { best = d; a = i; b = j; }
    }
    int l0 = px[a * 4], a0 = px[a * 4 + 3], dl = px[b * 4] - l0, da = px[b * 4 + 3] - a0;
    uint64_t lo = 0x53 | 4ull << 13 | (uint64_t)l0 << 17 | (uint64_t)px[b * 4] << 25 | (uint64_t)a0 << 33 | (uint64_t)px[b * 4 + 3] << 41, hi = 0;
    for (int i = 0; i < 16; i++) {
        int t = best ? ((px[i * 4] - l0) * dl + (px[i * 4 + 3] - a0) * da) * 14 / best : 0, q = t < 0 ? 0 : t > 13 ? 7 : (t + 1) / 2;
        for (int k = 0; k < 3; k++) if (q >> k & 1) hi |= 1ull << (63 - (i * 3 + k));      /* weights come from the block's end, backwards */
    }
    memcpy(out, &lo, 8); memcpy(out + 8, &hi, 8);
}
/* astc_la against the decoder the renderers use: 0 = it cannot be trusted here, and the pictures stay the game's */
static int astc_ok(void) {
    static int ok = -1;
    if (ok < 0) {
        uint8_t px[64], blk[16], back[64];
        for (int i = 0; i < 16; i++) { px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = (uint8_t)(40 + i * 12); px[i * 4 + 3] = (uint8_t)(255 - i * 9); }
        astc_la(px, blk);
        ok = tex_decode_block_rgba8(FMT_ASTC4, blk, back, 16) == 0;
        for (int i = 0; ok && i < 64; i++) ok = abs(px[i] - back[i]) <= 14;
        if (!ok) printf("button prompts: the ASTC self-check failed, the game's button pictures stay\n");
    }
    return ok;
}
/* what stands for glyph g: s = 1 everywhere, 0 where the symbol is; 0 = the game's own letter stays */
static int symbol(uint8_t *s, int g, int family, int by_letter) {
    static const uint8_t ids[] = {G_A, G_B, G_X, G_Y, G_L, G_ZL, G_ZR, G_PLUS, G_MINUS};
    static const char *const ps[] = {0, 0, 0, 0, "L1", "L2", "R2"}, *const xbox[] = {"B", "A", "Y", "X", "LB", "LT", "RT"};
    static const int kb[] = {0, 1, 2, 3, 6, 8, 9, 10, 11};
    typedef void Face(uint8_t *);
    int i = 0;
    while (i < (int)sizeof ids && ids[i] != g) i++;
    if (i == (int)sizeof ids) return 0;
    memset(s, 1, S * S);
    if (family == 4) {
        int vk = bound(kb[i]);
        char t[4];
        if (vk == 1 || vk == 2 || vk == 4) snprintf(t, sizeof t, "M%d", vk == 4 ? 3 : vk); else key_label(vk, t);
        if (!t[0]) return 0;
        text(s, t);
    } else if (i >= 7) (i == 7 ? bars_across : family == 2 ? bars_up : view)(s);
    else if (family == 2 && i < 4) ((Face *[]){by_letter ? ps_cross : ps_circle, by_letter ? ps_circle : ps_cross, by_letter ? ps_square : ps_triangle, by_letter ? ps_triangle : ps_square})[i](s);
    else if (family == 3 && i < 4 && by_letter) return 0;
    else text(s, family == 2 ? ps[i] : xbox[i]);
    return 1;
}
/* the symbol in the letter's box of an image w wide (was: as the game made it): the cap's shade row by row, the letter's
 * darkness for the symbol; the box grows by 2 texels to take the letter's soft edge along */
static void stamp(uint8_t *rgba, const uint8_t *was, int w, int h, const Icon *ic, const uint8_t *s, int *box) {
    int sx0 = S, sx1 = -1, sy0 = S, sy1 = -1;
    for (int j = 0; j < S; j++) for (int i = 0; i < S; i++) if (!s[j * S + i]) { sx0 = i < sx0 ? i : sx0; sx1 = i > sx1 ? i : sx1; sy0 = j < sy0 ? j : sy0; sy1 = j > sy1 ? j : sy1; }
    if (sx1 < 0) return;
    int side = ic->w > ic->h ? ic->w : ic->h;                /* a name of two letters may be wider than the game's one */
    float k = (float)side * SS / (sx1 - sx0 + 1), ky = (float)side * SS / (sy1 - sy0 + 1);      /* texels a unit of the art */
    k = k < ky ? k : ky;
    float cx = ic->x + ic->w / 2.f, cy = ic->y + ic->h / 2.f, mx = (sx0 + sx1 + 1) / (2.f * SS), my = (sy0 + sy1 + 1) / (2.f * SS);
    int gw = (int)(k * (sx1 - sx0 + 1) / SS) + 1, gh = (int)(k * (sy1 - sy0 + 1) / SS) + 1;
    gw = gw > ic->w ? gw : ic->w; gh = gh > ic->h ? gh : ic->h;
    int x0 = (int)(cx - gw / 2.f) - 2, y0 = (int)(cy - gh / 2.f) - 2, x1 = x0 + gw + 5, y1 = y0 + gh + 5;
    x0 = x0 < 0 ? 0 : x0; y0 = y0 < 0 ? 0 : y0; x1 = x1 > w ? w : x1; y1 = y1 > h ? h : y1;
    box[0] = x0 < box[0] ? x0 : box[0]; box[1] = y0 < box[1] ? y0 : box[1]; box[2] = x1 > box[2] ? x1 : box[2]; box[3] = y1 > box[3] ? y1 : box[3];
    long frame = 0, fn = 0;
    int lo = 255, hi = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        int v = was[((size_t)y * w + x) * 4];
        if (x < ic->x || x >= ic->x + ic->w || y < ic->y || y >= ic->y + ic->h) { frame += v; fn++; }
        lo = v < lo ? v : lo; hi = v > hi ? v : hi;
    }
    int light = !fn || frame / fn > 127, inkv = light ? lo : hi;      /* a dark letter on a light cap, or the other way round */
    for (int y = y0; y < y1; y++) {
        int face = light ? 0 : 255;
        for (int x = x0 > 2 ? x0 - 2 : 0; x < (x1 + 2 < w ? x1 + 2 : w); x++) { int v = was[((size_t)y * w + x) * 4]; face = light ? (v > face ? v : face) : (v < face ? v : face); }
        for (int x = x0; x < x1; x++) {
            int i0 = (int)floorf(((x - cx) / k + mx) * SS + .5f), i1 = (int)floorf(((x + 1 - cx) / k + mx) * SS + .5f);
            int j0 = (int)floorf(((y - cy) / k + my) * SS + .5f), j1 = (int)floorf(((y + 1 - cy) / k + my) * SS + .5f), n = 0;
            if (i1 <= i0) i1 = i0 + 1;
            if (j1 <= j0) j1 = j0 + 1;
            for (int j = j0; j < j1; j++) for (int i = i0; i < i1; i++) n += i >= 0 && i < S && j >= 0 && j < S && !s[j * S + i];
            int c = n * 255 / ((i1 - i0) * (j1 - j0));
            uint8_t *px = rgba + ((size_t)y * w + x) * 4;
            if (ic->cut) { px[0] = px[1] = px[2] = 255; px[3] = (uint8_t)(255 - c); }
            else px[0] = px[1] = px[2] = (uint8_t)(face + (inkv - face) * c / 255);
        }
    }
}
/* every live picture from the game's own blocks, like paint() */
static void paint_icons(int family, int by_letter) {
    uint8_t *s = family > 1 && astc_ok() ? malloc(S * S) : NULL;
    for (LiveIcon *l = g_icon; l < g_icon + ICON_MAX; l++) {
        if (!l->mem) continue;
        const TexLayout *L = &l->L;
        int w = L->lvl_w[0], h = L->lvl_h[0], box[4] = {w, h, 0, 0};
        size_t n = (size_t)w * h * 4;
        uint8_t *rgba = s ? malloc(n * 2 + L->lvl_linear_size[0]) : NULL;
        memcpy(l->shown, l->orig, L->layer_size);
        if (rgba) {
            uint8_t *was = rgba + n, *lin = was + n;
            tex_deswizzle_level(L, 0, l->orig, lin);
            tex_decode_rgba8(FMT_ASTC4, lin, w, h, was);
            memcpy(rgba, was, n);
            for (const Icon *ic = l->first; ic < ICONS + ICON_N && ic->hash == l->first->hash; ic++) if (symbol(s, ic->g, family, by_letter))
                stamp(rgba, was, w, h, ic, s, box);
            int x0 = box[0], y0 = box[1], x1 = box[2], y1 = box[3];
            for (int lv = 0; x1 > x0 && lv < L->levels; lv++) {      /* the blocks under the letters, at every level; rgba halves in place */
                int lw = L->lvl_w[lv], lh = L->lvl_h[lv], wb = L->lvl_wb[lv], hb = L->lvl_hb[lv];
                if (lv) for (int y = 0; y < lh; y++) for (int x = 0; x < lw; x++) for (int c = 0; c < 4; c++) {
                    int pw = L->lvl_w[lv - 1], ph = L->lvl_h[lv - 1], xa = x * 2, xb = xa + 1 < pw ? xa + 1 : xa, ya = y * 2, yb = ya + 1 < ph ? ya + 1 : ya;
                    rgba[((size_t)y * lw + x) * 4 + c] = (uint8_t)((rgba[((size_t)ya * pw + xa) * 4 + c] + rgba[((size_t)ya * pw + xb) * 4 + c] + rgba[((size_t)yb * pw + xa) * 4 + c] + rgba[((size_t)yb * pw + xb) * 4 + c] + 2) / 4);
                }
                tex_deswizzle_level(L, lv, l->orig + L->lvl_offset[lv], lin);
                int bx0 = x0 >> lv >> 2, by0 = y0 >> lv >> 2, bx1 = ((x1 - 1) >> lv >> 2) + 1, by1 = ((y1 - 1) >> lv >> 2) + 1;
                for (int by = by0; by < by1 && by < hb; by++) for (int bx = bx0; bx < bx1 && bx < wb; bx++) {
                    uint8_t px[64];
                    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
                        int tx = bx * 4 + x < lw ? bx * 4 + x : lw - 1, ty = by * 4 + y < lh ? by * 4 + y : lh - 1;
                        memcpy(px + (y * 4 + x) * 4, rgba + ((size_t)ty * lw + tx) * 4, 4);
                    }
                    astc_la(px, lin + ((size_t)by * wb + bx) * 16);
                }
                tex_swizzle_level(L, lv, lin, l->shown + L->lvl_offset[lv]);
            }
            free(rgba);
        }
        memcpy(l->mem, l->shown, L->layer_size);
        nvn_gl_memory_written((uint64_t)(uintptr_t)l->mem, L->layer_size);
    }
    free(s);
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
    for (LiveIcon *l = g_icon; l < g_icon + ICON_MAX; l++) {
        if (l->mem && memcmp(l->mem, l->shown, l->L.layer_size)) { free(l->orig); *l = (LiveIcon){0}; }
        any |= l->mem != NULL;
    }
    if (any && (g_dirty || family != g_family || sig != g_sig)) {
        int said = family != g_family || sig != g_sig;
        paint(family, by_letter);
        paint_icons(family, by_letter);
        g_family = family; g_sig = sig; g_dirty = 0;
        if (said) printf("button prompts: %s%s\n", family == 2 ? "PlayStation" : family == 3 ? "Xbox" : family == 4 ? "keyboard" : "Switch", by_letter ? ", face buttons by printed letter" : "");
    }
    ReleaseSRWLockExclusive(&g_lock);
}

/* a picture of ICONS? */
static void icon_texture(uint8_t *mem, const uint8_t *limit, const TexLayout *L) {
    if (L->width > 256 || L->height > 256 || L->layers != 1 || L->linear || L->levels > TEX_MAX_LEVELS || (limit && mem + L->layer_size > limit)) return;
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < L->lvl_size[0]; i++) h = (h ^ mem[i]) * 0x100000001b3ull;
    for (const Icon *ic = ICONS; ic < ICONS + ICON_N; ic++) if (ic->hash == h && ic->x + ic->w <= L->width && ic->y + ic->h <= L->height) {
        AcquireSRWLockExclusive(&g_lock);
        LiveIcon *l = g_icon;
        while (l < g_icon + ICON_MAX && l->mem) l++;
        uint8_t *copy = l < g_icon + ICON_MAX ? malloc(L->layer_size * 2) : NULL;
        if (copy) {
            *l = (LiveIcon){.first = ic, .mem = mem, .orig = copy, .shown = copy + L->layer_size, .L = *L};
            memcpy(l->orig, mem, L->layer_size); memcpy(l->shown, mem, L->layer_size);
            g_dirty = 1;
        }
        ReleaseSRWLockExclusive(&g_lock);
        prompts_tick();
        return;
    }
}
/* nvn_gl.c tex_initialize: is this a font page with prompts, or a picture of a button? */
void prompts_texture(uint8_t *mem, const uint8_t *limit, const TexLayout *L) {
    int format = L->format, width = L->width, height = L->height;
    if (format == FMT_ASTC4 && mem) { icon_texture(mem, limit, L); return; }
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
    for (LiveIcon *l = g_icon; l < g_icon + ICON_MAX; l++) if (l->mem == mem) { free(l->orig); *l = (LiveIcon){0}; }
    ReleaseSRWLockExclusive(&g_lock);
}
