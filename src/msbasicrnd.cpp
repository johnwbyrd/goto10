#include "c64rnd.h"

// --- MOVFM: unpack 5-byte MBF from memory into FAC (code20 line 11) ---
// FACOV is set to 0 by STY FACOV where Y=0 (code20 line 30).
C64Rnd::FAC C64Rnd::movfm(const uint8_t* mem) {
    FAC f{};
    f.exp  = mem[0];
    f.m1   = mem[1] | 0x80;
    f.m2   = mem[2];
    f.m3   = mem[3];
    f.m4   = mem[4];
    f.sign = mem[1] & 0x80;
    f.ov   = 0;
    return f;
}

// --- MOVMF: round FAC and pack into 5-byte MBF (code20 line 39) ---
// ROUND (code22 line 59): ASL FACOV, if carry set then INCFAC.
// INCFAC (code18 line 62): increment mantissa from LSB with carry chain.
// If m1 overflows to 0: RNDSHF sets m1=$80 and increments exponent.
void C64Rnd::movmf(const FAC& fac, uint8_t* mem) {
    uint8_t m1 = fac.m1, m2 = fac.m2, m3 = fac.m3, m4 = fac.m4;
    uint8_t exp = fac.exp;

    if (exp == 0) {
        mem[0] = mem[1] = mem[2] = mem[3] = mem[4] = 0;
        return;
    }

    if (fac.ov & 0x80) {
        m4++;
        if (m4 == 0) { m3++; if (m3 == 0) { m2++; if (m2 == 0) {
            m1++; if (m1 == 0) { m1 = 0x80; exp++; }
        }}}
    }

    mem[0] = exp;
    mem[1] = (m1 & 0x7F) | (fac.sign & 0x80);
    mem[2] = m2;
    mem[3] = m3;
    mem[4] = m4;
}

// --- 6502 helper: one bit shift right via SHFTR3/SHFTR4/ROLSHF ---
// SHFTR3: ASL 1,X; BCC SHFTR4; INC 1,X
// SHFTR4: ROR 1,X; ROR 1,X
// ROLSHF: ROR 2,X; ROR 3,X; ROR 4,X; ROR A
// The ASL+INC+ROR+ROR sequence is an arithmetic right shift of byte 1,X
// that preserves bit 7 (the implicit leading 1).
static void shiftr_one_bit(uint8_t& b1, uint8_t& b2, uint8_t& b3, uint8_t& b4, uint8_t& a) {
    uint8_t c = (b1 >> 7) & 1;  // ASL: old bit 7 -> carry
    b1 <<= 1;                    // ASL: shift left
    if (c) b1++;                  // BCC/INC: restore bit 0 if carry was set
    // First ROR: carry (from ASL) into bit 7, bit 0 out
    uint8_t nc = b1 & 1; b1 = (c << 7) | (b1 >> 1); c = nc;
    // Second ROR
    nc = b1 & 1; b1 = (c << 7) | (b1 >> 1); c = nc;
    // ROLSHF: ROR chain through remaining bytes and A register
    nc = b2 & 1; b2 = (c << 7) | (b2 >> 1); c = nc;
    nc = b3 & 1; b3 = (c << 7) | (b3 >> 1); c = nc;
    nc = b4 & 1; b4 = (c << 7) | (b4 >> 1); c = nc;
    nc = a & 1;  a  = (c << 7) | (a >> 1);
}

