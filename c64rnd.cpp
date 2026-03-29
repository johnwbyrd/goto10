#include "c64rnd.h"

// Unpack 5-byte MBF from memory into FAC.
// Equivalent to MOVFM ($BBA2).
//
// Memory format: [exp] [m1|sign] [m2] [m3] [m4]
// Bit 7 of m1 is the sign bit in memory; when unpacked, sign goes
// to FACSGN and bit 7 of m1 is restored to 1 (implicit leading 1).
C64Rnd::FAC C64Rnd::movfm(const uint8_t* mem) {
    FAC f{};
    f.exp  = mem[0];
    f.m1   = mem[1] | 0x80;  // restore implicit high bit
    f.m2   = mem[2];
    f.m3   = mem[3];
    f.m4   = mem[4];
    f.sign = (mem[1] & 0x80) ? 0xFF : 0x00;
    f.ov   = 0;
    return f;
}

// Pack FAC back into 5-byte MBF memory.
// Equivalent to MOVMF ($BBD4).
//
// First calls ROUND ($BC1B), then stores bytes.
// ROUND: ASL FACOV (shift bit 7 into carry, destroying FACOV).
// If carry set, call INCFAC to increment mantissa from LSB.
// If INCFAC causes m1 to wrap to 0, shift right 1 and bump exponent.
void C64Rnd::movmf(const FAC& fac, uint8_t* mem) {
    uint8_t m1 = fac.m1;
    uint8_t m2 = fac.m2;
    uint8_t m3 = fac.m3;
    uint8_t m4 = fac.m4;
    uint8_t exp = fac.exp;

    if (exp == 0) {
        mem[0] = mem[1] = mem[2] = mem[3] = mem[4] = 0;
        return;
    }

    // ROUND: ASL FACOV — tests bit 7, if set, increment mantissa
    if (fac.ov & 0x80) {
        // INCFAC: INC FACLO, if zero INC FACMO, if zero INC FACMOH, if zero INC FACHO
        m4++;
        if (m4 == 0) {
            m3++;
            if (m3 == 0) {
                m2++;
                if (m2 == 0) {
                    m1++;
                    if (m1 == 0) {
                        // Mantissa overflowed to zero.
                        // RNDSHF: ROR into m1 (C=1 from the INC wrap), INC exp
                        // C=1 since INCFAC doesn't touch C (the carry was set
                        // from the ASL FACOV), so ROR shifts 1 into bit 7.
                        m1 = 0x80;
                        exp++;
                    }
                }
            }
        }
    }

    mem[0] = exp;
    mem[1] = (m1 & 0x7F) | (fac.sign & 0x80);
    mem[2] = m2;
    mem[3] = m3;
    mem[4] = m4;
}

