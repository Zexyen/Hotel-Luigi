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

static void ret1(Ctx *c) { RET(1); }
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

static const ImportDef lm3_imports[] = {
    {"_ZN2nn3hid6detail21EnqueueVibrationValueERKNS0_21VibrationDeviceHandleERKNS0_14VibrationValueE", hid_EnqueueVibrationValue},
    {"_ZN2nn3hid6detail28FlushEnqueuedVibrationValuesEv", ret0},      /* Result: success */
    {"_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", hid_GetTouchScreenState1},
    {"_ZN2nn3hid21InitializeTouchScreenEv", nothing},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorERKj", hid_NoControllerColor},
    {"_ZN2nn3hid22GetNpadControllerColorEPNS0_19NpadControllerColorES2_RKj", hid_NoControllerColor},
    {"_ZN2nn3hid17StopSixAxisSensorERKNS0_19SixAxisSensorHandleE", nothing},
    {"_ZN2nn3hid21IsSixAxisSensorAtRestERKNS0_19SixAxisSensorHandleE", ret1},     /* a pad lying still */
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
    void *page = (void *)((uintptr_t)p & ~(uintptr_t)0xfff);      /* read-only by now (hostrt.c), unless WT_NO_PROTECT */
    plat_protect(page, 0x1000, PLAT_RW);
    *p = now;
    if (!getenv("WT_NO_PROTECT")) plat_protect(page, 0x1000, PLAT_RO);
    printf("lm3: %s: main+0x%llx %g -> %g\n", what, (unsigned long long)off, was, now);
}
void lm3_image_options(void) {
    const char *e = getenv("RECOMPS_ASPECT");      /* gfx_options.c: aspect_ratio, when it is not 16:9 */
    float a = e ? (float)atof(e) : 0.0f;
    const char *ft = getenv("LM3_FRAME_MS");        /* test: the game's own frame time target, whatever frame_rate says */
    if (ft || gfx_options()->frame_rate >= 60) set_const(0x13c94dc, 100.0f / 3.0f, ft ? (float)atof(ft) : 1000.0f / (float)gfx_options()->frame_rate, "frame_rate, the frame time in ms");
    if (gfx_options()->frame_rate >= 60 && !getenv("LM3_KEEP_LIMITER")) set_const(0x13c8e64, 350.0f, 0.0f, "frame_rate, the longest wait of the game's own limiter in ms");
    if (a > 0.0f) set_const(0x13c96fc, 16.0f / 9.0f, a, "aspect_ratio, the screen's width / height");
}

void register_lm3_imports(void) { register_imports(lm3_imports, (int)(sizeof lm3_imports / sizeof lm3_imports[0])); }
