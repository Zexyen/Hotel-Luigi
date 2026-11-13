/* [witchtime] PC settings menu: a panel drawn by the runtime over the game, for the keys of graphics.ini.
 *
 * Open: F1 on the keyboard, or Start + Select (Plus + Minus) on player 1's controller; seen at the game's own pad reads
 * (pcmenu_gate, nnhid.c), so nothing runs while it is closed but one key and one mask test per read. While it is open:
 *   - the game is held: every guest thread stops at its next SDK call (hostrt.c g_guest_frozen, the console's suspension);
 *     its sound runs dry; the game's pad reads (none while held) get an idle pad, and after the menu
 *     closes too, until the buttons that closed it are let go;
 *   - a thread of its own reads player 1 (nnhid_menu_read: test scripts, the keyboard player's keys, the first controller),
 *     the arrow keys / Enter / Escape / F1 and the mouse, and posts a job to the render thread about 60 times a second that
 *     presents the game's last picture again with the panel over it (nvn_vk_pcmenu.c).
 * Controls: up / down choose a row, left / right (or A / Enter, B / Escape closes) change it; the mouse: point at a row,
 * click on the value (on '‹' for the one before). A change is written to graphics.ini at once (gfx_option_set: comments and
 * other keys kept) and applied while the game runs where the runtime can: cutscene_fps (the game library's switch), vsync
 * (present mode and pacing), the window (Linux), model detail, film grain, aspect,
 * resolution_scale (when the panel closes, so that going past 3.0 does not make its images; from the game's Options rows at
 * once: nvn_vk_draw.c rs_apply). The rest (frame_rate) is marked "applies at the next start". A key set by an environment
 * variable is shown and locked.
 * Rows with `optional` appear only when graphics.ini has their key (for keys of later builds; a frame-rate cap: one more row,
 * and its live field in gfx_option_set). Anisotropic filtering and the mip bias are fixed (16, -1; gfx_options.c) and have no rows since 2026-11-10.
 * The panel: drawn on the CPU (BGRA) when something changes, text with stb_truetype (third_party/, public domain / MIT) in
 * subsets of Russo One and M PLUS Rounded 1c (SIL OFL 1.1, third_party/OFL-*.txt), embedded with #embed; opaque, the
 * whole window: the launcher's look (launcher/ui/launcher.rcss). The same drawing makes the first-start screen
 * (draw_prep) for nvn_vk_draw.c.
 * Some rows are not graphics.ini's: button_prompts, swap_confirm and layout, of controllers.ini.
 * Test aids: WT_PCMENU_SHOT=<folder> (nvn_vk_pcmenu.c), WT_PCMENU_OPEN=<seconds> opens it that long after the first pad
 * read (for scripted runs without keys), WT_PCMENU_LOG=1 logs each input step. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "gfx_options.h"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

static const unsigned char k_font_head[] = {               /* Russo One (SIL OFL 1.1, third_party/OFL-RussoOne.txt): names, values, hints */
#embed "third_party/RussoOne-pcmenu.ttf"
};
static const unsigned char k_font_ui[] = {                 /* M PLUS Rounded 1c ExtraBold (SIL OFL 1.1, third_party/OFL-MPLUSRounded1c.txt): sentences */
#embed "third_party/MPLUSRounded1c-pcmenu.ttf"
};

extern volatile LONG g_guest_frozen;                       /* hostrt.c */
extern volatile LONG g_display_close;                      /* display.c: the window is being closed (the game must run to exit) */
void nnhid_menu_read(uint64_t *buttons, int *lx, int *ly, int swap_ab);       /* nnhid.c */
int nnhid_menu_key(int vk);
uint32_t nnhid_menu_pad(void);
int nvn_front_post_pcmenu(void);                           /* nvn_gl.c: 0 = not the Vulkan renderer */
int nvn_vk_hdr_on(void);                                   /* nvn_vk.c: the picture goes out as HDR now */
void display_vsync_apply(void);                            /* display.c */
int display_window_apply(void);
void display_mouse_want(int on);
int nnpad_mouse_cfg(float *sens, int *invert);             /* nnpad_sdl.c */
const char *nnpad_prompts_setting(void);
int nnpad_prompts_set(const char *val);
const char *nnpad_menu_swap_setting(void);
int nnpad_menu_swap_set(const char *val);
int nnpad_menu_swapped(void);
const char *nnpad_layout_setting(void);
int nnpad_layout_set(const char *val);
int nnpad_button_prompts(void);
int nnpad_prompts_by_letter(void);

/* A_PROMPTS, A_MENU_SWAP, A_LAYOUT: keys of controllers.ini. A game adds its own settings as rows of k_rows */
enum { A_LIVE, A_VSYNC, A_WINDOW, A_SIZE, A_SCALE, A_RESTART, A_PROMPTS, A_MENU_SWAP, A_LAYOUT };
#define ROW_VALUES 12
typedef struct {
    const char *key, *label, *help, *env, *dflt;           /* dflt: the reader's value when the key is not in the file */
    int apply, optional;
    const char *val[ROW_VALUES], *name[ROW_VALUES];
} Row;
static const Row k_rows[] = {
    {"window", "Display mode", "Windowed: a window of the size below. Borderless and fullscreen cover the monitor the window is on.", NULL, "windowed", A_WINDOW, 0,
     {"windowed", "borderless", "fullscreen"}, {"Windowed", "Borderless", "Fullscreen"}},
    {"size", "Window size", "The window's size in windowed mode. The game is drawn at the internal resolution and shown at this size.", "WT_WINDOW", NULL, A_SIZE, 0,
     {"1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"}, {"1280 \xC3\x97 720", "1600 \xC3\x97 900", "1920 \xC3\x97 1080", "2560 \xC3\x97 1440", "3840 \xC3\x97 2160"}},
    {"vsync", "Vertical sync", "Auto: locked to a 60 / 120 / 240 Hz display, else the program's own 60 Hz clock. Off: no waiting, tearing allowed. On: the swapchain's FIFO.", "WT_VK_PRESENT", "auto", A_VSYNC, 0,
     {"auto", "display", "clock", "off", "on", "mailbox"}, {"Auto", "Display", "Clock", "Off", "On", "Mailbox"}},
    {"frame_rate", "Frame rate", "30: the game as made. Above: a frame for every display refresh at that rate; the game's speed stays the same. Applies at the next start.", NULL, "30", A_RESTART, 0,
     {"30", "60", "90", "120"}, {"30 fps", "60 fps", "90 fps", "120 fps"}},
    {"resolution_scale", "Internal resolution", "The size the game renders at, times its own. Applies when this panel closes. The first time away from 1.0 the game may stop for a while as its shaders are prepared.", "WT_VK_SCALE", "1.0", A_SCALE, 0,
     {"0.4", "0.5", "0.6", "0.75", "0.8", "1.0", "1.2", "1.6", "2.0", "2.4", "3.0"},
     {"0.4 \xC3\x97", "0.5 \xC3\x97", "0.6 \xC3\x97", "0.75 \xC3\x97", "0.8 \xC3\x97", "1.0 \xC3\x97", "1.2 \xC3\x97", "1.6 \xC3\x97", "2.0 \xC3\x97", "2.4 \xC3\x97", "3.0 \xC3\x97"}},
    {"hdr", "HDR", "Auto: HDR output while the desktop's HDR is on for this monitor. The scene's lights go above white, up to HDR peak; menus and the HUD stay at white.", NULL, "auto", A_LIVE, 0,
     {"auto", "off", "on"}, {"Auto", "Off", "On"}},
    {"hdr_paper_white", "HDR white", "How bright the game's white is with HDR on. Auto: the desktop's brightness of SDR programs.", NULL, "auto", A_LIVE, 0,
     {"auto", "100", "150", "200", "250", "300", "400"}, {"Auto", "100 nits", "150 nits", "200 nits", "250 nits", "300 nits", "400 nits"}},
    {"hdr_peak", "HDR peak", "How bright the scene's brightest parts get with HDR on: your monitor's peak brightness. Auto: what the desktop says (Linux), 1000 on Windows.", NULL, "auto", A_LIVE, 0,
     {"auto", "400", "600", "800", "1000", "1500", "2000", "4000"}, {"Auto", "400 nits", "600 nits", "800 nits", "1000 nits", "1500 nits", "2000 nits", "4000 nits"}},
    {"aspect_ratio", "Picture shape", "The shape of the game's own picture. Wider than 16:9 shows more at the sides; Auto is the screen's shape at the start. Applies at the next start.", NULL, "16:9", A_RESTART, 0,
     {"16:9", "auto", "21:9", "32:9"}, {"16:9", "Auto", "21:9", "32:9"}},
    {"aspect", "Picture fit", "Keep shape: the picture keeps its shape, with black bars on a window of another shape. Stretch: it fills the window.", NULL, "keep", A_LIVE, 0,
     {"keep", "stretch"}, {"Keep shape", "Stretch"}},
    {"button_prompts", "Button prompts", "Which buttons the game shows. Auto follows what you use, pad, keyboard or mouse. The others always show that set.", NULL, "auto", A_PROMPTS, 0,
     {"auto", "playstation", "xbox", "switch", "keyboard"}, {"Auto", "PlayStation", "Xbox", "Switch", "Keyboard"}},
    {"swap_confirm", "Swap Confirm", "Menus confirm with B and go back with A, and show it. Gameplay and the keyboard stay the same. This panel follows when it next opens.", NULL, "off", A_MENU_SWAP, 0,
     {"off", "on"}, {"Off", "On"}},
    {"layout", "Face buttons", "Position: the right button acts as A, as on a Switch. Printed letter: the button marked A, Cross as A on PlayStation pads. Prompts follow.", NULL, "auto", A_LAYOUT, 0,
     {"auto", "positional", "labels"}, {"Auto", "Position", "Printed letter"}},
};
#define NROW ((int)(sizeof k_rows / sizeof k_rows[0]))

