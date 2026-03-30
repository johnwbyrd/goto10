// Trace the multiply for seed $73 $44 $94 $D2 $A0 step by step.
// Compare our implementation against a separate clean implementation.
#include <cstdio>
#include <cstdint>
#include <cstring>

// Our existing step function (from cycle_detect.cpp / c64rnd.cpp)
#include "c64rnd.h"

// A second, independent multiply implementation that tries to be
// maximally faithful to the 6502 source code.
struct Regs {
    uint8_t resho, resmoh, resmo, reslo;
    uint8_t facov;
    uint8_t bits;
};

static void dump_regs(const char* label, const Regs& r) {
    printf("  %-12s RESHO=%02X RESMOH=%02X RESMO=%02X RESLO=%02X FACOV=%02X\n",
           label, r.resho, r.resmoh, r.resmo, r.reslo, r.facov);
}

// MULSHF: X = RESHO-1, then SHFTR2
static void mulshf(Regs& r) {
    // SHFTR2: shift result right 8 bits
    // LDY 4,X -> RESLO; STY FACOV
    r.facov = r.reslo;
    // LDY 3,X -> RESMO; STY 4,X -> RESLO
    r.reslo = r.resmo;
    // LDY 2,X -> RESMOH; STY 3,X -> RESMO
    r.resmo = r.resmoh;
    // LDY 1,X -> RESHO; STY 2,X -> RESMOH
    r.resmoh = r.resho;
    // LDY BITS; STY 1,X -> RESHO
    r.resho = r.bits;
}

// Process one multiplier byte against ARG, accumulating into result.
// Faithful to the 6502 MLTPLY/MLTPL1 code.
static void mltply_byte(Regs& r, uint8_t mbyte,
                         uint8_t argho, uint8_t argmoh, uint8_t argmo, uint8_t arglo,
                         const char* name) {
    if (mbyte == 0) {
        mulshf(r);
        printf("  [%s] = $00 -> MULSHF\n", name);
        dump_regs("after MULSHF", r);
        return;
    }

    // MLTPL1: LSR A, ORA #$80
    uint8_t carry = mbyte & 1;  // LSR puts bit 0 into carry
    mbyte = (mbyte >> 1) | 0x80;  // LSR then ORA #$80

    printf("  [%s] = $%02X, after LSR|ORA: $%02X, initial carry=%d\n",
           name, (mbyte & 0x7F) | (carry ? 1 : 0), mbyte, carry);

    int bit_count = 0;
    for (;;) {
        // MLTPL2: TAY (save mbyte); BCC MLTPL3 (skip add if carry clear)
        if (carry) {
            // CLC; ADC chain: RESLO += ARGLO, RESMO += ARGMO, etc.
            uint16_t sum;
            // CLC
            sum = (uint16_t)r.reslo + (uint16_t)arglo;
            r.reslo = sum & 0xFF;
            sum = (uint16_t)r.resmo + (uint16_t)argmo + (sum >> 8);
            r.resmo = sum & 0xFF;
            sum = (uint16_t)r.resmoh + (uint16_t)argmoh + (sum >> 8);
            r.resmoh = sum & 0xFF;
            sum = (uint16_t)r.resho + (uint16_t)argho + (sum >> 8);
            r.resho = sum & 0xFF;
            carry = (sum >> 8) & 1;
        } else {
            carry = 0;  // carry stays clear for the ROR
        }

        // MLTPL3: ROR RESHO, ROR RESMOH, ROR RESMO, ROR RESLO, ROR FACOV
        uint8_t nc;
        nc = r.resho & 1;  r.resho = (carry << 7) | (r.resho >> 1);  carry = nc;
        nc = r.resmoh & 1; r.resmoh = (carry << 7) | (r.resmoh >> 1); carry = nc;
        nc = r.resmo & 1;  r.resmo = (carry << 7) | (r.resmo >> 1);  carry = nc;
        nc = r.reslo & 1;  r.reslo = (carry << 7) | (r.reslo >> 1);  carry = nc;
        nc = r.facov & 1;  r.facov = (carry << 7) | (r.facov >> 1);  carry = nc;
        // carry out of FACOV is discarded

        // TYA; LSR A; BNE MLTPL2
        carry = mbyte & 1;
        mbyte >>= 1;
        bit_count++;

        if (mbyte == 0) break;
    }
    printf("  [%s] %d bits processed\n", name, bit_count);
    dump_regs("after byte", r);
}

