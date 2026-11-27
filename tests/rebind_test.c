/* Standalone unit test for Hotel Luigi gameplay controller button rebinding (runtime/rebind.h).
 * Build & run:
 *   clang -O2 -Wall -Wextra -I runtime tests/rebind_test.c -o /tmp/rebind_test && /tmp/rebind_test */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rebind.h"

static int g_pass, g_fail;
static void check(int ok, const char *what) {
    if (ok) g_pass++;
    else { g_fail++; printf("  FAIL: %s\n", what); }
}

int main(void) {
    uint8_t r[16];

    /* 1. Default layout is valid identity */
    rb_default(r);
    check(rb_valid(r) && rb_is_default(r), "default layout is valid identity");
    check(rb_remap_buttons(r, 0x00ff0a55ull) == 0x00ff0a55ull, "identity layout leaves buttons and stick bits unchanged");

    /* 2. rb_valid rejects duplicates and out-of-range entries */
    rb_default(r);
    r[RB_X] = r[RB_Y];
    check(!rb_valid(r), "rb_valid rejects duplicate button");
    rb_default(r);
    r[RB_A] = 16;
    check(!rb_valid(r), "rb_valid rejects out-of-range button");

    /* 3. rb_swap preserves bijection and swaps with previous owner */
    rb_default(r);
    rb_swap(r, RB_A, RB_R);        /* Strobulb (A) -> R, Strobulb alt (R) -> A */
    check(rb_valid(r) && !rb_is_default(r) && r[RB_A] == RB_R && r[RB_R] == RB_A,
          "rb_swap swaps A and R cleanly and marks non-default");

    /* 4. Multi-swap layout round-trips every Pro button 0..15, chords, and preserves stick bits 16..23 */
    {
        rb_default(r);
        rb_swap(r, RB_A, RB_R);
        rb_swap(r, RB_X, RB_Y);
        rb_swap(r, RB_ZL, RB_L);
        rb_swap(r, RB_STICK_R, RB_B);
        rb_swap(r, RB_PLUS, RB_MINUS);
        check(rb_valid(r), "custom multi-swap layout is valid");

        int all_ok = 1;
        for (int b = 0; b < 16; b++) {
            uint64_t phys = 1ull << rb_physical_of(r, b);
            uint64_t expect = 1ull << b;
            uint64_t got = rb_remap_buttons(r, phys);
            uint64_t back = rb_to_pad_buttons(r, got);
            if (got != expect || back != phys) all_ok = 0;
        }
        check(all_ok, "rb_remap_buttons / rb_to_pad_buttons round-trip every button 0..15");

        uint64_t sticks = 0x00a50000ull;
        uint64_t chord = sticks | (1ull << RB_R) | (1ull << RB_L) | (1ull << RB_B);
        uint64_t out = rb_remap_buttons(r, chord);
        check(out == (sticks | (1ull << RB_A) | (1ull << RB_ZL) | (1ull << RB_STICK_R)),
              "rb_remap_buttons preserves stick-as-button bits and remaps chords");
        check(rb_to_pad_buttons(r, out) == chord, "rb_to_pad_buttons inverts chord with stick bits");
    }

    /* 5. Swapping back to identity restores rb_is_default */
    rb_default(r);
    rb_swap(r, RB_ZR, RB_R);
    rb_swap(r, RB_ZR, RB_ZR);
    check(rb_valid(r) && rb_is_default(r), "swapping back restores identity default");

    printf("rebind_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
