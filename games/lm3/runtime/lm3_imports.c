/* Imports of Luigi's Mansion 3's main (nnSdk 7.3.2) that the shared runtime had no handler for, reached at boot.
 * Registered by boot.c after b3_imports.c (later registrations replace earlier ones). Each one says what it models. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "runtime.h"
#include "hostrt.h"
#include "gfx_options.h"
#include "plat.h"

#define A0 (c->x[0])
#define A1 (c->x[1])
#define A2 (c->x[2])
#define A3 (c->x[3])
#define RET(v) (c->x[0] = (uint64_t)(v))

static void nothing(Ctx *c) { (void)c; }
static void ret0(Ctx *c) { RET(0); }

/* void sincosf(float x, float *sin, float *cos) */
static void c_sincosf(Ctx *c) { float x = c->v[0].f[0]; *(float *)(uintptr_t)A0 = sinf(x); *(float *)(uintptr_t)A1 = cosf(x); }

/* nn::os::GetHostArgv: no arguments on a retail console; an argv that is only its terminator */
static uint64_t g_argv[1];
static void os_GetHostArgv(Ctx *c) { RET((uint64_t)(uintptr_t)g_argv); }

/* nn::hid::GetTouchScreenState<1>: docked, no touch can arrive: a state with no touches (0x38 bytes) */
static void hid_GetTouchScreenState1(Ctx *c) { memset((void *)(uintptr_t)A0, 0, 0x38); }
/* nn::hid::GetNpadControllerColor: hid 604 (0x4b8ca), "no controller of that kind": the game keeps its default colours */
static void hid_NoControllerColor(Ctx *c) { RET(0x4b8ca); }
/* nn::oe::GetExpectedVolumeBalance(float *main, float *library applet) */
static void oe_GetExpectedVolumeBalance(Ctx *c) { *(float *)(uintptr_t)A0 = 1.0f; *(float *)(uintptr_t)A1 = 1.0f; }
static void c_isalnum(Ctx *c) { RET(isalnum((int)A0) != 0); }
static void c_isxdigit(Ctx *c) { RET(isxdigit((int)A0) != 0); }
static void c_strpbrk(Ctx *c) { RET((uint64_t)(uintptr_t)strpbrk((const char *)(uintptr_t)A0, (const char *)(uintptr_t)A1)); }
static void c_puts(Ctx *c) { RET(puts((const char *)(uintptr_t)A0)); }
static void c_strtod(Ctx *c) { c->v[0].df[0] = strtod((const char *)(uintptr_t)A0, (char **)(uintptr_t)A1); }
static void c_memset_s(Ctx *c) { memset((void *)(uintptr_t)A0, (int)A2, A3 < A1 ? A3 : A1); RET(0); }   /* (dest, destsz, ch, count) */

/* nn::hid::detail::EnqueueVibrationValue(handle, value): sent at once by the runtime's SendVibrationValue (nnhid.c) */
static void hid_EnqueueVibrationValue(Ctx *c) {
    ImportFn send = import_lookup_any("_ZN2nn3hid18SendVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE");
    if (send) send(c);
    RET(0);                                                     /* Result: success (the SDK aborts on anything else) */
}

/* nn::hid::GetNpadState(NpadFullKeyState *, id): the runtime's own, then at player 1's Discord's turn (presence.c) and,
 * with LM3_DEBUG, the debug menu's (debug_menu.c) */
int lm3_debug_tick(Ctx *c, uint64_t pad); void lm3_debug_boot(void); int lm3_debug_on(void); void lm3_presence_tick(Ctx *c);
static ImportFn g_pad_state;
static void hid_GetNpadState_tick(Ctx *c) {
    uint64_t out = A0;
    uint32_t id = *(uint32_t *)(uintptr_t)A1;
    g_pad_state(c);
    if (id) return;
    lm3_presence_tick(c);
    if (lm3_debug_on() && lm3_debug_tick(c, *(uint64_t *)(uintptr_t)(out + 8))) memset((void *)(uintptr_t)(out + 8), 0, 0x18);      /* buttons and sticks, as nnhid.c pad_gate */
}

