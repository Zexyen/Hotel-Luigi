/* Host side of nn::hid devices: the one interface nnhid.c uses to read pads (nnpad_sdl.c implements it with SDL3).
 * Button numbers are the Pro Controller bits of nnhid.c (0 A, 1 B, 2 X, 3 Y, 4 StickL, 5 StickR, 6 L, 7 R, 8 ZL, 9 ZR,
 * 10 Plus, 11 Minus, 12 Left, 13 Up, 14 Right, 15 Down). Players are 0..7. */
#ifndef NNPAD_H
#define NNPAD_H
#include <stdint.h>

typedef struct {
    uint64_t buttons;           /* bits 0..15 only (the stick-as-button bits are derived by nnhid.c) */
    int lx, ly, rx, ry;         /* -32767..32767, up/right positive, dead zone and circular normalisation applied */
    int wired;                  /* 0 = bluetooth / wireless */
    int has_motion;             /* the device has a gyro and accelerometer */
    float accel[3];             /* in G, Nintendo axes (see nnpad_sdl.c: +x right, +y forward, +z up; gravity at rest is (0, 0, -1)) */
    float gyro[3];              /* in revolutions per second, same axes */
} NnPadState;

int  nnpad_button_prompts(void);                /* [Recomps#1] [general] button_prompts: 1 switch, 2 playstation, 3 xbox, 4 keyboard (auto resolved) */
void nnpad_prompts_used(int keyboard);          /* player 1 used a pad (0) or the keyboard / mouse (1): button_prompts = auto follows */
int  nnpad_prompts_by_letter(void);             /* [Recomps#1] finding 148: player 1's face buttons by printed letter (A = the bottom button) */
int  nnpad_menu_swapped(void);                  /* [witchtime] [general] swap_confirm, for the device in use: the pad's A and B change places (nnhid.c), prompts and panel follow */
int  nnpad_bind_get(unsigned char *out);        /* [lm3] [general] bind_controls: fills 16-byte Pro button permutation; 0 = game default */
int  nnpad_bind_set(const unsigned char *b);    /* [lm3] write bind_controls (NULL = game default) to controllers.ini; 0 = not written */
int  nnpad_bind_live(void);                     /* [lm3] 1 when a custom gameplay button layout is active for controllers */
const char *nnpad_deadzone_setting(void);       /* [lm3] [general] deadzone, formatted as written (e.g. "0.10") */
int  nnpad_deadzone_set(const char *val);       /* [lm3] write [general] deadzone (0.00..0.90) to controllers.ini and apply live */
const char *nnpad_invert_camera_x_setting(void);/* [lm3] [general] invert_camera_x ("on" / "off") */
int  nnpad_invert_camera_x_set(const char *val);/* [lm3] write [general] invert_camera_x to controllers.ini and apply live */
const char *nnpad_invert_camera_y_setting(void);/* [lm3] [general] invert_camera_y ("on" / "off") */
int  nnpad_invert_camera_y_set(const char *val);/* [lm3] write [general] invert_camera_y to controllers.ini and apply live */
int  nnpad_init(void);                           /* SDL up (no video, no window); 0 = failed. Also loads controllers.ini. */
void nnpad_shutdown(void);                       /* stop rumble and close open pads on exit */
void nnpad_poll(int force);                      /* hot-plug, config reload; cheap when called often (at most once per ms) */
int  nnpad_present(int player);                  /* a device is assigned to this player slot */
int  nnpad_get(int player, NnPadState *out);     /* 0 = none */
void nnpad_rumble(int player, float low, float high);   /* amplitudes 0..1 of the low and high band */
void nnpad_disconnect(int player);               /* DisconnectNpad: hidden until one of its buttons is pressed */
int  nnpad_keyboard_player(void);                /* 0..7, -1 = nobody; fixed once resolved (see controllers.ini) */
#define KB_N 25
int  nnpad_kb_vk(int idx);                       /* configured virtual key: 0..15 = Pro buttons, 16..19 = left stick left/up/right/down, 20..23 = right stick, 24 = walk; 0 = unbound */
int  nnpad_key_vk(const char *name);             /* one key name of controllers.ini (W, SPACE, LSHIFT, MOUSE1, ...) -> virtual key, 0 = unknown */
int  nnpad_tool(int argc, char **argv);          /* --pad-test / --pad-map <n> / HL_PAD_TEST: returns the exit code, or -1 if this is no tool request */

/* self-test hooks implemented by nnhid.c (they call the handlers the game calls) */
typedef struct {
    uint32_t (*style)(uint32_t id);
    void (*state)(uint32_t id, uint64_t *buttons, int32_t stick[4], uint32_t *attr);
    void (*vibrate)(uint32_t id, float low, float high);
    int (*six)(uint32_t id, float accel[3], float gyro[3], int *at_rest);
    void (*test_mode)(int on);
    uint32_t (*led)(uint32_t id);
    void (*kb)(int on);                          /* keyboard + mouse = player 1, driven by key() / mouse() instead of the real devices */
    void (*key)(int vk, int down);
    void (*mouse)(int dx, int dy);               /* raw mouse counts, as the window gathers them */
} NnHidHooks;
int nnpad_selftest(const NnHidHooks *h);         /* number of failed checks; only virtual devices count while it runs */
#endif
