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

void register_lm3_imports(void) { register_imports(lm3_imports, (int)(sizeof lm3_imports / sizeof lm3_imports[0])); }