static const ImportDef lm3_imports[] = {
    {"_ZN2nn3hid6detail21EnqueueVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE", hid_EnqueueVibrationValue},
    {"_ZN2nn3hid6detail28FlushEnqueuedVibrationValuesEv", ret0},      /* Result: success */
    {"_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", hid_GetTouchScreenState1},
    {"_ZN2nn3hid21InitializeTouchScreenEv", nothing},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorERKj", hid_NoControllerColor},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorES2_RKj", hid_NoControllerColor},
    {"_ZN2nn3hid17StopSixAxisSensorERKNS0_19SixAxisSensorHandleE", nothing},
    {"_ZN2nn3hid30SetNpadJoyAssignmentModeSingleERKj", nothing},
    {"_ZN2nn3hid14DisconnectNpadERKj", nothing},
    /* notifications and performance settings change nothing on a PC */
    {"_ZN2nn2oe42SetOperationModeChangedNotificationEnabledEb", nothing},
    {"_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", nothing},
    {"_ZN2nn2oe28SetResumeNotificationEnabledEb", nothing},
    {"_ZN2nn2oe31EnterExitRequestHandlingSectionEv", nothing},
    {"_ZN2nn2oe31LeaveExitRequestHandlingSectionEv", nothing},
    {"_ZN2nn2oe27SetPerformanceConfigurationENS0_15PerformanceModeEi", nothing},
    {"_ZN2nn2oe24GetExpectedVolumeBalanceEPfS1_", oe_GetExpectedVolumeBalance},
    {"_ZN2nn2oe24SetExpectedVolumeBalanceEff", nothing},
    /* the GPU driver's own memory: the runtime is the driver here */
    {"_ZN2nn2vi16ListFrameHistoryEPNS0_9FrameInfoEiPKNS0_5LayerE", ret0},     /* no frame records kept: 0 entries (the game's frame-time statistics) */
    {"_ZN2nv18InitializeGraphicsEPvm", nothing},
    {"_ZN2nv20SetGraphicsAllocatorEPFPvmmS0_EPFvS0_S0_EPFS0_S0_mS0_ES0_", nothing},
    {"isalnum", c_isalnum}, {"isxdigit", c_isxdigit}, {"strpbrk", c_strpbrk}, {"puts", c_puts}, {"strtod", c_strtod}, {"memset_s", c_memset_s},
    {"sincosf", c_sincosf},
    {"_ZN2nn2os11GetHostArgcEv", ret0},
    {"_ZN2nn2os11GetHostArgvEv", os_GetHostArgv},
    /* the module list is for the game's crash reports: none */
    {"_ZN2nn4diag40GetRequiredBufferSizeForGetAllModuleInfoEv", ret0},
    {"_ZN2nn4diag16GetAllModuleInfoEPPNS0_10ModuleInfoEPvm", ret0},
    {"_ZN2nn4diag26SetAssertionFailureHandlerEPFNS0_25AssertionFailureOperationERKNS0_13AssertionInfoEE", nothing},
    {"_ZN2nn2os21SetDyingMessageRegionEmm", nothing},
    {"_ZN2nn2oe10InitializeEv", nothing},
    {"_ZN2nn2fs12SetAllocatorEPFPvmEPFvS1_mE", nothing},      /* the runtime's nn::fs allocates on the host */
    {"_ZN2nn3aoc16ListAddOnContentEPiii", ret0},              /* no add-on content installed */
    {"_ZN2nn6socket10InitializeEPvmmi", ret0},                /* Result success; the sockets themselves are not modelled yet */
};

/* graphics.ini frame_rate = 60 .. 120 and aspect_ratio: the two constants of main's read-only data that the 60 FPS and 21:9
 * mods patch, set from the options instead (boot.c, once the window is made (aspect_ratio = auto) and before the game starts).
 * At 60 and above the game's own frame limiter is also switched off (the longest it may wait, 350 ms, becomes 0): the
 * runtime's clock releases the game every frame (display.c), and with both waiting they took turns (53 fps in the lobby
 * at 60). The frame time itself stays the mod's value: other code reads it (halved, it made the save thumbnail four
 * times darker, 2026-11-12), so it is not the place to switch the limiter off.
 * The rest of each mod: the present interval (display.c display_set_interval) and the game's screen-shape function
 * (game_shim_eh.c h_aspect). A constant that is not the expected one (another build of main) is left alone. */