// --- FMULT: multiply FAC by 5-byte MBF constant (code19 line 30) ---
void C64Rnd::fmult(FAC& fac, const uint8_t* mem) {
    if (fac.exp == 0) return;

    // CONUPK (code19 line 78): unpack constant into ARG
    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) { fac.exp = 0; fac.m1 = fac.m2 = fac.m3 = fac.m4 = fac.ov = 0; return; }
    uint8_t arg_m1 = mem[1] | 0x80;
    uint8_t arg_m2 = mem[2];
    uint8_t arg_m3 = mem[3];
    uint8_t arg_m4 = mem[4];
    uint8_t arg_sign = mem[1] & 0x80;

    // MULDIV (code19 line 102): combine exponents.
    // Both code paths (sum >= 256 and sum < 256) reach ADC #$80 with carry = 0.
    // The carry from that ADC persists into the multiply loop.
    uint16_t exp_sum = (uint16_t)fac.exp + (uint16_t)arg_exp;
    uint8_t low = exp_sum & 0xFF;
    uint16_t biased = (uint16_t)low + 0x80;  // ADC #$80 with C=0
    uint8_t new_exp = biased & 0xFF;
    if (new_exp == 0) { fac.exp = 0; fac.m1 = fac.m2 = fac.m3 = fac.m4 = fac.ov = 0; return; }

    fac.sign = (fac.sign ^ arg_sign) & 0x80;
    uint8_t carry = (biased >> 8) & 1;  // carry from MULDIV's ADC #$80

    // Clear result registers (code19 lines 34-38). STA does not affect carry.
    uint8_t res1 = 0, res2 = 0, res3 = 0, res4 = 0, res_ov = 0;

    // Multiply loop: process 5 bytes FACOV, FACLO, FACMO, FACMOH, FACHO.
    // LDA does not affect carry. Carry persists across byte boundaries.
    uint8_t mul_bytes[5] = { fac.ov, fac.m4, fac.m3, fac.m2, fac.m1 };

    for (int i = 0; i < 5; i++) {
        uint8_t mbyte = mul_bytes[i];

        if (mbyte == 0) {
            // MULSHF (code18 line 85): byte shift, then fall into SHIFTR.
            res_ov = res4; res4 = res3; res3 = res2; res2 = res1; res1 = 0;

            // SHIFTR (code18 line 96): ADC #8 using persisted carry.
            // A=0 (the zero byte). Result is 8+carry = 8 or 9.
            uint16_t sa = (uint16_t)0 + 8 + carry;
            uint8_t a = sa & 0xFF;
            carry = (sa >> 8) & 1;
            // a is 8 or 9 — positive, nonzero. BMI/BEQ not taken.
            // SBC #8: 6502 SBC = A - 8 - (1-C) = A - 9 + C.
            int sbc = (int)a - 9 + carry;
            uint8_t y = (uint8_t)(sbc & 0xFF);
            carry = (sbc >= 0) ? 1 : 0;

            uint8_t a_reg = res_ov;
            if (carry) {
                // BCS SHFTRT: no extra bit shift.
                carry = 0;  // SHFTRT: CLC
            } else {
                // Bit shift loop: y iterations (INY; BNE). y=$FF = one iteration.
                while (y != 0) {
                    shiftr_one_bit(res1, res2, res3, res4, a_reg);
                    y++;
                }
                carry = 0;  // SHFTRT: CLC
            }
            res_ov = a_reg;
            continue;
        }

        // Nonzero byte: MLTPL1 (code19 line 52). LSR A; ORA #$80.
        carry = mbyte & 1;
        mbyte = (mbyte >> 1) | 0x80;

        for (;;) {
            if (carry) {
                // CLC; ADC chain: RESLO += ARGLO, RESMO += ARGMO, etc.
                uint16_t s;
                s = (uint16_t)res4 + arg_m4;           res4 = s & 0xFF;
                s = (uint16_t)res3 + arg_m3 + (s >> 8); res3 = s & 0xFF;
                s = (uint16_t)res2 + arg_m2 + (s >> 8); res2 = s & 0xFF;
                s = (uint16_t)res1 + arg_m1 + (s >> 8); res1 = s & 0xFF;
                carry = (s >> 8) & 1;
            } else {
                carry = 0;
            }

            // MLTPL3: ROR RESHO..RESLO, FACOV
            uint8_t nc;
            nc = res1 & 1;   res1   = (carry << 7) | (res1 >> 1);   carry = nc;
            nc = res2 & 1;   res2   = (carry << 7) | (res2 >> 1);   carry = nc;
            nc = res3 & 1;   res3   = (carry << 7) | (res3 >> 1);   carry = nc;
            nc = res4 & 1;   res4   = (carry << 7) | (res4 >> 1);   carry = nc;
            nc = res_ov & 1; res_ov = (carry << 7) | (res_ov >> 1);

            // TYA; LSR A: next multiplier bit into carry, check sentinel.
            carry = mbyte & 1;
            mbyte >>= 1;
            if (mbyte == 0) break;
        }
        // carry holds the last bit shifted out — persists to the next byte.
    }

    // MOVFR (code20 line 2): copy result to FAC, then normalize.
    fac.exp = new_exp;
    fac.m1 = res1; fac.m2 = res2; fac.m3 = res3; fac.m4 = res4; fac.ov = res_ov;
    normalize(fac);
}

