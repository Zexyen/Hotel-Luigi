/* [lm3] Gameplay controller button rebinding (Luigi's Mansion 3).
 *
 * A custom layout is a bijection r[16] over Nintendo Pro Controller buttons:
 *   r[b] = physical Pro button index that triggers logical gameplay button b.
 * Menus keep their own buttons (and swap_confirm); only gameplay input and
 * gameplay button prompts follow r[]. Keyboard & mouse bindings stay in
 * [keyboard] of controllers.ini, so r[] only applies to controllers.
 *
 * Pro button numbering matches nnpad.h / nnhid.c:
 *   0 A, 1 B, 2 X, 3 Y, 4 STICK_L, 5 STICK_R, 6 L, 7 R, 8 ZL, 9 ZR,
 *   10 PLUS, 11 MINUS, 12 LEFT, 13 UP, 14 RIGHT, 15 DOWN. */
#ifndef HOTELLUIGI_REBIND_H
#define HOTELLUIGI_REBIND_H

#include <stdint.h>

enum {
    RB_A = 0, RB_B, RB_X, RB_Y,
    RB_STICK_L, RB_STICK_R,
    RB_L, RB_R, RB_ZL, RB_ZR,
    RB_PLUS, RB_MINUS,
    RB_LEFT, RB_UP, RB_RIGHT, RB_DOWN
};

/* Check that r[0..15] is a valid permutation of 0..15. */
static inline int rb_valid(const uint8_t r[16]) {
    unsigned seen = 0;
    for (int i = 0; i < 16; i++) {
        if (r[i] >= 16) return 0;
        seen |= 1u << r[i];
    }
    return seen == 0xffffu;
}

/* Fill out[0..15] with the default identity layout. */
static inline void rb_default(uint8_t out[16]) {
    for (int i = 0; i < 16; i++) out[i] = (uint8_t)i;
}

/* Return 1 if r[0..15] is the identity layout, else 0. */
static inline int rb_is_default(const uint8_t r[16]) {
    for (int i = 0; i < 16; i++) if (r[i] != (uint8_t)i) return 0;
    return 1;
}

/* Assign physical button `to` to logical slot `b`, swapping with whichever slot
 * currently holds `to` so r[] remains a valid permutation. */
static inline void rb_swap(uint8_t r[16], int b, uint8_t to) {
    if (b < 0 || b >= 16 || to >= 16 || r[b] == to) return;
    for (int i = 0; i < 16; i++) {
        if (r[i] == to) { r[i] = r[b]; break; }
    }
    r[b] = to;
}

/* Remap physical Pro Controller button bits (0..15) into logical gameplay button bits.
 * Higher bits (stick-as-button bits 16..23) pass through unchanged. */
static inline uint64_t rb_remap_buttons(const uint8_t r[16], uint64_t phys) {
    uint64_t out = phys & ~0xffffull;
    for (int b = 0; b < 16; b++) {
        if ((phys >> r[b]) & 1ull) out |= 1ull << b;
    }
    return out;
}

/* Inverse of rb_remap_buttons: logical gameplay bits -> physical Pro Controller bits. */
static inline uint64_t rb_to_pad_buttons(const uint8_t r[16], uint64_t logical) {
    uint64_t out = logical & ~0xffffull;
    for (int b = 0; b < 16; b++) {
        if ((logical >> b) & 1ull) out |= 1ull << r[b];
    }
    return out;
}

/* Return the physical Pro button index (0..15) bound to logical button `b`. */
static inline int rb_physical_of(const uint8_t r[16], int b) {
    return b >= 0 && b < 16 ? (int)r[b] : b;
}

#endif
