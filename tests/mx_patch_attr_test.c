/* runtime/mx_translate.c, tessellation generic patch attributes (issue #61): AST.P / ALD.P at 0x20 + 16k + 4c (p = 8..127)
 * map to layout(location = k) patch out/in vec4 patch_attrK[c], matching GLSLC's location assignment. Also checks that TCS does
 * not race-initialise patch_attrK per invocation, and that out-of-range or overlapping locations fail cleanly.
 *   clang -O2 -Wall -I runtime tests/mx_patch_attr_test.c runtime/mx_translate.c -o build/mx_patch_attr_test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mx_translate.h"

static uint64_t mk_ast(unsigned addr, unsigned reg, int patch) {
    return 0xeff0000000000000ull | (7ull << 16) | (255ull << 8) | ((uint64_t)addr << 20) | ((uint64_t)(patch ? 1 : 0) << 31) | reg;
}
static uint64_t mk_ald(unsigned addr, unsigned reg, int patch, int out) {
    return 0xefd8000000000000ull | (7ull << 16) | (255ull << 8) | (255ull << 39) | ((uint64_t)addr << 20) | ((uint64_t)(patch ? 1 : 0) << 31) | ((uint64_t)(out ? 1 : 0) << 32) | reg;
}
static uint64_t mk_exit(void) { return 0xe30000000000000full | (7ull << 16); }
static uint64_t mk_nop(void) { return 0x50b0000000000f00ull | (7ull << 16); }

static MxOut run_stage(int stage, const uint64_t *insns, int nwords) {
    uint8_t code[0x50 + 64];
    MxIn in; MxOut out;
    memset(code, 0, sizeof code);
    memset(&in, 0, sizeof in); memset(&out, 0, sizeof out);
    if (stage == MX_TESS_CONTROL) code[0x0b] = 3;
    memcpy(code + 0x50, insns, (size_t)nwords * 8);
    in.stage = stage; in.code = code; in.code_len = 0x50 + nwords * 8; in.target = 1;
    in.tess_prim = 4; in.tess_spacing = 0; in.tess_order = 0x901;
    mx_translate(&in, &out);
    return out;
}

int main(void) {
    int ok = 1;
    /* 1. TCS: write out_attr11.x (0x130), AST.P to 0xe0 (patch_attr12[0]) and 0x178 (patch_attr21[2]), ALD.P.O from 0xe0, outer[0] (0x00) */
    uint64_t tcs_w[8] = {
        0, mk_ast(0x130, 1, 0), mk_ast(0xe0, 2, 1), mk_ast(0x178, 3, 1),
        0, mk_ald(0xe0, 4, 1, 1), mk_ast(0x00, 4, 1), mk_exit()
    };
    MxOut tcs = run_stage(MX_TESS_CONTROL, tcs_w, 8);
    if (!tcs.glsl) { printf("tcs failed: %s %s\n", tcs.op, tcs.why); return 1; }
    ok = ok && strstr(tcs.glsl, "layout(location = 11) out vec4 out_attr11[];") != NULL;
    ok = ok && strstr(tcs.glsl, "layout(location = 12) patch out vec4 patch_attr12;") != NULL;
    ok = ok && strstr(tcs.glsl, "layout(location = 21) patch out vec4 patch_attr21;") != NULL;
    ok = ok && strstr(tcs.glsl, "patch_attr12[0] = F(r2);") != NULL;
    ok = ok && strstr(tcs.glsl, "patch_attr21[2] = F(r3);") != NULL;
    ok = ok && strstr(tcs.glsl, "r4 = U(patch_attr12[0]);") != NULL;
    ok = ok && strstr(tcs.glsl, "gl_TessLevelOuter[0] = F(r4);") != NULL;
    /* patch_attr must not be initialised per-invocation in main() */
    ok = ok && strstr(tcs.glsl, "patch_attr12 = vec4") == NULL;
    if (!ok) puts(tcs.glsl);
    free(tcs.glsl);

    /* 2. TES: read in_attr11.x (0x130), ALD.P from 0xe0 (patch_attr12[0]) and 0x178 (patch_attr21[2]) */
    uint64_t tes_w[4] = { 0, mk_ald(0x130, 1, 0, 0), mk_ald(0xe0, 2, 1, 0), mk_ald(0x178, 3, 1, 0) };
    uint64_t tes_w2[8] = { 0, tes_w[1], tes_w[2], tes_w[3], 0, mk_ast(0x70, 2, 0), mk_exit(), mk_nop() };
    MxOut tes = run_stage(MX_TESS_EVAL, tes_w2, 8);
    if (!tes.glsl) { printf("tes failed: %s %s\n", tes.op, tes.why); return 1; }
    ok = ok && strstr(tes.glsl, "layout(location = 11) in vec4 in_attr11[];") != NULL;
    ok = ok && strstr(tes.glsl, "layout(location = 12) patch in vec4 patch_attr12;") != NULL;
    ok = ok && strstr(tes.glsl, "layout(location = 21) patch in vec4 patch_attr21;") != NULL;
    ok = ok && strstr(tes.glsl, "r2 = U(patch_attr12[0]);") != NULL;
    ok = ok && strstr(tes.glsl, "r3 = U(patch_attr21[2]);") != NULL;
    if (!ok) puts(tes.glsl);
    free(tes.glsl);

    /* 3. Header padding (0x18, p=6) and out-of-range (0x200, p=128) must fail cleanly */
    uint64_t bad_pad[4] = { 0, mk_ast(0x18, 1, 1), mk_exit(), mk_nop() };
    MxOut o_pad = run_stage(MX_TESS_CONTROL, bad_pad, 4);
    ok = ok && !o_pad.glsl;
    free(o_pad.glsl);

    uint64_t bad_hi[4] = { 0, mk_ast(0x200, 1, 1), mk_exit(), mk_nop() };
    MxOut o_hi = run_stage(MX_TESS_CONTROL, bad_hi, 4);
    ok = ok && !o_hi.glsl;
    free(o_hi.glsl);

    /* 4. Overlapping per-vertex out_attr12 (0x140) and patch_attr12 (0xe0) must fail cleanly */
    uint64_t bad_ov[4] = { 0, mk_ast(0x140, 1, 0), mk_ast(0xe0, 2, 1), mk_exit() };
    MxOut o_ov = run_stage(MX_TESS_CONTROL, bad_ov, 4);
    ok = ok && !o_ov.glsl && strstr(o_ov.why, "overlaps") != NULL;
    free(o_ov.glsl);

    puts(ok ? "mx_patch_attr_test: ok" : "mx_patch_attr_test: FAILED");
    return !ok;
}