// --- FADD: add 5-byte MBF constant to FAC (code17 line 99) ---
// Only the FAC > ARG same-sign path is implemented (sufficient for RND).
void C64Rnd::fadd(FAC& fac, const uint8_t* mem) {
    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) return;

    uint8_t arg_m1 = mem[1] | 0x80;
    uint8_t arg_m2 = mem[2];
    uint8_t arg_m3 = mem[3];
    uint8_t arg_m4 = mem[4];
    uint8_t arisgn = (mem[1] & 0x80) ^ (fac.sign & 0x80);

    if (fac.exp == 0) { fac = movfm(mem); return; }

    uint8_t oldov = fac.ov;  // code17 line 102-103: LDX FACOV; STX OLDOV
    int diff = (int)arg_exp - (int)fac.exp;  // SEC; SBC FACEXP

    if (diff == 0) {
        fac.ov = 0;
    } else if (diff < 0) {
        // FADDA (code17 line 121): ARG is smaller. Shift ARG right.
        fac.ov = 0;  // STY FACOV where Y=0

        // FADD1 (code17 line 123): CMP #$F9. For large shifts, BMI taken → FADD5 → SHIFTR.
        // Carry entering SHIFTR: from CMP #$F9 with A < $F9 → carry clear.
        int8_t a = (int8_t)diff;
        uint8_t carry = 0;

        // SHIFTR loop (code18 line 96): ADC #8, byte shift while negative/zero.
        for (;;) {
            uint16_t sa = (uint16_t)(uint8_t)a + 8 + carry;
            a = (int8_t)(sa & 0xFF);
            carry = (sa >> 8) & 1;

            if ((a & 0x80) || a == 0) {
                // SHFTR2: byte shift ARG right.
                fac.ov = arg_m4;
                arg_m4 = arg_m3; arg_m3 = arg_m2; arg_m2 = arg_m1; arg_m1 = 0;
                continue;
            }
            break;
        }

        // SBC #8 (code18 line 99): undo overshoot. 6502 SBC = A - 8 - (1-C).
        int sbc = (int)(uint8_t)a - 9 + carry;
        int8_t y = (int8_t)(sbc & 0xFF);
        carry = (sbc >= 0) ? 1 : 0;

        // TAY; LDA FACOV; BCS SHFTRT.
        uint8_t a_reg = fac.ov;
        if (!carry) {
            // Bit shift loop (code18 lines 103-113).
            while (y != 0) {
                shiftr_one_bit(arg_m1, arg_m2, arg_m3, arg_m4, a_reg);
                y++;
            }
        }
        // SHFTRT (code18 line 114): CLC; RTS. Carry = 0.
        carry = 0;

        // FADD4 (code17 line 129): BIT ARISGN; BPL FADD2 (same sign → add).
        if (!(arisgn & 0x80)) {
            // FADD2 (code18 line 18): ADC OLDOV, then add mantissa bytes.
            uint16_t s;
            s = (uint16_t)a_reg + oldov + carry;  fac.ov = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m4 + arg_m4 + carry; fac.m4 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m3 + arg_m3 + carry; fac.m3 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m2 + arg_m2 + carry; fac.m2 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m1 + arg_m1 + carry; fac.m1 = s & 0xFF; carry = (s >> 8) & 1;

            // SQUEEZ (code18 line 46): BCC RNDRTS. If carry, RNDSHF.
            if (carry) {
                fac.exp++;
                uint8_t c = 1, nc;
                nc = fac.m1 & 1; fac.m1 = (c << 7) | (fac.m1 >> 1); c = nc;
                nc = fac.m2 & 1; fac.m2 = (c << 7) | (fac.m2 >> 1); c = nc;
                nc = fac.m3 & 1; fac.m3 = (c << 7) | (fac.m3 >> 1); c = nc;
                nc = fac.m4 & 1; fac.m4 = (c << 7) | (fac.m4 >> 1); c = nc;
                fac.ov = (c << 7) | (fac.ov >> 1);
            }
        }
    }
    // diff > 0 (ARG > FAC): not reached in RND.
}