static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile LONG g_open, g_work, g_job;
static ULONGLONG g_open_at, g_close_at;                    /* the opening's and the closing's start (the GPU path animates them: pcmenu_gpu_scene) */
#define PCM_OPEN_MS 320                                        /* the opening: a quick start, a gradual finish (ease_out) */
#define PCM_CLOSE_MS 220                                       /* the closing: the same way back, quicker */
static volatile LONG g_release;                            /* a bit per npad id: that player still holds what was pressed in the menu */
static int g_sel, g_cur[NROW], g_start[NROW], g_shown[NROW], g_lock_why[NROW], g_inited, g_said_restart[NROW], g_nowin;
static char g_raw[NROW][32];                               /* a value of the file that is none of the row's: shown as it is */
static unsigned g_ver;
static int g_env_cut, g_env_set[NROW];
static int g_swap;                                         /* swap_confirm as this opening of the panel began: a change must not turn the press that made it into "close" */
static int g_fam, g_letter;                                /* the button names of the hints */
/* the environment as the program started (gfx_options sets WT_CUTSCENE_FPS itself, later) */
__attribute__((constructor)) static void pcmenu_env(void) {
    for (int i = 0; i < NROW; i++) g_env_set[i] = k_rows[i].env && getenv(k_rows[i].env) != NULL;
}
static int row_on(int i) { return g_shown[i] && (strncmp(k_rows[i].key, "hdr_", 4) || nvn_vk_hdr_on()); }      /* HDR's two brightness rows: while the picture goes out as HDR */
static int row_count(int i) { int n = 0; while (n < ROW_VALUES && k_rows[i].val[n]) n++; return n; }
static int find_val(int i, const char *v) {
    for (int k = 0; k < row_count(i); k++) {
        if (!_stricmp(k_rows[i].val[k], v)) return k;
        char *e; double a = strtod(v, &e), b = atof(k_rows[i].val[k]);       /* numbers: 1 = 1.0, 0.50 = 0.5 */
        if (e != v && !*e && strspn(k_rows[i].val[k], "+-.0123456789") == strlen(k_rows[i].val[k]) && fabs(a - b) < 1e-6) return k;
    }
    return -1;
}
static void page_go(void) {                                /* the first row shown */
    g_sel = 0;
    while (g_sel < NROW - 1 && !row_on(g_sel)) g_sel++;
}
static void menu_init(void) {                              /* the first opening: the file's values = what runs */
    const GfxOptions *o = gfx_options();
    char v[64];
    for (int i = 0; i < NROW; i++) {
        const Row *r = &k_rows[i];
        int have = r->apply == A_SIZE || r->apply == A_PROMPTS || r->apply == A_MENU_SWAP || r->apply == A_LAYOUT || gfx_ini_get(r->key, v, sizeof v);
        if (r->apply == A_SIZE) snprintf(v, sizeof v, "%dx%d", o->width, o->height);      /* WT_WINDOW included */
        else if (r->apply == A_PROMPTS) snprintf(v, sizeof v, "%s", nnpad_prompts_setting());
        else if (r->apply == A_MENU_SWAP) snprintf(v, sizeof v, "%s", nnpad_menu_swap_setting());
        else if (r->apply == A_LAYOUT) snprintf(v, sizeof v, "%s", nnpad_layout_setting());
        else if (!have) snprintf(v, sizeof v, "%s", r->dflt);
        g_shown[i] = !r->optional || have;
        if ((g_cur[i] = find_val(i, v)) < 0) snprintf(g_raw[i], sizeof g_raw[i], "%s", v);
        g_start[i] = g_cur[i];
        g_lock_why[i] = g_env_set[i] ? 1 : o->profile == 1 && (!strcmp(r->key, "vsync") || !strcmp(r->key, "resolution_scale")) ? 2 : 0;
    }
    if (!g_shown[g_sel]) g_sel = 0;
    g_inited = 1;
}

/* ---------------------------------------------------------------- drawing (BGRA, on the CPU) */
/* The look: the launcher's handheld monitor in green (launcher/ui/launcher.rcss; ui_render.cpp crt): a dark red bezel, a
 * green glass with grid lines and scan lines, bordered rows, the chosen one bright with white words, what the panel has
 * to say in pale green on the glass, a dark bar of button hints at the bottom. The in-game panel is a monitor at the
 * right of the window, over the game's picture (the CPU fallback, which cannot show the game, fills the left with the
 * bezel's dark). Everything is laid out for 1280 x 720 and scaled by s = min(W / 1280, H / 720); the panel keeps to the
 * right edge. */
static stbtt_fontinfo g_f_head, g_f_ui;                    /* Russo One (names, values, hints); M PLUS Rounded 1c (sentences) */
static uint8_t *g_px;                                      /* the panel */
static int g_pw, g_ph, g_rect[4], g_for_w, g_for_h, g_gpu;      /* g_gpu: the renderer composes the words (pcmenu_gpu) */
/* A layer: the same size as the panel, premultiplied BGRA, with the box that was drawn in. While g_to is set, put()
 * draws into it instead of the panel; layer_over() composes it onto the panel. The panel's words are a layer over its
 * ground, so that the GPU path can move them apart (the opening). */
