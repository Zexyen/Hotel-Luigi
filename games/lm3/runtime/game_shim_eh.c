/* Shim of the game DLL generated WITH landing-pad resume entries (tools/wtbuild/src/lifter/eh.rs).
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
 * (../wtbuild/mod.rs) to h_<name>(Ctx *c), defined in this file. The stand-in has none. */

/* What the runtime's FSR pass (runtime/nvn_vk_fsr.c) tells the game while it runs: the scene image's size (0 = off) and
 * AMD's jitter phase count. A game that supports FSR shifts its projection by them, each frame, in a host override. */
__declspec(dllexport) volatile int game_fsr_w, game_fsr_h, game_fsr_phases;
