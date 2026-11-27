/* Shim of the game DLL generated WITH landing-pad resume entries (tools/hlbuild/src/lifter/eh.rs).
 * Everything of game_shim.c, plus what the exception unwinder needs from main: the resume table, and the host's
 * nro_catch_call for the functions that have a catch handler. The host finds game_eh_bind with GetProcAddress
 * (nncxxrt.c), so an exe works with either kind of DLL. */
#include "game_shim.c"
#include <stdlib.h>

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

/* Motion vectors for moving objects (runtime/nvn_vk_fsr.c fsr_mv_last): by the address of an object's constant-buffer
 * slot this frame, the slot it used last frame. A game fills it from a host override of its upload; empty (as here),
 * objects keep the camera's vector. */
typedef struct { volatile uint64_t dst, prev; } GameMvPair;
__declspec(dllexport) GameMvPair game_mv_pairs[1 << 16];

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

/* Gallery / FE synchronous archive unload (Archive::Unload, 0x1e6f0, vtable 0x1692190+0x18): when an entity is marked
 * for deletion (vtable+0x28, 0x9501e0) it is appended to the circular pending-deletion list rooted at EntityManager+0x10
 * (EntityManager = *(main+0x1ca4dc8)), which is only drained inside 30 Hz GameSimulation (0x737780 -> 0x94b600).
 * At frame_rate > 30, FEManager::Update runs every rendered frame while GameSimulation skips 1-3 frames between 30 Hz
 * steps, so Gallery's one-frame-later synchronous bundle unload (0x68dfc0 with async=0 -> 0x1e6f0 with x1=0) frees the
 * bundle's skeleton OBJECTS slots before the pending ghost entity is destroyed and unregistered; Rare Ghosts (Group 3)
 * even unloads on the same frame at 30 fps. RenderManager::RenderFrame (0x464920) then dereferences the freed OBJECTS
 * slot and crashes. Draining the already-queued deletions right before a synchronous archive unload preserves the game's
 * 30 Hz simulation timing while guaranteeing no live entity still references assets the unload is about to free. */
void f_710001e6f0(Ctx *c);
void h_archive_unload(Ctx *c) {
    if ((c->x[1] & 1) == 0) {
        uint64_t mgr = *(const uint64_t *)(uintptr_t)0x7101ca4dc8ULL;
        if (mgr && *(const uint64_t *)(uintptr_t)(mgr + 16)) {
            uint64_t a0 = c->x[0], a1 = c->x[1], a2 = c->x[2], a3 = c->x[3], lr = c->x[30];
            while (*(uint64_t *)(uintptr_t)(mgr + 16)) {
                uint64_t tail = *(uint64_t *)(uintptr_t)(mgr + 16);
                *(uint64_t *)(uintptr_t)(mgr + 16) = 0;
                uint64_t cur = *(uint64_t *)(uintptr_t)(tail + 24);
                for (;;) {
                    uint64_t next = *(uint64_t *)(uintptr_t)(cur + 24);
                    *(uint64_t *)(uintptr_t)(cur + 24) = 0;
                    uint64_t vt = *(uint64_t *)(uintptr_t)cur;
                    c->x[0] = cur;
                    c->x[30] = 0x710094ba64ULL;
                    call_indirect(c, *(const uint64_t *)(uintptr_t)(vt + 128));
                    if (cur == tail) break;
                    cur = next;
                }
            }
            c->x[0] = a0; c->x[1] = a1; c->x[2] = a2; c->x[3] = a3; c->x[30] = lr;
        }
    }
    f_710001e6f0(c);
}