typedef struct { uint8_t *px; int n, x0, y0, x1, y1; } Layer;              /* n: the pixels allocated */
static Layer *g_to;
static void layer_clear(Layer *l) {
    if (!l->px || l->n != g_pw * g_ph) { free(l->px); l->px = calloc((size_t)g_pw * g_ph, 4); l->n = g_pw * g_ph; }
    else memset(l->px, 0, (size_t)g_pw * g_ph * 4);
    l->x0 = g_pw; l->y0 = g_ph; l->x1 = l->y1 = 0;
}
static void layer_over(const Layer *l) {
    if (!l->px) return;
    for (int j = l->y0; j < l->y1; j++) for (int i = l->x0; i < l->x1; i++) {
        const uint8_t *q = l->px + ((size_t)j * g_pw + i) * 4;
        if (!q[3]) continue;
        uint8_t *p = g_px + ((size_t)j * g_pw + i) * 4;
        float k = 1 - q[3] / 255.0f;
        p[0] = (uint8_t)(p[0] * k + q[0] + 0.5f); p[1] = (uint8_t)(p[1] * k + q[1] + 0.5f); p[2] = (uint8_t)(p[2] * k + q[2] + 0.5f);
    }
}
static void layer_blend(int x, int y, float b, float g, float r, float a) {   /* a premultiplied source over the layer */
    Layer *l = g_to;
    uint8_t *p = l->px + ((size_t)y * g_pw + x) * 4;
    float k = 1 - a;
    p[0] = (uint8_t)(p[0] * k + b + 0.5f); p[1] = (uint8_t)(p[1] * k + g + 0.5f); p[2] = (uint8_t)(p[2] * k + r + 0.5f); p[3] = (uint8_t)(p[3] * k + a * 255 + 0.5f);
    if (x < l->x0) l->x0 = x; if (y < l->y0) l->y0 = y; if (x + 1 > l->x1) l->x1 = x + 1; if (y + 1 > l->y1) l->y1 = y + 1;
}
static unsigned g_drawn_ver = ~0u, g_gen;
static int g_row_y[NROW], g_row_h, g_row_x0[NROW], g_row_x1[NROW], g_val_x0[NROW], g_val_x1[NROW], g_close_r[4];   /* row_x: a row's own span (hover, clicks) */
typedef struct { uint8_t b, g, r; } Rgb;
static const Rgb C_GLASS = {0x22, 0x9a, 0x1c}, C_GLASS_LO = {0x0c, 0x4a, 0x06}, C_GRID = {0x50, 0xe6, 0x46}, C_BEZEL0 = {0x2a, 0x12, 0x70}, C_BEZEL1 = {0x12, 0x06, 0x3a},
                 C_ROW = {0x2e, 0xa0, 0x26}, C_EDGE = {0x48, 0xe0, 0x3f}, C_INK = {0x0a, 0x30, 0x04}, C_ON0 = {0x46, 0xe0, 0x3e}, C_ON1 = {0x28, 0xb8, 0x1f}, C_EDGE_ON = {0xe0, 0xff, 0xdf},
                 C_TAB = {0x38, 0xcc, 0x2f}, C_PALE = {0xb8, 0xff, 0xb4}, C_YELLOW = {0x5c, 0xe5, 0xff}, C_WHITE = {255, 255, 255}, C_BLACK = {0, 0, 0}, C_DARK = {0x26, 0x26, 0x26};
static int fonts_init(void) {
    return stbtt_InitFont(&g_f_head, k_font_head, stbtt_GetFontOffsetForIndex(k_font_head, 0)) &&
           stbtt_InitFont(&g_f_ui, k_font_ui, stbtt_GetFontOffsetForIndex(k_font_ui, 0));
}
static void put(int x, int y, Rgb c, float a) {
    if (x < 0 || y < 0 || x >= g_pw || y >= g_ph || a <= 0) return;
    if (g_to) { if (a > 1) a = 1; layer_blend(x, y, c.b * a, c.g * a, c.r * a, a); return; }
    uint8_t *p = g_px + ((size_t)y * g_pw + x) * 4;
    if (a >= 1) { p[0] = c.b; p[1] = c.g; p[2] = c.r; p[3] = 255; return; }
    p[0] = (uint8_t)(p[0] + (c.b - p[0]) * a); p[1] = (uint8_t)(p[1] + (c.g - p[1]) * a); p[2] = (uint8_t)(p[2] + (c.r - p[2]) * a);
}
static Rgb mix(Rgb a, Rgb b, float t) { Rgb c = {(uint8_t)(a.b + (b.b - a.b) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.r + (b.r - a.r) * t)}; return c; }
static void rect(int x, int y, int w, int h, Rgb c, float a) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) put(i, j, c, a); }
static void frame(int x, int y, int w, int h, int t, Rgb c) {      /* a border t thick, inside the box */
    rect(x, y, w, t, c, 1); rect(x, y + h - t, w, t, c, 1); rect(x, y + t, t, h - 2 * t, c, 1); rect(x + w - t, y + t, t, h - 2 * t, c, 1);
}
static void disc(float cx, float cy, float r, Rgb c, float ring) {      /* ring > 0: only the outline */
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++) for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
        float d = sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy)), a = r - d + 0.5f;
        if (ring > 0) { float b = d - (r - ring) + 0.5f; a = a < b ? a : b; }
        put(x, y, c, a > 1 ? 1 : a);
    }
}
/* the monitor from x0 to the right edge, the window's height (the launcher's gen:crt at this scale): the bezel's bz
 * pixels, then the glass, green at its middle and darker out to its rim, with grid lines and scan lines */
static void glass(int x0, int W, int H, float s, int bz) {
    int grid = (int)(92 * s), line = (int)(2 * s + 0.5f), scan = (int)(5 * s + 0.5f);
    if (line < 1) line = 1; if (scan < 3) scan = 3;
    for (int y = 0; y < H; y++) for (int x = x0; x < W; x++) {
        if (x < x0 + bz) { put(x, y, mix(C_BEZEL0, C_BEZEL1, (float)y / H), 1); continue; }
        float dx = ((x + 0.5f - x0) / (W - x0) - 0.5f) / 0.6f, dy = ((y + 0.5f) / H - 0.4f) / 0.75f, d = sqrtf(dx * dx + dy * dy);
        Rgb g = mix(C_GLASS, C_GLASS_LO, d > 1 ? 1 : d);
        if ((x - x0 + (int)(36 * s)) % grid < line || (y + (int)(58 * s)) % grid < line) g = mix(g, C_GRID, 0.22f);
        put(x, y, mix(C_BLACK, g, y % scan < (scan * 2 + 2) / 5 ? 0.8f : 1), 1);
    }
}