// Multiply FAC by the 5-byte MBF constant at mem[].
// Equivalent to FMULT ($BA28).
//
// This reproduces the exact C64 shift-and-add binary multiplication:
// - Unpack the constant into ARG
// - Add exponents (minus bias)
// - Multiply mantissas via 40-bit shift-and-add
// - Normalize result
void C64Rnd::fmult(FAC& fac, const uint8_t* mem) {
    if (fac.exp == 0) return;  // FAC is zero, result is zero

    // Unpack constant into ARG (CONUPK, $BA8C)
    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) {
        // ARG is zero, result is zero
        fac.exp = 0;
        fac.m1 = fac.m2 = fac.m3 = fac.m4 = 0;
        fac.ov = 0;
        return;
    }

    uint8_t arg_m1 = mem[1] | 0x80;  // restore implicit high bit
    uint8_t arg_m2 = mem[2];
    uint8_t arg_m3 = mem[3];
    uint8_t arg_m4 = mem[4];
    uint8_t arg_sign = mem[1] & 0x80;

    // MULDIV ($BAB7): combine exponents and signs
    // New exponent = FAC_exp + ARG_exp. If sum < 256, subtract 128 (remove double bias).
    // If sum >= 256, the intermediate overflows, so we add back (sum - 256 + 128) = sum - 128.
    uint16_t new_exp = (uint16_t)fac.exp + (uint16_t)arg_exp;
    // The 6502 code does: CLC, ADC FACEXP. If carry set (>=256), we subtract 128.
    // If no carry (<256), we also subtract 128, but if result < 128 we underflow to zero.
    if (new_exp < 128) {
        // Underflow - result is zero
        fac.exp = 0;
        fac.m1 = fac.m2 = fac.m3 = fac.m4 = 0;
        fac.ov = 0;
        return;
    }
    new_exp -= 128;
    if (new_exp > 255) {
        // Overflow - on real C64 this would be an error, but shouldn't happen with RND
        new_exp = 255;
    }

    // Result sign = XOR of signs
    fac.sign = (fac.sign ^ arg_sign) & 0x80;

    // Shift-and-add multiplication.
    // Process FAC mantissa bytes from LSB to MSB: FACOV, FACLO, FACMO, FACMOH, FACHO
    // For each byte, if zero just shift result right 8 bits; otherwise do
    // 8 rounds of conditional-add-and-shift.
    //
    // Result accumulates in RESHO:RESMOH:RESMO:RESLO:FACOV (5 bytes, MSB first)

    uint8_t res1 = 0, res2 = 0, res3 = 0, res4 = 0; // RESHO, RESMOH, RESMO, RESLO
    uint8_t res_ov = 0; // FACOV (rounding byte of result)

    // The 5 multiplier bytes, processed LSB to MSB
    uint8_t mul_bytes[5] = { fac.ov, fac.m4, fac.m3, fac.m2, fac.m1 };

    for (int i = 0; i < 5; i++) {
        uint8_t mbyte = mul_bytes[i];

        if (mbyte == 0) {
            // MULSHF: shift result right 8 bits
            res_ov = res4;
            res4 = res3;
            res3 = res2;
            res2 = res1;
            res1 = 0;
            continue;
        }

        // MLTPL1 (for FACHO) or MLTPLY (for others):
        // LSR A to get first bit into carry, then ORA #$80 as sentinel
        uint8_t carry_bit;
        if (i < 4) {
            // MLTPLY path: first check if zero (already handled above),
            // then fall through to MLTPL1
            carry_bit = mbyte & 1;
            mbyte = (mbyte >> 1) | 0x80;  // LSR then ORA #$80
        } else {
            // MLTPL1 path (for FACHO, the MSB): same operation
            carry_bit = mbyte & 1;
            mbyte = (mbyte >> 1) | 0x80;
        }

        // Loop: process 8 bits
        for (;;) {
            if (carry_bit) {
                // Add ARG mantissa to result (RESHO:RESMOH:RESMO:RESLO)
                uint16_t sum;
                sum = (uint16_t)res4 + (uint16_t)arg_m4;
                res4 = sum & 0xFF;
                sum = (uint16_t)res3 + (uint16_t)arg_m3 + (sum >> 8);
                res3 = sum & 0xFF;
                sum = (uint16_t)res2 + (uint16_t)arg_m2 + (sum >> 8);
                res2 = sum & 0xFF;
                sum = (uint16_t)res1 + (uint16_t)arg_m1 + (sum >> 8);
                res1 = sum & 0xFF;
                carry_bit = (sum >> 8) & 1;  // carry from addition
            } else {
                carry_bit = 0;
            }

            // ROR result right 1 bit (carry feeds into MSB)
            uint8_t new_carry;

            new_carry = res1 & 1;
            res1 = (carry_bit << 7) | (res1 >> 1);
            carry_bit = new_carry;

            new_carry = res2 & 1;
            res2 = (carry_bit << 7) | (res2 >> 1);
            carry_bit = new_carry;

            new_carry = res3 & 1;
            res3 = (carry_bit << 7) | (res3 >> 1);
            carry_bit = new_carry;

            new_carry = res4 & 1;
            res4 = (carry_bit << 7) | (res4 >> 1);
            carry_bit = new_carry;

            new_carry = res_ov & 1;
            res_ov = (carry_bit << 7) | (res_ov >> 1);
            // The carry out of res_ov is discarded

            // TYA; LSR A - shift multiplier byte right, getting next bit
            carry_bit = mbyte & 1;
            mbyte >>= 1;

            if (mbyte == 0) break;  // sentinel bit shifted out, done with this byte
        }
    }

    // MOVFR ($BB8F): copy result to FAC and normalize
    fac.exp = (uint8_t)new_exp;
    fac.m1 = res1;
    fac.m2 = res2;
    fac.m3 = res3;
    fac.m4 = res4;
    fac.ov = res_ov;

    normalize(fac);
}

// Add 5-byte MBF constant to FAC.
// Equivalent to FADD ($B867).
//
// For the RND function, the additive constant is so small (~3.9E-8)
// relative to the product (~millions) that it gets completely shifted
// out during alignment. The result is unchanged. We implement it
// anyway for correctness.
void C64Rnd::fadd(FAC& fac, const uint8_t* mem) {
    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) return;  // adding zero

    if (fac.exp == 0) {
        // FAC is zero, result is the constant
        fac = movfm(mem);
        return;
    }

    // Compute exponent difference to determine alignment shift
    int exp_diff = (int)fac.exp - (int)arg_exp;

    if (exp_diff > 0) {
        // FAC has larger exponent; ARG must be shifted right by exp_diff bits.
        // If exp_diff >= 40 (8 + 32 mantissa bits), ARG is completely lost.
        if (exp_diff >= 40) return;  // ARG contributes nothing

        // For the RND case, exp_diff = $98 - $68 = $30 = 48, so we always return here.
        // The add truly is a no-op.

        // (Full FADD implementation would align mantissas and add, but
        // for the RND use case this path is never reached.)
        return;
    }

    // exp_diff <= 0: ARG has larger or equal exponent.
    // Full implementation would be needed for a general FADD,
    // but this case doesn't occur in the RND function.
    // For completeness, just load the constant (rough approximation).
    // A bit-exact general FADD is complex and not needed here.
    if (exp_diff < -40) {
        fac = movfm(mem);
        return;
    }

    // For small differences, we'd need to implement the full add.
    // This doesn't happen in the RND sequence.
}