static void set_const(uint64_t off, float was, float now, const char *what) {
    float *p = (float *)(uintptr_t)(GUEST_BASE + off);
    if (*p != was) { printf("lm3: %s: main+0x%llx is %g, not %g: left alone\n", what, (unsigned long long)off, *p, was); return; }
    void *page = (void *)((uintptr_t)p & ~(uintptr_t)0xfff);      /* read-only by now (hostrt.c), unless HL_NO_PROTECT */
    plat_protect(page, 0x1000, PLAT_RW);
    *p = now;
    if (!getenv("HL_NO_PROTECT")) plat_protect(page, 0x1000, PLAT_RO);
    printf("lm3: %s: main+0x%llx %g -> %g\n", what, (unsigned long long)off, was, now);
}
/* Retail Luigi's Mansion 3 (v1.4.0) asset bug workaround (GitHub issue #67):
 * Luigi's Prologue handheld flashlight submesh (1091 verts, 1548 tris, all bound 100% to bone 25) is stored in
 * global.data (block #54, uncompressed 0x399a7a0..0x39b9630) with all 1091 base vertices collapsed to a ~0.1 mm point
 * at (-2.137, 0.179, 0.699) so the flashlight hides when Luigi equips the Poltergust (morph weight = 0). When Luigi
 * holds the flashlight in the Prologue (morph weight -> 1), 23 of its 24 connected components (1065 verts) expand to
 * the flashlight body in his hand (bind-space x in [-2.506, -2.122]), but one stray 26-vertex, 32-triangle component
 * (vertices 879..904, morph indices 880..905) has retail morph deltas around (+1.87, -0.18, -0.30), placing it 2.24 m
 * away at (-0.259, 0.030, 0.431), roughly 2.1 m below the floor and 0.75 m behind Luigi. Because it sits under the floor,
 * its back faces fail depth in the character pass, stamp the occluded-character stencil bit (0x04), expand Luigi's
 * skinned AABB downward, and cause SilhouettePost to draw a dark speck near the bottom of the screen whenever Luigi
 * moves near the top of a room before obtaining the Poltergust.
 *
 * The 1-morph-target CPU skinning job (main+0x4dffd0) is only invoked indirectly via its vtable slot (main+0x16be958).
 * Wrapping that slot lets us recognize the exact retail global.data morph delta buffer on its first skinning pass and
 * zero the position deltas of entries 880..905 in place so those 26 stray vertices remain collapsed at their 0.1 mm
 * base-mesh point inside the flashlight tail cap (matching the unequipped state). Set LM3_KEEP_FLASHLIGHT_SPECK=1 to
 * keep retail behavior. */
#define LM3_SKIN_MORPH_VTABLE_OFF 0x16be958ULL
#define LM3_SKIN_MORPH_FN_OFF     0x4dffd0ULL

static SRWLOCK g_fl_morph_lock = SRWLOCK_INIT;

static int lm3_fix_flashlight_morph_deltas(uint32_t *del) {
    /* Fast lock-free check on entry 880's X delta (+1.869874f = 0x3fef5809) before taking the lock. */
    if (*(const volatile uint32_t *)(del + 880 * 12) != 0x3fef5809u) return 0;
    int patched = 0;
    AcquireSRWLockExclusive(&g_fl_morph_lock);
    /* Verify the exact retail global.data fingerprint: zero sentinel del[0], body vert del[1], and stray endpoints del[880] & del[905]. */
    if (del[0] == 0u && del[1] == 0u && del[2] == 0u &&
        del[1 * 12 + 0] == 0xbeb9f278u && del[1 * 12 + 1] == 0xbd043e88u && del[1 * 12 + 2] == 0xbec0a425u &&
        del[880 * 12 + 0] == 0x3fef5809u && del[880 * 12 + 1] == 0xbe368f8fu && del[880 * 12 + 2] == 0xbe9a5ee7u &&
        del[905 * 12 + 0] == 0x3ff34580u && del[905 * 12 + 1] == 0xbe111441u && del[905 * 12 + 2] == 0xbe8c614eu) {
        /* Clear 881..905 first and 880 last so a concurrent lock-free reader never skips a partially patched buffer. */
        for (int i = 881; i <= 905; i++) {
            del[i * 12 + 0] = 0u;
            del[i * 12 + 1] = 0u;
            del[i * 12 + 2] = 0u;
        }
        del[880 * 12 + 1] = 0u;
        del[880 * 12 + 2] = 0u;
        *(volatile uint32_t *)(del + 880 * 12 + 0) = 0u;
        patched = 1;
    }
    ReleaseSRWLockExclusive(&g_fl_morph_lock);
    if (patched)
        printf("lm3: issue #67 workaround (retail global.data asset bug): collapsed 26 stray Prologue flashlight morph vertices (879..904)\n");
    return patched;
}