/* ---- text ---- */
static int utf8(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    int c = *p++;
    if (c >= 0xF0) { c = (c & 7) << 18 | (p[0] & 63) << 12 | (p[1] & 63) << 6 | (p[2] & 63); p += 3; }
    else if (c >= 0xE0) { c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63); p += 2; }
    else if (c >= 0xC0) { c = (c & 31) << 6 | (p[0] & 63); p += 1; }
    *s = (const char *)p;
    return c;
}
static float text_w(const stbtt_fontinfo *f, const char *s, float px, float track) {
    float sc = stbtt_ScaleForPixelHeight(f, px), w = 0;
    int prev = 0, adv, lsb;
    while (*s) { int c = utf8(&s); if (prev) w += sc * stbtt_GetCodepointKernAdvance(f, prev, c); stbtt_GetCodepointHMetrics(f, c, &adv, &lsb); w += adv * sc + track; prev = c; }
    return w - (prev ? track : 0);
}
static float text_a(const stbtt_fontinfo *f, float x, int base, const char *s, float px, Rgb col, float track, float alpha) {      /* returns the pen's end */
    static uint8_t buf[256 * 256];
    float sc = stbtt_ScaleForPixelHeight(f, px);
    int prev = 0, adv, lsb;
    while (*s) {
        int c = utf8(&s), x0, y0, x1, y1;
        if (prev) x += sc * stbtt_GetCodepointKernAdvance(f, prev, c);
        float fx = x - floorf(x);
        stbtt_GetCodepointBitmapBoxSubpixel(f, c, sc, sc, fx, 0, &x0, &y0, &x1, &y1);
        int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0 && w <= 256 && h <= 256) {
            stbtt_MakeCodepointBitmapSubpixel(f, buf, w, h, w, sc, sc, fx, 0, c);
            for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (buf[j * w + i]) put((int)floorf(x) + x0 + i, base + y0 + j, col, buf[j * w + i] / 255.0f * alpha);
        }
        stbtt_GetCodepointHMetrics(f, c, &adv, &lsb);
        x += adv * sc + track;
        prev = c;
    }
    return x;
}
static float text(const stbtt_fontinfo *f, float x, int base, const char *s, float px, Rgb col, float track) { return text_a(f, x, base, s, px, col, track, 1); }
static int wrap(const stbtt_fontinfo *f, char lines[][160], int maxl, const char *s, float px, float track, float width) {      /* word wrap; track: the letter spacing it is drawn with */
    int n = 0;
    while (*s && n < maxl) {
        char line[160] = "";
        const char *best = s;
        for (const char *p = s; ; p++) {
            if (*p == ' ' || !*p) {
                char t[160]; size_t len = (size_t)(p - s) < sizeof t - 1 ? (size_t)(p - s) : sizeof t - 1;
                memcpy(t, s, len); t[len] = 0;
                if (text_w(f, t, px, track) > width && best != s) break;
                memcpy(line, t, len + 1); best = p;
                if (!*p) break;
            }
        }
        snprintf(lines[n++], 160, "%s", line);
        s = *best ? best + 1 : best;
    }
    return n;
}
static const char *pad_name(int b) {                       /* the button on a pad of the family in use: by position, its Pro name's place */
    static const char *const sw[16] = {"A", "B", "X", "Y", "Left stick press", "Right stick press", "L", "R", "ZL", "ZR", "Plus", "Minus", "D-pad left", "D-pad up", "D-pad right", "D-pad down"};
    static const char *const face[2][2][4] = {{{"Circle", "Cross", "Triangle", "Square"}, {"Cross", "Circle", "Square", "Triangle"}}, {{"B", "A", "Y", "X"}, {"A", "B", "X", "Y"}}};
    static const char *const ps[6] = {"L1", "R1", "L2", "R2", "Options", "Create"}, *const xb[6] = {"LB", "RB", "LT", "RT", "Menu", "View"};
    if (g_fam != 2 && g_fam != 3) return sw[b];
    if (b < 4) return face[g_fam == 3][g_letter][b];
    if (b >= 6 && b <= 11) return (g_fam == 3 ? xb : ps)[b - 6];
    return sw[b];
}
static const char *val_name(int i) {
    return g_cur[i] >= 0 ? k_rows[i].name[g_cur[i]] : g_raw[i];
}
static int row_restart(int i) {                            /* a value that is not the running one yet */
    if (g_cur[i] == g_start[i]) return 0;
    return k_rows[i].apply == A_RESTART || (k_rows[i].apply == A_SCALE && g_open) || ((k_rows[i].apply == A_WINDOW || k_rows[i].apply == A_SIZE) && g_nowin);
}
/* a button hint on the hint bar: the button that acts as Pro button b (0 = A, 1 = B) on player 1's pad, drawn as the
 * prompts show it (g_fam: 1 Switch, 2 PlayStation, 3 Xbox, 4 the keyboard; g_letter: PlayStation and Xbox pads by printed
 * letter or by position, the Face buttons row): a letter cut pale in a dark disc, a PlayStation shape, or a key cap;
 * then the word. Returns the width used; draw = 0 measures only. */