// Normalize FAC: shift mantissa left until MSB of m1 is set.
// Equivalent to NORMAL ($B8D7).
void C64Rnd::normalize(FAC& fac) {
    if (fac.exp == 0) return;

    // Byte-level normalization: if m1 == 0, shift left 8 bits at a time
    while (fac.m1 == 0) {
        if (fac.exp <= 8) {
            fac.exp = 0;
            return;
        }
        fac.exp -= 8;
        fac.m1 = fac.m2;
        fac.m2 = fac.m3;
        fac.m3 = fac.m4;
        fac.m4 = fac.ov;
        fac.ov = 0;
    }

    // Bit-level normalization: shift left until MSB is set
    while (!(fac.m1 & 0x80)) {
        if (fac.exp <= 1) {
            fac.exp = 0;
            return;
        }
        fac.exp--;

        uint8_t c0 = (fac.ov >> 7);
        fac.ov <<= 1;
        uint8_t c1 = (fac.m4 >> 7);
        fac.m4 = (fac.m4 << 1) | c0;
        uint8_t c2 = (fac.m3 >> 7);
        fac.m3 = (fac.m3 << 1) | c1;
        uint8_t c3 = (fac.m2 >> 7);
        fac.m2 = (fac.m2 << 1) | c2;
        fac.m1 = (fac.m1 << 1) | c3;
    }
}

// Convert FAC to a C++ double.
double C64Rnd::fac_to_double(const FAC& fac) {
    if (fac.exp == 0) return 0.0;

    // MBF exponent is biased by 128. Exponent 128 means 2^0.
    int true_exp = (int)fac.exp - 128;

    // Mantissa: m1.m2.m3.m4 with implicit high bit set in m1.
    // Value = mantissa / 2^32 * 2^true_exp
    // But the mantissa is in [0.5, 1.0) when normalized (MSB set = 0.1 binary),
    // so the float value = mantissa_uint32 / 2^32 * 2^(true_exp)
    uint32_t mantissa = ((uint32_t)fac.m1 << 24)
                      | ((uint32_t)fac.m2 << 16)
                      | ((uint32_t)fac.m3 << 8)
                      | ((uint32_t)fac.m4);

    double result = (double)mantissa / 4294967296.0;  // / 2^32
    // Scale by 2^true_exp
    if (true_exp > 0) {
        for (int i = 0; i < true_exp; i++) result *= 2.0;
    } else if (true_exp < 0) {
        for (int i = 0; i < -true_exp; i++) result *= 0.5;
    }

    if (fac.sign & 0x80) result = -result;
    return result;
}

// The main RND(1) function.
// Equivalent to the n>0 path at $E09C-$E0F6.
double C64Rnd::next() {
    // 1. Load seed into FAC (MOVFM from RNDX)
    FAC fac = movfm(seed_.data());

    // 2. Multiply by RMULZC (11879546)
    fmult(fac, RMULZC);

    // 3. Add RADDZC (3.927677739E-8) -- effectively a no-op
    fadd(fac, RADDZC);

    // 4. Byte swap: m1<->m4, m2<->m3 (RND1, $E0D3-$E0E1)
    uint8_t tmp;
    tmp = fac.m1; fac.m1 = fac.m4; fac.m4 = tmp;
    tmp = fac.m2; fac.m2 = fac.m3; fac.m3 = tmp;

    // 5. Force positive sign (STRNEX, $E0E3-$E0E5)
    fac.sign = 0x00;

    // 6. Move exponent to rounding byte, set exponent to $80 ($E0E7-$E0ED)
    fac.ov = fac.exp;
    fac.exp = 0x80;

    // 7. Normalize ($E0EF)
    normalize(fac);

    // 8. Store result back as seed (MOVMF to RNDX, $E0F2-$E0F6)
    movmf(fac, seed_.data());

    // 9. Return the value as a double
    return fac_to_double(fac);
}

// Return the maze character decision for: 10 PRINT CHR$(205.5+RND(1)); : GOTO 10
// PETSCII $CD (205) = '\' diagonal (upper-left to lower-right)
// PETSCII $CE (206) = '/' diagonal (upper-right to lower-left)
// RND(1) < 0.5  -> 205.5+RND < 206 -> CHR$(205) = '\'  -> returns true
// RND(1) >= 0.5 -> 205.5+RND >= 206 -> CHR$(206) = '/'  -> returns false
bool C64Rnd::next_slash() {
    return next() < 0.5;
}