// --- NORMAL: normalize FAC (code17 line 153, code18 lines 1-17) ---
void C64Rnd::normalize(FAC& fac) {
    if (fac.exp == 0) return;

    // Byte-level left shift (code17 lines 156-161, code18 lines 2-13).
    while (fac.m1 == 0) {
        if (fac.exp <= 8) { fac.exp = 0; return; }
        fac.exp -= 8;
        fac.m1 = fac.m2; fac.m2 = fac.m3; fac.m3 = fac.m4; fac.m4 = fac.ov; fac.ov = 0;
    }

    // Bit-level left shift (code18 lines 33-39): ASL FACOV; ROL m4..m1.
    while (!(fac.m1 & 0x80)) {
        if (fac.exp <= 1) { fac.exp = 0; return; }
        fac.exp--;
        uint8_t c;
        c = fac.ov >> 7;  fac.ov <<= 1;
        uint8_t c1 = fac.m4 >> 7; fac.m4 = (fac.m4 << 1) | c;  c = c1;
        c1 = fac.m3 >> 7; fac.m3 = (fac.m3 << 1) | c;  c = c1;
        c1 = fac.m2 >> 7; fac.m2 = (fac.m2 << 1) | c;  c = c1;
        fac.m1 = (fac.m1 << 1) | c;
    }
}

// --- Convert FAC to double ---
double C64Rnd::fac_to_double(const FAC& fac) {
    if (fac.exp == 0) return 0.0;
    int true_exp = (int)fac.exp - 128;
    uint32_t mantissa = ((uint32_t)fac.m1 << 24) | ((uint32_t)fac.m2 << 16)
                      | ((uint32_t)fac.m3 << 8) | fac.m4;
    double result = (double)mantissa / 4294967296.0;
    if (true_exp > 0) for (int i = 0; i < true_exp; i++) result *= 2.0;
    else if (true_exp < 0) for (int i = 0; i < -true_exp; i++) result *= 0.5;
    if (fac.sign & 0x80) result = -result;
    return result;
}

// --- RND(1): the complete function at $E097-$E0F6 ---
double C64Rnd::next() {
    FAC fac = movfm(seed_.data());
    fmult(fac, RMULZC);
    fadd(fac, RADDZC);

    // Byte swap: m1<->m4, m2<->m3 ($E0D3-$E0E1)
    uint8_t t;
    t = fac.m1; fac.m1 = fac.m4; fac.m4 = t;
    t = fac.m2; fac.m2 = fac.m3; fac.m3 = t;

    // Force into [0,1) ($E0E3-$E0ED)
    fac.sign = 0;
    fac.ov = fac.exp;
    fac.exp = 0x80;
    normalize(fac);

    movmf(fac, seed_.data());
    return fac_to_double(fac);
}

bool C64Rnd::next_slash() {
    return next() < 0.5;
}
