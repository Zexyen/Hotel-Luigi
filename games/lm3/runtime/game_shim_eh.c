/* Shim of the game DLL generated WITH landing-pad resume entries (tools/hlbuild/src/lifter/eh.rs).
 * Everything of game_shim.c, plus what the exception unwinder needs from main: the resume table, and the host's
 * nro_catch_call for the functions that have a catch handler. The host finds game_eh_bind with GetProcAddress
 * (nncxxrt.c), so an exe works with either kind of DLL. */
#include "game_shim.c"

typedef struct { uint64_t lp; void (*fn)(Ctx *, uint64_t); } NroResume;
extern const NroResume resume_table_main[];
extern const int resume_count_main;

static void (*host_catch_call)(Ctx *, void (*)(Ctx *, uint64_t));
void nro_catch_call(Ctx *c, void (*body)(Ctx *, uint64_t)) { host_catch_call(c, body); }

__declspec(dllexport) void game_eh_bind(void (*catch_call)(Ctx *, void (*)(Ctx *, uint64_t)), const NroResume **table, int *count) {
    host_catch_call = catch_call;
    *table = resume_table_main;
    *count = resume_count_main;
}

/* Host versions of game functions go here: the lifter sends the direct calls of an address listed in HOST_OVERRIDES
 * (../hlbuild/mod.rs) to h_<name>(Ctx *c), defined in this file, and so does the function table (calls through a pointer). */

/* graphics.ini aspect_ratio (the 21:9 mod, built in; RECOMPS_ASPECT from gfx_options.c): main's screen-shape function
 * (0x67be0: the screen's width / height) answers the shape asked for. The game's own function still runs: its first
 * call initialises the screen settings. The 16:9 constant the mod also patches: lm3_imports.c lm3_image_options. */
void f_7100067be0(Ctx *c);
void h_aspect(Ctx *c) {
    static float a = -1.0f;     /* whichever thread comes first sets it, each to the same value */
    f_7100067be0(c);
    if (a < 0.0f) { const char *e = getenv("RECOMPS_ASPECT"); a = e ? (float)atof(e) : 0.0f; }
    if (a > 0.0f) { c->v[0].d[0] = c->v[0].d[1] = 0; c->v[0].f[0] = a; }
}

/* What the runtime's FSR pass (runtime/nvn_vk_fsr.c) tells the game while it runs: the scene image's size (0 = off) and
 * AMD's jitter phase count. A game that supports FSR shifts its projection by them, each frame, in a host override. */
__declspec(dllexport) volatile int game_fsr_w, game_fsr_h, game_fsr_phases;

/* graphics.ini frame_rate = 60 .. 120: the game's step keeps the time a hitch takes (cutscene sound ahead of the picture).
 * FrameTimer::Update (0x79f320) steps by 0.8 x the last step + 0.2 x the mean of the middle 7 of the last 11 frame times
 * (main+0x13c91ec, +0x13c90a8), so a long frame is left out of the mean and its time is never stepped, while Wwise plays
 * the cutscene's sound on in real time. A console at 30 fps does not hitch; a PC at 60 and above does, and every hitch put the
 * picture further behind (measured, 100 ms hitches: 95 ms each lost at 120 fps). Here the time not stepped is owed: what
 * is over one step is paid at once, in steps of at most 1/30 s (the game's own at 30 fps), so a hitch's time lands within
 * a few frames of it (one just before a cutscene's sound starts must not put the picture ahead); the rest, the jitter of
 * frames, an eighth a frame, so the game's smoothing still smooths. The timer (main+0x1c73e78), as
 * floats: [2] this frame's time in s (the game caps it at 0.35), [3] the time scale, [4] step x scale, [5] step; [5] is
 * also the game's "last step", so its own value goes back before it runs again. 30 fps (the game as made): unchanged. */
void f_710079f320(Ctx *c);
void h_frame_timer(Ctx *c) {
    static int on = -1;
    static float own;
    static double owed;
    float *t = (float *)(uintptr_t)0x7101c73e78ULL;
    if (on < 0) on = *(const float *)(uintptr_t)0x71013c94dcULL != 100.0f / 3.0f;      /* the frame time lm3_imports.c sets for 60 and above */
    if (!on) { f_710079f320(c); return; }
    if (own > 0.0f) t[5] = own;
    f_710079f320(c);
    own = t[5];
    owed += (double)t[2] - own;
    if (owed > 1.0) owed = 1.0; else if (owed < -1.0) owed = -1.0;      /* a stall longer than a second is not made up */
    double pay = owed > own ? owed - own : owed / 8.0, most = 1.0 / 30.0 - own;
    if (pay > most) pay = most > 0.0 ? most : 0.0; else if (pay < -0.5 * own) pay = -0.5 * own;
    owed -= pay;
    t[5] = own + (float)pay;
    t[4] = t[5] * t[3];
}