int main() {
    // Input seed: $80 $34 $00 $00 $D1
    // After MOVFM: exp=$80, m1=$B4, m2=$00, m3=$00, m4=$D1, facov=0
    uint8_t fac_m1 = 0xB4;  // $34 | $80
    uint8_t fac_m2 = 0x00;
    uint8_t fac_m3 = 0x00;
    uint8_t fac_m4 = 0xD1;
    uint8_t fac_ov = 0x00;

    // Multiplier constant: $98 $35 $44 $7A $00
    uint8_t arg_ho = 0xB5;   // $35 | $80
    uint8_t arg_moh = 0x44;
    uint8_t arg_mo = 0x7A;
    uint8_t arg_lo = 0x00;

    printf("=== Reference implementation (faithful to 6502 source) ===\n\n");
    printf("FAC mantissa: %02X %02X %02X %02X, FACOV: %02X\n", fac_m1, fac_m2, fac_m3, fac_m4, fac_ov);
    printf("ARG mantissa: %02X %02X %02X %02X\n\n", arg_ho, arg_moh, arg_mo, arg_lo);

    Regs r = {0, 0, 0, 0, 0, 0};  // result cleared, BITS=0

    // Process 5 multiplier bytes: FACOV, FACLO, FACMO, FACMOH, FACHO
    mltply_byte(r, fac_ov,  arg_ho, arg_moh, arg_mo, arg_lo, "FACOV");
    mltply_byte(r, fac_m4,  arg_ho, arg_moh, arg_mo, arg_lo, "FACLO");
    mltply_byte(r, fac_m3,  arg_ho, arg_moh, arg_mo, arg_lo, "FACMO");
    mltply_byte(r, fac_m2,  arg_ho, arg_moh, arg_mo, arg_lo, "FACMOH");
    mltply_byte(r, fac_m1,  arg_ho, arg_moh, arg_mo, arg_lo, "FACHO");

    printf("\nResult: RESHO=%02X RESMOH=%02X RESMO=%02X RESLO=%02X FACOV=%02X\n",
           r.resho, r.resmoh, r.resmo, r.reslo, r.facov);

    // Now do MOVFR + NORMAL (post-multiply normalize)
    // New exponent: $80 + $98 - $80 = $98
    uint8_t exp = 0x98;
    uint8_t m1 = r.resho, m2 = r.resmoh, m3 = r.resmo, m4 = r.reslo;
    uint8_t ov = r.facov;

    printf("\nPre-normalize: exp=%02X m=%02X %02X %02X %02X ov=%02X\n", exp, m1, m2, m3, m4, ov);

    // Normalize
    while (m1 == 0) {
        exp -= 8; m1 = m2; m2 = m3; m3 = m4; m4 = ov; ov = 0;
    }
    while (!(m1 & 0x80)) {
        exp--;
        uint8_t c = ov >> 7; ov <<= 1;
        uint8_t c1 = m4 >> 7; m4 = (m4 << 1) | c;
        uint8_t c2 = m3 >> 7; m3 = (m3 << 1) | c1;
        uint8_t c3 = m2 >> 7; m2 = (m2 << 1) | c2;
        m1 = (m1 << 1) | c3;
    }
    printf("Post-normalize: exp=%02X m=%02X %02X %02X %02X ov=%02X\n", exp, m1, m2, m3, m4, ov);

    // Byte swap
    uint8_t t;
    t = m1; m1 = m4; m4 = t;
    t = m2; m2 = m3; m3 = t;

    // Force [0,1): ov=exp, exp=$80, normalize again
    ov = exp; exp = 0x80;
    while (m1 == 0) {
        exp -= 8; m1 = m2; m2 = m3; m3 = m4; m4 = ov; ov = 0;
    }
    while (!(m1 & 0x80)) {
        exp--;
        uint8_t c = ov >> 7; ov <<= 1;
        uint8_t c1 = m4 >> 7; m4 = (m4 << 1) | c;
        uint8_t c2 = m3 >> 7; m3 = (m3 << 1) | c1;
        uint8_t c3 = m2 >> 7; m2 = (m2 << 1) | c2;
        m1 = (m1 << 1) | c3;
    }

    // Round
    if (ov & 0x80) {
        m4++; if (m4 == 0) { m3++; if (m3 == 0) { m2++; if (m2 == 0) {
            m1++; if (m1 == 0) { m1 = 0x80; exp++; }
        }}}
    }

    // Pack
    uint8_t seed1 = m1 & 0x7F;  // clear implicit bit, sign=0
    printf("\nFinal seed: %02X %02X %02X %02X %02X\n", exp, seed1, m2, m3, m4);
    printf("Expected:   7E 0D 33 A3 FA\n");

    // Also run through C64Rnd for comparison
    printf("\n=== C64Rnd implementation ===\n");
    C64Rnd rng({0x80, 0x34, 0x00, 0x00, 0xD1});
    rng.next();
    auto s = rng.seed();
    printf("C64Rnd:     %02X %02X %02X %02X %02X\n", s[0], s[1], s[2], s[3], s[4]);

    return 0;
}
