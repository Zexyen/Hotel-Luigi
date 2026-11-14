/* The SIMD forms Luigi's Mansion 3's main needed on top of the lifter's (neon.h, rt_ext.h), against values worked by hand
 * from the Arm ARM pseudocode.
 *   clang -O2 -I runtime tests/neon_lm3_test.c -lm -o neon_lm3_test && ./neon_lm3_test */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "runtime.h"
#include "rt_ext.h"

int main(void) {
    V128 a, b, r;
    vzero(&a); vzero(&b);
    a.f[0] = 1; a.f[1] = -2; a.f[2] = 9; a.f[3] = 9; b.f[0] = 5; b.f[1] = 7;
    v_f3v(&r, &a, &b, F_MAXP, 0, 2); assert(r.f[0] == 1 && r.f[1] == 7 && r.d[1] == 0);        /* fmaxp v.2s */
    v_f3v(&r, &a, &b, F_MINP, 0, 2); assert(r.f[0] == -2 && r.f[1] == 5);                       /* fminp v.2s */
    a.f[0] = NAN; v_f3v(&r, &a, &b, F_MAXP, 0, 2); assert(isnan(r.f[0]));                        /* FPMax propagates NaN */

    a.f[0] = INFINITY; a.f[1] = 0; b.f[0] = 0; b.f[1] = -INFINITY;                               /* frecps / frsqrts: inf x 0 */
    v_f3v(&r, &a, &b, F_RECPS, 0, 2); assert(r.f[0] == 2.0f && r.f[1] == 2.0f);
    v_f3v(&r, &a, &b, F_RSQRTS, 0, 2); assert(r.f[0] == 1.5f && r.f[1] == 1.5f);
    a.f[0] = 3; b.f[0] = 0.25f; v_f3v(&r, &a, &b, F_RECPS, 0, 2); assert(r.f[0] == 1.25f);

    {   /* msr FPCR with FZ: a denormal product is 0, so Havok's length of a tiny vector is 0 and not infinite */
        volatile float t = 1.98e-20f;
        guest_set_fpcr(GUEST_FPCR_FZ); assert(guest_fpcr() == GUEST_FPCR_FZ); assert(t * t == 0);
        guest_set_fpcr(0); assert(guest_fpcr() == 0); assert(t * t != 0);
    }

    a.d[0] = 0x80000000fffffffbull; a.d[1] = 0x0000000700000000ull;                              /* -5, INT_MIN, 0, 7 */
    v_i1v(&r, &a, I1_CMGT0, 2, 4); assert(r.d[0] == 0 && r.d[1] == 0xffffffff00000000ull);
    v_i1v(&r, &a, I1_CMGE0, 2, 4); assert(r.d[0] == 0 && r.d[1] == ~0ull);
    v_i1v(&r, &a, I1_CMLE0, 2, 4); assert(r.d[0] == ~0ull && r.d[1] == 0x00000000ffffffffull);
    v_i1v(&r, &a, I1_CMLT0, 2, 4); assert(r.d[0] == ~0ull && r.d[1] == 0);
    v_i1v(&r, &a, I1_CLZ, 2, 4); assert(r.d[0] == 0 && r.d[1] == 0x0000001d00000020ull);        /* clz: 0, 0, 32, 29 */

    a.d[0] = 0x00000003fffffffeull; b.d[0] = 0x0000000900000008ull;                              /* (-2, 3), (8, 9) */
    v_maxminp(&r, &a, &b, 2, 0, 1, 0); assert(r.d[0] == 0x00000009fffffffeull && r.d[1] == 0);  /* umaxp v.2s */
    v_maxminp(&r, &a, &b, 2, 0, 0, 1); assert(r.d[0] == 0x00000008fffffffeull);                 /* sminp v.2s */

    a.d[0] = 0x0100ffff007f8000ull; a.d[1] = 0;                                                  /* 16-bit: -32768, 127, -1, 256 */
    v_sqxtun(&r, &a, 0, 0); assert(r.d[0] == 0xff007f00ull && r.d[1] == 0);                      /* sqxtun v.8b: 0, 127, 0, 255 */

    a.d[0] = 0xffffffff00000003ull; a.d[1] = 0x5555555555555555ull;
    v_mull_elem(&r, &a, 0x10, 2, 1, 0); assert(r.d[0] == 0x30 && r.d[1] == 0xffffffff0ull);     /* umull v.2d, v.2s, v.s[i] */
    puts("neon_lm3_test: ok");
    return 0;
}