static float hint_btn(float x, int base, float px, int b, const char *what, Rgb disc_c, Rgb mark_c, Rgb text_c, int draw) {
    float r = px * 0.6f, cx = x + r, cy = base - px * 0.33f, gw;
    if (g_fam == 4) {                                      /* the keyboard: Enter confirms, Esc leaves (menu_read) */
        const char *key = b ? "ESC" : "ENT";
        float kpx = px * 0.5f, kw = text_w(&g_f_head, key, kpx, 1 * px / 20) + px * 0.6f, kh = 2 * r * 0.85f, rr = px * 0.15f;
        gw = kw;
        if (draw) {
            float x0 = x, y0 = cy - kh / 2;
            for (int j = (int)y0 - 1; j <= (int)(y0 + kh) + 1; j++) for (int i = (int)x0 - 1; i <= (int)(x0 + kw) + 1; i++) {      /* a rounded box */
                float dx = fabsf(i + 0.5f - (x0 + kw / 2)) - (kw / 2 - rr), dy = fabsf(j + 0.5f - (y0 + kh / 2)) - (kh / 2 - rr);
                float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0, in = dx > dy ? dx : dy, d = sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - rr;
                float a = 0.5f - d;
                if (a > 0) put(i, j, disc_c, a > 1 ? 1 : a);
            }
            float lw = text_w(&g_f_head, key, kpx, 1 * px / 20);
            text(&g_f_head, x0 + (kw - lw) / 2, (int)(cy + kpx * 0.36f), key, kpx, mark_c, 1 * px / 20);
        }
    } else {
        gw = 2 * r;
        if (draw) {
            disc(cx, cy, r, disc_c, 0);
            if (g_fam == 2) {                              /* PlayStation: by position Circle acts as A, by letter Cross does */
                int cross = (b == 0) == (g_letter != 0);
                if (cross) {
                    float L = r * 0.4f, w = px * 0.07f;
                    for (int j = (int)(cy - L - 1); j <= (int)(cy + L + 1); j++) for (int i = (int)(cx - L - 1); i <= (int)(cx + L + 1); i++) {
                        float dx = i + 0.5f - cx, dy = j + 0.5f - cy, u = fabsf(dx + dy) * 0.70710678f, v = fabsf(dx - dy) * 0.70710678f;
                        float d1 = u > w ? u - w : 0, e1 = v > L ? v - L : 0, d2 = v > w ? v - w : 0, e2 = u > L ? u - L : 0;
                        float d = sqrtf(d1 * d1 + e1 * e1); float f = sqrtf(d2 * d2 + e2 * e2); if (f < d) d = f;
                        float a = 1 - d; if (a > 0) put(i, j, mark_c, a > 1 ? 1 : a);
                    }
                } else disc(cx, cy, r * 0.46f, mark_c, px * 0.085f);
            } else {
                const char *letter = pad_name(b);
                float lw = text_w(&g_f_head, letter, px * 0.72f, 0);
                text(&g_f_head, cx - lw / 2, (int)(cy + px * 0.26f), letter, px * 0.72f, mark_c, 0);
            }
        }
    }
    if (draw) text(&g_f_head, x + gw + px * 0.4f, base, what, px, text_c, 0);
    return gw + px * 0.4f + text_w(&g_f_head, what, px, 0);
}
static void alloc_panel(int pw, int ph) {
    if (pw * ph != g_pw * g_ph) { free(g_px); g_px = malloc((size_t)pw * ph * 4); }
    g_pw = pw; g_ph = ph;
}
static float scale_of(int W, int H) { float s = (float)W / 1280.0f < (float)H / 720.0f ? (float)W / 1280.0f : (float)H / 720.0f; return s < 0.45f ? 0.45f : s; }
static int border_of(float s) { int t = (int)(3 * s); return t < 1 ? 1 : t; }
/* a section's plate, as the launcher's lit tab: h high, the words in the middle; returns its width */
static int plate(float x, float y, float h, const char *t, float s) {
    float px = h * 0.48f;
    int w = (int)(text_w(&g_f_head, t, px, 0) + h * 1.4f);
    rect((int)x, (int)y, w, (int)h, C_TAB, 1); frame((int)x, (int)y, w, (int)h, border_of(s), C_EDGE_ON);
    text(&g_f_head, x + h * 0.7f, (int)(y + h * 0.5f + px * 0.36f), t, px, C_WHITE, 0);
    return w;
}
static void foot(int x, int W, int H, float s) {           /* the hint bar */
    rect(x, (int)(H - 64 * s), W - x, (int)(64 * s) + 1, C_BLACK, 0.72f);
    rect(x, (int)(H - 64 * s), W - x, border_of(s * 0.67f), C_WHITE, 0.25f);
}
static void draw_panel_words(int W, int H, float s);
static uint8_t *g_pbg; static int g_pbg_w, g_pbg_h; static unsigned g_pbg_ver = ~0u;     /* the panel's ground, once a size */
static Layer g_words;                                                                  /* over it: the words, once a version */
static float panel_edge(int W, float s) { return W - 600 * s; }                          /* the monitor's left edge */
static void draw_panel(int W, int H) {
    float s = scale_of(W, H);
    alloc_panel(W, H);
    if (!g_px) return;
    g_rect[0] = 0; g_rect[1] = 0; g_rect[2] = W; g_rect[3] = H;
    if (g_pbg && (g_pbg_w != W || g_pbg_h != H)) { free(g_pbg); g_pbg = NULL; }
    int fresh = !g_pbg;
    if (fresh) {
        int edge = (int)panel_edge(W, s);
        rect(0, 0, edge, H, C_BEZEL1, 1);
        glass(edge, W, H, s, (int)(12 * s));
        g_pbg = malloc((size_t)W * H * 4); g_pbg_w = W; g_pbg_h = H;
        if (g_pbg) memcpy(g_pbg, g_px, (size_t)W * H * 4);
    } else memcpy(g_px, g_pbg, (size_t)W * H * 4);
    if (fresh || g_pbg_ver != g_ver) {
        layer_clear(&g_words);
        g_to = &g_words;
        draw_panel_words(W, H, s);
        g_to = NULL;
        g_pbg_ver = g_ver;
    }
    if (!g_gpu) layer_over(&g_words);              /* (else the renderer draws the words over this ground) */
}
static void draw_panel_words(int W, int H, float s) {
    int nshow = 0, bt = border_of(s);
    for (int i = 0; i < NROW; i++) nshow += row_on(i);
    float x0 = panel_edge(W, s) + 52 * s, x1 = W - 40 * s, foot_y = H - 64 * s;
    /* where you are */
    int pw = plate(x0, 26 * s, 46 * s, "PC Settings", s);
    text(&g_f_ui, x0 + pw + 18 * s, (int)(26 * s + 23 * s + 15 * s * 0.36f), "The game waits while this is open.", 15 * s, C_PALE, 0);
    /* the rows; under them the chosen row's state and its help need 124 */
    float top = 92 * s, pitch = 50 * s;
    if (nshow * pitch > foot_y - 124 * s - top) pitch = (foot_y - 124 * s - top) / nshow;
    float rowh = pitch * 0.86f, rpx = rowh * 0.5f;
    int rx = (int)x0, rw = (int)(x1 - x0), rh = (int)rowh, k = 0;
    g_row_h = rh;
    for (int i = 0; i < NROW; i++) {
        g_row_y[i] = -1;
        if (!row_on(i)) continue;
        int y = (int)(top + k++ * pitch), sel = i == g_sel, base = y + (int)(rowh * 0.5f + rpx * 0.36f);
        float vx1 = x1 - 14 * s, vx0 = vx1 - 210 * s, ta = g_lock_why[i] ? 0.5f : 1;
        Rgb tc = sel ? C_WHITE : C_INK;
        g_row_y[i] = y; g_row_x0[i] = rx; g_row_x1[i] = rx + rw; g_val_x0[i] = (int)vx0; g_val_x1[i] = (int)vx1;
        if (sel) for (int j = 0; j < rh; j++) rect(rx, y + j, rw, 1, mix(C_ON0, C_ON1, (float)j / rh), 1);
        else rect(rx, y, rw, rh, C_ROW, 0.7f);
        frame(rx, y, rw, rh, bt, sel ? C_EDGE_ON : C_EDGE);
        text_a(&g_f_head, x0 + 20 * s, base, k_rows[i].label, rpx, tc, 0, ta);
        const char *v = val_name(i);
        text_a(&g_f_head, (vx0 + vx1) / 2 - text_w(&g_f_head, v, rpx, 0) / 2, base, v, rpx, tc, 0, ta);
        if (!g_lock_why[i]) {
            text(&g_f_head, vx0, base, "\xE2\x80\xB9", rpx * 1.25f, tc, 0);
            text(&g_f_head, vx1 - text_w(&g_f_head, "\xE2\x80\xBA", rpx * 1.25f, 0), base, "\xE2\x80\xBA", rpx * 1.25f, tc, 0);
        }
        if (row_restart(i)) disc(vx0 - 14 * s, y + rowh * 0.5f, 5 * s, C_YELLOW, 0);
    }
    /* the state of the chosen row, under the rows at the right */
    char st_txt[160] = "";
    float sa = 1; int dot = 0;
    if (g_lock_why[g_sel] == 1) { snprintf(st_txt, sizeof st_txt, "Set by the environment variable %s", k_rows[g_sel].env); sa = 0.6f; }
    else if (g_lock_why[g_sel] == 2) { snprintf(st_txt, sizeof st_txt, "Set by profile = low in graphics.ini"); sa = 0.6f; }
    else if (row_restart(g_sel)) { snprintf(st_txt, sizeof st_txt, "Saved; applies %s", k_rows[g_sel].apply == A_SCALE ? "when this panel closes" : "at the next start"); dot = 1; }
    else if (g_cur[g_sel] != g_start[g_sel]) snprintf(st_txt, sizeof st_txt, "Applied and saved");
    if (st_txt[0]) {
        float px = 15 * s, w = text_w(&g_f_ui, st_txt, px, 0);
        int sb = (int)(top + nshow * pitch + 16 * s);
        if (dot) disc(x1 - w - 14 * s, sb - px * 0.36f, 5 * s, C_YELLOW, 0);
        text_a(&g_f_ui, x1 - w, sb, st_txt, px, C_PALE, 0, sa);
    }
    /* the chosen row's help on the glass above the hint bar: up to three lines, in smaller type until it fits (four at the smallest) */
    char ln[4][160];
    int n;
    float hpx;
    for (hpx = 17 * s; (n = wrap(&g_f_ui, ln, 4, k_rows[g_sel].help, hpx, 0, x1 - x0)) > 3 && hpx > 13 * s; hpx -= 0.5f * s) {}
    for (int l = 0; l < n; l++)
        text(&g_f_ui, (x0 + x1) / 2 - text_w(&g_f_ui, ln[l], hpx, 0) / 2, (int)(foot_y - 48 * s + (l - (n - 1) / 2.0f) * hpx * 1.3f + hpx * 0.36f), ln[l], hpx, C_PALE, 0);
    /* the hint bar: the buttons */
    foot((int)(x0 - 40 * s), W, H, s);
    int fb = (int)(foot_y + 32 * s + 22 * s * 0.36f);
    float fpx = 22 * s;
    text_a(&g_f_ui, x0, fb - (int)(2 * s), "F1  \xC2\xB7  Start + Select", 14 * s, C_WHITE, 0, 0.6f);
    const char *close = "Close";
    float wc = hint_btn(0, fb, fpx, g_swap ? 1 : 0, "Change", C_WHITE, C_DARK, C_WHITE, 0), wb = hint_btn(0, fb, fpx, g_swap ? 0 : 1, close, C_WHITE, C_DARK, C_WHITE, 0), x = x1 - wc - 26 * s - wb;
    hint_btn(x, fb, fpx, g_swap ? 1 : 0, "Change", C_WHITE, C_DARK, C_WHITE, 1);
    x += wc + 26 * s;
    g_close_r[0] = (int)(x - 10 * s); g_close_r[1] = fb - (int)(30 * s); g_close_r[2] = (int)(wb + 20 * s); g_close_r[3] = (int)(44 * s);
    hint_btn(x, fb, fpx, g_swap ? 0 : 1, close, C_WHITE, C_DARK, C_WHITE, 1);
}


