/* runtime/mx_translate.c, mx_infer_int_tex: static SASS analysis of texture units consumed by integer ops.
 *   clang -O2 -Wall -I runtime tests/mx_infer_int_tex_test.c runtime/mx_translate.c -o build/mx_infer_int_tex_test */
#include <stdio.h>
#include <string.h>
#include "mx_translate.h"

static uint32_t run_infer(const uint64_t *insns, int nwords, uint32_t *sint_out) {
    uint8_t code[0x50 + 256];
    MxIn in;
    memset(code, 0, sizeof code);
    memset(&in, 0, sizeof in);
    memcpy(code + 0x50, insns, (size_t)nwords * 8);
    in.stage = MX_FRAGMENT;
    in.code = code;
    in.code_len = 0x50 + nwords * 8;
    return mx_infer_int_tex(&in, sint_out);
}

/* Helper encoders for Maxwell SASS words (matching runtime/mx_ops.h) */
static uint64_t enc_texs_2d(unsigned rd0, unsigned ra, unsigned rb, unsigned unit) {
    /* TEXS: 0xd800000000000000, enc=2 (bits 53..56: 2D lod 0), f32 (bit 59=1), swz=0 (.x), rd1=255 (bits 28..35), off=8+2*unit (bits 36..48) */
    uint64_t off = 8 + 2 * (uint64_t)unit;
    return 0xd8400000ff070000ull | (off << 36) | ((uint64_t)rb << 20) | ((uint64_t)ra << 8) | rd0;
}
static uint64_t enc_texs_shadow(unsigned rd0, unsigned ra, unsigned rb, unsigned unit) {
    /* TEXS enc=6 (2D shadow lod 0): bits 53..56 = 6 */
    uint64_t off = 8 + 2 * (uint64_t)unit;
    return 0xd8c00000ff070000ull | (off << 36) | ((uint64_t)rb << 20) | ((uint64_t)ra << 8) | rd0;
}
static uint64_t enc_fadd_reg(unsigned rd, unsigned ra, unsigned rb) {
    return 0x5c58000000070000ull | ((uint64_t)rb << 20) | ((uint64_t)ra << 8) | rd;
}
static uint64_t enc_iadd_reg(unsigned rd, unsigned ra, unsigned rb) {
    return 0x5c10000000070000ull | ((uint64_t)rb << 20) | ((uint64_t)ra << 8) | rd;
}
static uint64_t enc_mov_reg(unsigned rd, unsigned rb) {
    return 0x5c98078000070000ull | ((uint64_t)rb << 20) | rd;
}
static uint64_t enc_bfe_imm(unsigned rd, unsigned ra, int sgn) {
    /* BFE (imm): bit 48 = signed */
    return 0x3800000000070000ull | ((uint64_t)(sgn != 0) << 48) | ((uint64_t)ra << 8) | rd;
}
static uint64_t enc_i2f_reg(unsigned rd, unsigned rb, int sgn) {
    /* I2F (reg): bit 13 = signed */
    return 0x5cb8000000070000ull | ((uint64_t)(sgn != 0) << 13) | ((uint64_t)rb << 20) | rd;
}
static uint64_t enc_lop32i(unsigned rd, unsigned ra, uint32_t imm) {
    return 0x0400000000070000ull | ((uint64_t)imm << 20) | ((uint64_t)ra << 8) | rd;
}

int main(void) {
    uint32_t sint = 0, mask = 0;
    int ok = 1;

    /* 1. Pure float sample on unit 0 */
    {
        uint64_t w[4] = {0, enc_texs_2d(4, 0, 1, 0), enc_fadd_reg(4, 4, 2), 0xe30000000007000full};
        mask = run_infer(w, 4, &sint);
        if (mask != 0 || sint != 0) { printf("case 1 FAIL: mask=0x%x sint=0x%x\n", mask, sint); ok = 0; }
    }
    /* 2. Unsigned integer sample on unit 1, propagated through MOV + BFE(u) + IADD */
    {
        uint64_t w[8] = {0, enc_texs_2d(10, 0, 1, 1), enc_mov_reg(11, 10), enc_bfe_imm(12, 11, 0),
                         0, enc_iadd_reg(12, 12, 2), 0xe30000000007000full, 0x50b0000000070f00ull};
        mask = run_infer(w, 8, &sint);
        if (mask != (1u << 1) || sint != 0) { printf("case 2 FAIL: mask=0x%x sint=0x%x\n", mask, sint); ok = 0; }
    }
    /* 3. Signed integer sample on unit 2 via signed I2F */
    {
        uint64_t w[4] = {0, enc_texs_2d(6, 0, 1, 2), enc_i2f_reg(7, 6, 1), 0xe30000000007000full};
        mask = run_infer(w, 4, &sint);
        if (mask != (1u << 2) || sint != (1u << 2)) { printf("case 3 FAIL: mask=0x%x sint=0x%x\n", mask, sint); ok = 0; }
    }
    /* 4. Mixed shader: u0 float, u3 float abs-bit-masked via LOP32I 0x7fffffff, u5 uint, u6 shadow */
    {
        uint64_t w[12] = {
            0, enc_texs_2d(4, 0, 1, 0), enc_texs_2d(5, 0, 1, 3), enc_texs_2d(6, 0, 1, 5),
            0, enc_texs_shadow(7, 0, 1, 6), enc_fadd_reg(4, 4, 2), enc_lop32i(5, 5, 0x7fffffffu),
            0, enc_iadd_reg(6, 6, 2), enc_iadd_reg(7, 7, 2), 0xe30000000007000full
        };
        mask = run_infer(w, 12, &sint);
        if (mask != (1u << 5) || sint != 0) { printf("case 4 FAIL: mask=0x%x sint=0x%x\n", mask, sint); ok = 0; }
    }

    puts(ok ? "mx_infer_int_tex_test: ok" : "mx_infer_int_tex_test: FAILED");
    return !ok;
}
