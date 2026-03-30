#include "msbasicfprnd.h"

// --- Template method: RND(1) at $E097-$E0F6 ---
double MsBasicFpRnd::next() {
    FAC fac = movfm(seed_.data());
    fmult(fac);
    fadd(fac);
    byte_swap(fac);

    // Force into [0,1): $E0E3-$E0F1
    fac.sign = 0;
    fac.ov = fac.exp;
    fac.exp = 0x80;
    normalize(fac);

    movmf(fac, seed_.data());
    return fac_to_double(fac);
}

// --- MOVFM: unpack 5-byte MBF from memory into FAC (code20 line 11) ---
// FACOV is set to 0 by STY FACOV where Y=0 (code20 line 30).
// This is in Microsoft's original source, not a Commodore addition.
MsBasicFpRnd::FAC MsBasicFpRnd::movfm(const uint8_t* mem) {
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
void MsBasicFpRnd::movmf(const FAC& fac, uint8_t* mem) {
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

// --- NORMAL: normalize FAC (code17 line 153, code18 lines 1-17) ---
void MsBasicFpRnd::normalize(FAC& fac) {
    if (fac.exp == 0) return;

    while (fac.m1 == 0) {
        if (fac.exp <= 8) { fac.exp = 0; return; }
        fac.exp -= 8;
        fac.m1 = fac.m2; fac.m2 = fac.m3; fac.m3 = fac.m4; fac.m4 = fac.ov; fac.ov = 0;
    }

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
double MsBasicFpRnd::fac_to_double(const FAC& fac) {
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

// --- 6502 helper: one bit shift right via SHFTR3/SHFTR4/ROLSHF ---
static void shiftr_one_bit(uint8_t& b1, uint8_t& b2, uint8_t& b3, uint8_t& b4, uint8_t& a) {
    uint8_t c = (b1 >> 7) & 1;
    b1 <<= 1;
    if (c) b1++;
    uint8_t nc = b1 & 1; b1 = (c << 7) | (b1 >> 1); c = nc;
    nc = b1 & 1; b1 = (c << 7) | (b1 >> 1); c = nc;
    nc = b2 & 1; b2 = (c << 7) | (b2 >> 1); c = nc;
    nc = b3 & 1; b3 = (c << 7) | (b3 >> 1); c = nc;
    nc = b4 & 1; b4 = (c << 7) | (b4 >> 1); c = nc;
    nc = a & 1;  a  = (c << 7) | (a >> 1);
}

// --- FMULT: 6502 multiply with carry leak (code19 line 30) ---
void MsBasicFpRnd::fmult_6502(FAC& fac, const uint8_t* mem) {
    if (fac.exp == 0) return;

    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) { fac.exp = 0; fac.m1 = fac.m2 = fac.m3 = fac.m4 = fac.ov = 0; return; }
    uint8_t arg_m1 = mem[1] | 0x80;
    uint8_t arg_m2 = mem[2];
    uint8_t arg_m3 = mem[3];
    uint8_t arg_m4 = mem[4];
    uint8_t arg_sign = mem[1] & 0x80;

    uint16_t exp_sum = (uint16_t)fac.exp + (uint16_t)arg_exp;
    uint8_t low = exp_sum & 0xFF;
    uint16_t biased = (uint16_t)low + 0x80;
    uint8_t new_exp = biased & 0xFF;
    if (new_exp == 0) { fac.exp = 0; fac.m1 = fac.m2 = fac.m3 = fac.m4 = fac.ov = 0; return; }

    fac.sign = (fac.sign ^ arg_sign) & 0x80;
    uint8_t carry = (biased >> 8) & 1;

    uint8_t res1 = 0, res2 = 0, res3 = 0, res4 = 0, res_ov = 0;
    uint8_t mul_bytes[5] = { fac.ov, fac.m4, fac.m3, fac.m2, fac.m1 };

    for (int i = 0; i < 5; i++) {
        uint8_t mbyte = mul_bytes[i];

        if (mbyte == 0) {
            res_ov = res4; res4 = res3; res3 = res2; res2 = res1; res1 = 0;

            uint16_t sa = (uint16_t)0 + 8 + carry;
            uint8_t a = sa & 0xFF;
            carry = (sa >> 8) & 1;
            int sbc = (int)a - 9 + carry;
            uint8_t y = (uint8_t)(sbc & 0xFF);
            carry = (sbc >= 0) ? 1 : 0;

            uint8_t a_reg = res_ov;
            if (carry) {
                carry = 0;
            } else {
                while (y != 0) {
                    shiftr_one_bit(res1, res2, res3, res4, a_reg);
                    y++;
                }
                carry = 0;
            }
            res_ov = a_reg;
            continue;
        }

        carry = mbyte & 1;
        mbyte = (mbyte >> 1) | 0x80;

        for (;;) {
            if (carry) {
                uint16_t s;
                s = (uint16_t)res4 + arg_m4;           res4 = s & 0xFF;
                s = (uint16_t)res3 + arg_m3 + (s >> 8); res3 = s & 0xFF;
                s = (uint16_t)res2 + arg_m2 + (s >> 8); res2 = s & 0xFF;
                s = (uint16_t)res1 + arg_m1 + (s >> 8); res1 = s & 0xFF;
                carry = (s >> 8) & 1;
            } else {
                carry = 0;
            }

            uint8_t nc;
            nc = res1 & 1;   res1   = (carry << 7) | (res1 >> 1);   carry = nc;
            nc = res2 & 1;   res2   = (carry << 7) | (res2 >> 1);   carry = nc;
            nc = res3 & 1;   res3   = (carry << 7) | (res3 >> 1);   carry = nc;
            nc = res4 & 1;   res4   = (carry << 7) | (res4 >> 1);   carry = nc;
            nc = res_ov & 1; res_ov = (carry << 7) | (res_ov >> 1);

            carry = mbyte & 1;
            mbyte >>= 1;
            if (mbyte == 0) break;
        }
    }

    fac.exp = new_exp;
    fac.m1 = res1; fac.m2 = res2; fac.m3 = res3; fac.m4 = res4; fac.ov = res_ov;
    normalize(fac);
}

// --- FADD: 6502 add with OLDOV rounding byte carry (code17 line 99) ---
void MsBasicFpRnd::fadd_6502(FAC& fac, const uint8_t* mem) {
    uint8_t arg_exp = mem[0];
    if (arg_exp == 0) return;

    uint8_t arg_m1 = mem[1] | 0x80;
    uint8_t arg_m2 = mem[2];
    uint8_t arg_m3 = mem[3];
    uint8_t arg_m4 = mem[4];
    uint8_t arisgn = (mem[1] & 0x80) ^ (fac.sign & 0x80);

    if (fac.exp == 0) { fac = movfm(mem); return; }

    uint8_t oldov = fac.ov;
    int diff = (int)arg_exp - (int)fac.exp;

    if (diff == 0) {
        fac.ov = 0;
    } else if (diff < 0) {
        fac.ov = 0;

        int8_t a = (int8_t)diff;
        uint8_t carry = 0;

        for (;;) {
            uint16_t sa = (uint16_t)(uint8_t)a + 8 + carry;
            a = (int8_t)(sa & 0xFF);
            carry = (sa >> 8) & 1;

            if ((a & 0x80) || a == 0) {
                fac.ov = arg_m4;
                arg_m4 = arg_m3; arg_m3 = arg_m2; arg_m2 = arg_m1; arg_m1 = 0;
                continue;
            }
            break;
        }

        int sbc = (int)(uint8_t)a - 9 + carry;
        int8_t y = (int8_t)(sbc & 0xFF);
        carry = (sbc >= 0) ? 1 : 0;

        uint8_t a_reg = fac.ov;
        if (!carry) {
            while (y != 0) {
                shiftr_one_bit(arg_m1, arg_m2, arg_m3, arg_m4, a_reg);
                y++;
            }
        }
        carry = 0;

        if (!(arisgn & 0x80)) {
            uint16_t s;
            s = (uint16_t)a_reg + oldov + carry;  fac.ov = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m4 + arg_m4 + carry; fac.m4 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m3 + arg_m3 + carry; fac.m3 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m2 + arg_m2 + carry; fac.m2 = s & 0xFF; carry = (s >> 8) & 1;
            s = (uint16_t)fac.m1 + arg_m1 + carry; fac.m1 = s & 0xFF; carry = (s >> 8) & 1;

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
}