/* The first-start screen (nvn_vk_draw.c prep_first_hold): the game is held at its first frame while every shader is
 * prepared; the render thread sets the numbers here about ten times a second and copies the panel over the frame. The
 * monitor and the words are drawn once a size (g_bg) and the numbers over a copy of it. */
static struct { int on, pct, eta, reading; long long done, total; char info[96]; } g_prep;
static uint8_t *g_bg; static int g_bg_w, g_bg_h;
static void draw_prep(int W, int H) {
    float s = scale_of(W, H);
    alloc_panel(W, H);
    if (!g_px) return;
    g_rect[0] = 0; g_rect[1] = 0; g_rect[2] = W; g_rect[3] = H;
    if (g_bg && (g_bg_w != W || g_bg_h != H)) { free(g_bg); g_bg = NULL; }
    if (!g_bg) {
        glass(0, W, H, s, 0);
        plate(72 * s, 60 * s, 46 * s, "First start", s);
        text(&g_f_head, 72 * s, (int)(150 * s + 54 * s * 0.78f), "Preparing shaders", 54 * s, C_WHITE, 0);
        text(&g_f_ui, 72 * s, (int)(596 * s), "First start only  \xC2\xB7  Closing the window keeps what is done", 16 * s, C_PALE, 0);
        foot(0, W, H, s);
        text_a(&g_f_ui, 72 * s, (int)(H - 32 * s + 15 * s * 0.36f), g_prep.info, 15 * s, C_WHITE, 0, 0.8f);
        g_bg = malloc((size_t)W * H * 4); g_bg_w = W; g_bg_h = H;
        if (g_bg) memcpy(g_bg, g_px, (size_t)W * H * 4);
    } else memcpy(g_px, g_bg, (size_t)W * H * 4);
    /* the numbers */
    char t[64];
    float big = 84 * s, x = 72 * s;
    int pb = (int)(430 * s + big * 0.78f), bt = border_of(s);
    snprintf(t, sizeof t, "%d", g_prep.pct);
    x = text(&g_f_head, x, pb, t, big, C_WHITE, 0);
    x = text(&g_f_head, x + 2 * s, pb, "%", 36 * s, C_PALE, 0) + 18 * s;
    if (g_prep.reading) snprintf(t, sizeof t, "reading the game data");
    else snprintf(t, sizeof t, "%lld of about %lld stages", g_prep.done, g_prep.total);
    text(&g_f_ui, x, pb - (int)(26 * s), t, 18 * s, C_PALE, 0);
    if (g_prep.eta >= 0) snprintf(t, sizeof t, "about %d:%02d left", g_prep.eta / 60, g_prep.eta % 60); else snprintf(t, sizeof t, "working out the time left");
    text(&g_f_ui, x, pb, t, 18 * s, C_PALE, 0);
    int bx = (int)(72 * s), by = (int)(536 * s), bw = (int)(560 * s), bh = (int)(22 * s);      /* the bar: bordered as a row, filling from its left end */
    rect(bx, by, bw, bh, C_ROW, 0.7f);
    rect(bx, by, bw * (g_prep.pct < 0 ? 0 : g_prep.pct > 100 ? 100 : g_prep.pct) / 100, bh, C_ON0, 1);
    frame(bx, by, bw, bh, bt, C_EDGE_ON);
}
/* the render thread (nvn_vk_draw.c): the first-start screen's numbers; reading: the game data is still being read */
void pcmenu_prep_show(int pct, long long done, long long total, int eta, int reading, const char *info) {
    AcquireSRWLockExclusive(&g_lock);
    if (!g_inited && fonts_init()) menu_init();
    if (!g_prep.on || g_prep.pct != pct || g_prep.done != done || g_prep.total != total || g_prep.eta != eta || g_prep.reading != reading) g_ver++;
    g_prep.on = 1; g_prep.pct = pct; g_prep.done = done; g_prep.total = total; g_prep.eta = eta; g_prep.reading = reading;
    snprintf(g_prep.info, sizeof g_prep.info, "%s", info ? info : "");
    ReleaseSRWLockExclusive(&g_lock);
}
void pcmenu_prep_hide(void) {
    AcquireSRWLockExclusive(&g_lock);
    g_prep.on = 0; g_ver++;
    free(g_bg); g_bg = NULL;
    ReleaseSRWLockExclusive(&g_lock);
}

/* render thread (nvn_vk_pcmenu.c): the panel (or the hint) for this window size, drawn again when something changed */
int pcmenu_is_open(void) { return g_open != 0; }
int pcmenu_kind(void) { return g_open ? 1 : g_prep.on ? 3 : 0; }      /* what is shown: the menu, the first-start screen */
int pcmenu_settled(void) { return pcmenu_kind() != 1 || (!g_close_at && GetTickCount64() - g_open_at >= 1000); }    /* the menu done opening (its slide, its rows): WT_PCMENU_SHOT waits for it */
int pcmenu_visible(void) { return pcmenu_kind() != 0; }
int pcmenu_panel_size(int W, int H, int r[4], unsigned *gen) {
    static int drawn_kind = -1;
    int kind = pcmenu_kind();
    if (!kind) return 0;
    AcquireSRWLockExclusive(&g_lock);
    if (g_drawn_ver != g_ver || g_for_w != W || g_for_h != H || drawn_kind != kind) {
        if (kind == 1) draw_panel(W, H); else draw_prep(W, H);
        g_drawn_ver = g_ver; g_for_w = W; g_for_h = H; drawn_kind = kind; g_gen++;
    }
    memcpy(r, g_rect, sizeof g_rect); *gen = g_gen;
    ReleaseSRWLockExclusive(&g_lock);
    return g_px != NULL;
}
void pcmenu_panel_read(void *dst, int w, int h) {
    AcquireSRWLockExclusive(&g_lock);
    if (g_px && w == g_pw && h == g_ph) memcpy(dst, g_px, (size_t)w * h * 4);
    ReleaseSRWLockExclusive(&g_lock);
}
int pcmenu_take_work(void) { InterlockedExchange(&g_job, 0); return (int)InterlockedExchange(&g_work, 0); }
/* The GPU path (nvn_vk_pcmenu.c): on, the panel stops composing its words here; the renderer draws them every frame
 * from pcmenu_gpu_scene over the ground, from the layer (pcmenu_layer_read: premultiplied BGRA of the panel's size,
 * drawn once a version). Changing the mode draws the panel again. */
void pcmenu_gpu(int on) {
    AcquireSRWLockExclusive(&g_lock);
    if (g_gpu != (on != 0)) { g_gpu = on != 0; g_drawn_ver = ~0u; free(g_pbg); g_pbg = NULL; }
    ReleaseSRWLockExclusive(&g_lock);
}
static float ease_out(float t) { if (t <= 0) return 0; if (t >= 1) return 1; float u = 1 - t; return 1 - u * u * u; }      /* a quick start, a gradual finish */
/* a sprite: the ground (layer 0) or the words (1), the whole picture moved right by x at the opacity alpha; only what is
 * at clip or right of it, and (y1 > y0) only the rows y0 <= y < y1 */
typedef struct { int layer; float x, alpha, clip, y0, y1; } PcmSprite;
static PcmSprite *g_emit; static int g_emit_n, g_emit_max;
static void emit(int layer, float x, float alpha, float clip, float y0, float y1) {
    if (g_emit_n >= g_emit_max || alpha <= 0) return;
    PcmSprite e = {layer, x, alpha, clip, y0, y1};
    g_emit[g_emit_n++] = e;
}
/* The sprites to draw for the window (W, H) now, in order. The first-start screen: none, the ground the renderer copied
 * is all of it. The panel when the renderer cannot start from the game's picture (over_game in): the words over the
 * ground it copied (*over_game = 0). The panel otherwise (*over_game = 1): the monitor as sprites over the game's
 * picture, clipped at its left edge, so the game shows left of it; while it opens (PCM_OPEN_MS) it slides in from the
 * right, and the rows step in from the right one after another; closing (PCM_CLOSE_MS) slides it out, together. */