static void lm3_skin_morph_job(Ctx *c) {
    uint64_t arg = c->x[0];
    if (arg) {
        uint64_t job = *(const uint64_t *)(uintptr_t)(arg + 16);
        if (job) {
            const uint16_t *cnt = *(const uint16_t *const *)(uintptr_t)(job + 160);
            if (cnt && cnt[0] == 1091u && cnt[1] == 0u && cnt[2] == 0u && cnt[3] == 0u) {
                uint64_t morph = *(const uint64_t *)(uintptr_t)(job + 80);
                if (morph) {
                    const uint16_t *midx = *(const uint16_t *const *)(uintptr_t)(morph + 0);
                    uint32_t *del = *(uint32_t *const *)(uintptr_t)(morph + 8);
                    /* Verify midx[0..1090] maps through 1..1091 before indexing del[880..905], so any
                     * other 1091-vertex mesh with a smaller morph delta table is never read out of bounds. */
                    if (midx && del &&
                        midx[0] == 1u && midx[879] == 880u && midx[904] == 905u && midx[1090] == 1091u)
                        lm3_fix_flashlight_morph_deltas(del);
                }
            }
        }
    }
    call_guest(c, GUEST_BASE + LM3_SKIN_MORPH_FN_OFF);
}

static void lm3_hook_flashlight_morph_job(void) {
    if (getenv("LM3_KEEP_FLASHLIGHT_SPECK")) return;
    uint64_t *slot = (uint64_t *)(uintptr_t)(GUEST_BASE + LM3_SKIN_MORPH_VTABLE_OFF);
    uint64_t want = GUEST_BASE + LM3_SKIN_MORPH_FN_OFF;
    if (*slot != want) {
        printf("lm3: flashlight morph workaround: main+0x%llx is 0x%llx, not 0x%llx: left alone\n",
               (unsigned long long)LM3_SKIN_MORPH_VTABLE_OFF, (unsigned long long)*slot, (unsigned long long)want);
        return;
    }
    *slot = hostfn_address("lm3_skin_morph_job", lm3_skin_morph_job);
}

void lm3_image_options(void) {
    const char *e = getenv("RECOMPS_ASPECT");      /* gfx_options.c: aspect_ratio, when it is not 16:9 */
    float a = e ? (float)atof(e) : 0.0f;
    const char *ft = getenv("LM3_FRAME_MS");        /* test: the game's own frame time target, whatever frame_rate says */
    if (ft || gfx_options()->frame_rate > 30) set_const(0x13c94dc, 100.0f / 3.0f, ft ? (float)atof(ft) : 1000.0f / (float)gfx_options()->frame_rate, "frame_rate, the frame time in ms");
    if (gfx_options()->frame_rate > 30 && !getenv("LM3_KEEP_LIMITER")) set_const(0x13c8e64, 350.0f, 0.0f, "frame_rate, the longest wait of the game's own limiter in ms");
    if (a > 0.0f) set_const(0x13c96fc, 16.0f / 9.0f, a, "aspect_ratio, the screen's width / height");
    lm3_hook_flashlight_morph_job();
    lm3_debug_boot();
}

void register_lm3_imports(void) {
    register_imports(lm3_imports, (int)(sizeof lm3_imports / sizeof lm3_imports[0]));
    static const ImportDef pad = {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj", hid_GetNpadState_tick};
    if ((g_pad_state = import_lookup(pad.name))) register_imports(&pad, 1);
}