int pcmenu_gpu_scene(int W, int H, PcmSprite *out, int max, int *over_game) {
    int can = *over_game;
    *over_game = 0;
    if (pcmenu_kind() != 1) return 0;
    AcquireSRWLockExclusive(&g_lock);
    float s = scale_of(W, H), edge = panel_edge(W, s), fh = (float)H;
    ULONGLONG now = GetTickCount64();
    g_emit = out; g_emit_n = 0; g_emit_max = max;
    if (!can) emit(1, 0, 1, 0, 0, 0);
    else {
        *over_game = 1;
        float open = g_close_at ? 1 - ease_out((float)(now - g_close_at) / PCM_CLOSE_MS) : ease_out((float)(now - g_open_at) / PCM_OPEN_MS);      /* 0 away .. 1 in place */
        float dx = (1 - open) * (W - edge), ex = edge + dx;
        emit(0, dx, 1, ex, 0, 0);
        if (open >= 1 && !g_close_at) emit(1, 0, 1, edge, 0, 0);
        else {                                     /* the rows one after another from the right, within the monitor; the rest with it */
            float rows_top = fh, rows_bot = 0;
            int k = 0, nshow = 0;
            for (int i = 0; i < NROW; i++) nshow += g_row_y[i] >= 0;
            float stag = nshow > 1 ? (0.2f / (nshow - 1) < 0.04f ? 0.2f / (nshow - 1) : 0.04f) : 0;
            for (int i = 0; i < NROW; i++) {
                if (g_row_y[i] < 0) continue;
                float y0 = (float)g_row_y[i], y1 = y0 + g_row_h;
                if (y0 < rows_top) rows_top = y0; if (y1 > rows_bot) rows_bot = y1;
                float p = g_close_at ? open : ease_out(((float)(now - g_open_at) / 1000 - 0.06f - k * stag) / 0.24f), sh = (1 - p) * 70 * s;
                emit(1, dx + sh, p, ex, y0, y1);
                k++;
            }
            if (rows_top > rows_bot) emit(1, dx, 1, ex, 0, 0);
            else { emit(1, dx, 1, ex, 0, rows_top); emit(1, dx, 1, ex, rows_bot, fh); }
        }
    }
    int n = g_emit_n;
    g_emit = NULL; g_emit_n = g_emit_max = 0;
    ReleaseSRWLockExclusive(&g_lock);
    return n;
}
int pcmenu_layer_read(void *dst, int w, int h) {                     /* the words' pixels (w x h, the panel's size): 0 when there are none */
    int ok = 0;
    AcquireSRWLockExclusive(&g_lock);
    if (g_words.px && w == g_pw && h == g_ph && g_words.n == w * h && g_words.x1 > g_words.x0) { memcpy(dst, g_words.px, (size_t)w * h * 4); ok = 1; }
    ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

/* ---------------------------------------------------------------- changes */
static void add_work(LONG bit) { LONG o; do o = g_work; while (InterlockedCompareExchange(&g_work, o | bit, o) != o); }
static void set_value(int i, int k) {                    /* value k of row i: written, and applied where it can be */
    const Row *r = &k_rows[i];
    if (g_lock_why[i] || k < 0 || k >= row_count(i) || k == g_cur[i]) return;
    int ok;
    if (r->apply == A_SIZE) { char w[16], h[16]; sscanf(r->val[k], "%15[0-9]x%15[0-9]", w, h); ok = gfx_option_set("width", w) && gfx_option_set("height", h); }
    else if (r->apply == A_PROMPTS) { ok = nnpad_prompts_set(r->val[k]); g_fam = nnpad_button_prompts(); g_letter = nnpad_prompts_by_letter(); }      /* the hints and the bind names follow */
    else if (r->apply == A_MENU_SWAP) ok = nnpad_menu_swap_set(r->val[k]);
    else if (r->apply == A_LAYOUT) { ok = nnpad_layout_set(r->val[k]); g_letter = nnpad_prompts_by_letter(); }
    else ok = gfx_option_set(r->key, r->val[k]);
    if (!ok) return;
    g_cur[i] = k;
    switch (r->apply) {
    case A_VSYNC: display_vsync_apply(); add_work(2); break;
    case A_WINDOW: case A_SIZE: if (!display_window_apply()) g_nowin = 1; break;
    case A_SCALE: if (!g_open) add_work(4); break;         /* (the panel: when it closes) */
    default: break;
    }
    printf("pcmenu: %s = %s (%s)\n", r->key, r->val[k], !row_restart(i) ? "applied" : r->apply == A_SCALE ? "applies when the panel closes" : "applies at the next start");
}
/* a value's first two numbers (1.4; 1366x768; 16:10); 0 when it starts with none (auto, game) */
static int numbers2(const char *s, double *a, double *b) {
    char *e; *a = strtod(s, &e); *b = e != s && *e ? strtod(e + 1, NULL) : 0;
    return e != s;
}
static int before2(double a0, double a1, double b0, double b1) { return a0 < b0 || (a0 == b0 && a1 < b1); }
/* from a value not in the list (another version's, or the file edited by hand): the nearest one that way by its numbers
 * (1.4 -> 1.2 or 1.6, 16:10 -> 16:9 on the left); none that way: the ends when round (wrap), else -1 (stays) */
static int nearest(int i, int dir, int wrap) {
    double v0, v1, o0, o1, b0 = 0, b1 = 0;
    int n = row_count(i), best = -1, num = numbers2(g_raw[i], &v0, &v1), any = 0;      /* any: a row of numbers */
    for (int k = 0; k < n && num; k++) {
        if (!numbers2(k_rows[i].val[k], &o0, &o1)) continue;
        any = 1;
        if ((dir > 0 ? before2(v0, v1, o0, o1) : before2(o0, o1, v0, v1)) &&
            (best < 0 || (dir > 0 ? before2(o0, o1, b0, b1) : before2(b0, b1, o0, o1)))) { best = k; b0 = o0; b1 = o1; }
    }
    return best >= 0 ? best : wrap || !any ? (dir > 0 ? 0 : n - 1) : -1;
}
static void change(int i, int dir, int wrap) {             /* wrap 0 (left / right): stops at the ends; 1 (A, a click): round */
    int n = row_count(i);
    set_value(i, g_cur[i] < 0 ? nearest(i, dir, wrap) : wrap ? (g_cur[i] + dir + n) % n : g_cur[i] + dir);
}
static int activate(int i, int dir) {                      /* A or a click on a row; 1 = it took the press and the row's value is not what changes */
    change(i, dir, 1);
    return 0;
}

/* ---------------------------------------------------------------- the menu's thread */
static int g_log = -1;
static void mouse_at(int *x, int *y, int *down) {          /* window pixels; -1 = not over the window */
    *x = *y = -1;
    *down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#ifdef _WIN32
    POINT p; HWND h = GetForegroundWindow(); DWORD pid = 0;
    if (h) GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId()) { *down = 0; return; }       /* another program in front: its pointer and clicks are not ours (as nnhid.c's keyboard_ours) */
    if (GetCursorPos(&p) && ScreenToClient(h, &p)) { *x = p.x; *y = p.y; }
#else
    { extern int display_mouse_pixels(int *x, int *y); display_mouse_pixels(x, y); }
#endif
}
static DWORD WINAPI menu_thread(LPVOID arg) {
    (void)arg;
    int prev_dir = 0, prev_ok = 1, prev_back = 1, prev_click = 1, rep_at = 0, mx0 = -2, my0 = -2;
    ULONGLONG t_dir = 0;
    float sens; int inv;
    display_mouse_want(0);                                 /* the cursor back for the menu (the mouse camera lets it go) */
    while (g_open && !g_display_close) {
        uint64_t b; int lx, ly, mx, my, md;
        nnhid_menu_read(&b, &lx, &ly, g_swap);
        enum { BA = 1 << 0, BB = 1 << 1, BPLUS = 1 << 10, BMINUS = 1 << 11, BLEFT = 1 << 12, BUP = 1 << 13, BRIGHT = 1 << 14, BDOWN = 1 << 15 };
        int up = (b & BUP) || ly > 16000 || nnhid_menu_key(VK_UP), down = (b & BDOWN) || ly < -16000 || nnhid_menu_key(VK_DOWN);
        int left = (b & BLEFT) || lx < -16000 || nnhid_menu_key(VK_LEFT), right = (b & BRIGHT) || lx > 16000 || nnhid_menu_key(VK_RIGHT);
        int ok = (b & BA) || nnhid_menu_key(VK_RETURN) || nnhid_menu_key(VK_SPACE);
        int back = (b & BB) || nnhid_menu_key(VK_ESCAPE) || nnhid_menu_key(VK_BACK) || nnhid_menu_key(VK_F1) || (b & (BPLUS | BMINUS)) == (BPLUS | BMINUS);
        int dir = up ? 1 : down ? 2 : left ? 3 : right ? 4 : 0;
        ULONGLONG now = GetTickCount64();
        int step = dir && (dir != prev_dir || (rep_at && now >= t_dir + (ULONGLONG)rep_at));
        if (dir != prev_dir) { t_dir = now; rep_at = 380; } else if (step) { t_dir = now; rep_at = 90; }
        prev_dir = dir;
        mouse_at(&mx, &my, &md);
        AcquireSRWLockExclusive(&g_lock);
        int was_sel = g_sel, was_cur[NROW];
        memcpy(was_cur, g_cur, sizeof g_cur);
        if (step && (dir == 1 || dir == 2)) { do g_sel = (g_sel + (dir == 1 ? NROW - 1 : 1)) % NROW; while (!row_on(g_sel)); }
        if (step && (dir == 3 || dir == 4)) change(g_sel, dir == 3 ? -1 : 1, 0);
        if (ok && !prev_ok) { activate(g_sel, 1); g_ver++; }
        int px = mx - g_rect[0], py = my - g_rect[1], click = md && !prev_click;
        if (mx >= 0 && (mx != mx0 || my != my0 || click) && px >= 0 && px < g_pw && py >= 0 && py < g_ph) {      /* the mouse: hover chooses, a click changes */
            for (int i = 0; i < NROW; i++)                 /* a row's own span: the panel covers the window, its rows step down the diagonal */
                if (g_row_y[i] >= 0 && py >= g_row_y[i] && py < g_row_y[i] + g_row_h && px >= g_row_x0[i] && px < g_row_x1[i]) {
                    g_sel = i;
                    if (click && px >= g_val_x0[i] - g_row_h) { activate(i, px < g_val_x0[i] + g_row_h ? -1 : 1); g_ver++; break; }
                }
            if (click && px >= g_close_r[0] && px < g_close_r[0] + g_close_r[2] && py >= g_close_r[1] && py < g_close_r[1] + g_close_r[3]) back = 1, prev_back = 0;
        }
        mx0 = mx; my0 = my;
        if (g_sel != was_sel || memcmp(was_cur, g_cur, sizeof g_cur)) g_ver++;
        ReleaseSRWLockExclusive(&g_lock);
        if (g_log && (step || (ok && !prev_ok) || (back && !prev_back) || click)) printf("pcmenu: input: %s%s%s%s -> row %d (%s)\n", step ? (dir == 1 ? "up " : dir == 2 ? "down " : dir == 3 ? "left " : "right ") : "", ok && !prev_ok ? "A " : "", back && !prev_back ? "B " : "", click ? "click " : "", g_sel, k_rows[g_sel].key);
        prev_ok = ok; prev_click = md;
        if (back && !prev_back && !g_close_at) {
            g_close_at = now; g_ver++;                    /* the closing plays (PCM_CLOSE_MS); the game goes on after it */
        }
        prev_back = back;
        if (g_close_at && (now - g_close_at >= PCM_CLOSE_MS || !g_gpu)) break;
        if (!InterlockedExchange(&g_job, 1) && !nvn_front_post_pcmenu()) InterlockedExchange(&g_job, 0);
        Sleep(16);
    }
    InterlockedExchange(&g_release, 0xff);                 /* each player reads an idle pad until it has let go of everything */
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < NROW; i++) if (k_rows[i].apply == A_SCALE) g_start[i] = g_cur[i];
    InterlockedExchange(&g_open, 0);
    ReleaseSRWLockExclusive(&g_lock);
    if (!g_display_close) { add_work(4); nvn_front_post_pcmenu(); }      /* before the game goes on: its next commands come after the job */
    InterlockedExchange(&g_guest_frozen, 0);
    if (nnpad_mouse_cfg(&sens, &inv)) display_mouse_want(1);
    printf("pcmenu: closed; the game goes on\n");
    fflush(stdout);
    return 0;
}
static void menu_open(const char *why) {
    if (InterlockedExchange(&g_open, 1)) return;
    if (g_log < 0) g_log = getenv("WT_PCMENU_LOG") && atoi(getenv("WT_PCMENU_LOG"));
    AcquireSRWLockExclusive(&g_lock);
    if (!g_f_ui.data && !fonts_init()) { ReleaseSRWLockExclusive(&g_lock); InterlockedExchange(&g_open, 0); printf("pcmenu: the font cannot be read\n"); return; }
    if (!g_inited) menu_init();
    g_swap = nnpad_menu_swapped();
    g_fam = nnpad_button_prompts();
    g_letter = nnpad_prompts_by_letter();
    g_open_at = GetTickCount64(); g_close_at = 0;
    page_go();
    g_ver++;
    ReleaseSRWLockExclusive(&g_lock);
    InterlockedExchange(&g_guest_frozen, 1);               /* the game stops at its next SDK call (hostrt.c) */
    printf("pcmenu: opened (%s); the game is held\n", why);
    fflush(stdout);
    HANDLE th = CreateThread(NULL, 0, menu_thread, NULL, 0, NULL);
    if (th) CloseHandle(th); else { InterlockedExchange(&g_guest_frozen, 0); InterlockedExchange(&g_open, 0); }
}
/* nnhid.c, at every pad state the game reads: opens the menu, and says when the game gets an idle pad */
int pcmenu_gate(uint32_t id, uint64_t buttons) {
    static int prev = 1, test = -1;
    static ULONGLONG t0;
    LONG bit = id < 8 ? 1L << id : 0, o;
    if (g_open) return 1;
    if (id == 0) {
        int f1 = nnhid_menu_key(VK_F1), combo = (buttons & 0xc00) == 0xc00;      /* Start + Select (the game uses L3 and R3) */
        if (test < 0) { const char *e = getenv("WT_PCMENU_OPEN"); test = e ? atoi(e) : 0; t0 = GetTickCount64(); }
        if (test > 0 && GetTickCount64() - t0 >= (ULONGLONG)test * 1000) { test = 0; menu_open("WT_PCMENU_OPEN"); return 1; }
        if ((f1 || combo) && !prev && !(g_release & 1)) { prev = 1; menu_open(f1 ? "F1" : "Start + Select"); return 1; }
        prev = f1 || combo;
        if (prev && (g_release & 1)) return 1;             /* the F1 that closed it is still down */
    }
    if (!(g_release & bit)) return 0;
    if (buttons) return 1;
    do o = g_release; while (InterlockedCompareExchange(&g_release, o & ~bit, o) != o);
    if (g_log > 0) printf("pcmenu: player %u's input reaches the game again\n", id + 1);
    return 0;
}
