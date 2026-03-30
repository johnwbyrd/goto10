// For each of the known cycles, analyze the zero-byte patterns
// of states on the cycle.

#include "c64rnd.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>

static void brent(const C64Rnd::Seed& seed, uint64_t& mu, uint64_t& lambda) {
    C64Rnd tortoise(seed), hare(seed);
    hare.next();
    uint64_t power = 1; lambda = 1;
    while (tortoise.seed() != hare.seed()) {
        if (power == lambda) { tortoise.set_seed(hare.seed()); power *= 2; lambda = 0; }
        hare.next(); lambda++;
    }
    tortoise.set_seed(seed); hare.set_seed(seed);
    for (uint64_t i = 0; i < lambda; i++) hare.next();
    mu = 0;
    while (tortoise.seed() != hare.seed()) { tortoise.next(); hare.next(); mu++; }
}

// Seeds that reach each of the 12 known cycles (from the 432K scan)
static const uint32_t cycle_seeds[] = {
    1,       // 58078
    2,       // 724 (RND(-2) from earlier scan)
    25,      // 7036
    39,      // 5660
    503,     // 4232
    181,     // 2644
    381,     // 295
    7737,    // 207
    164855,  // 23
    77164,   // 7
    94836,   // 371
    152007,  // 85
};

static C64Rnd::Seed rnd_neg_seed(uint32_t n) {
    C64Rnd::Seed seed = {0,0,0,0,0};
    if (n == 0) return seed;
    uint32_t val = n;
    int bits = 0; uint32_t tmp = val;
    while (tmp > 0) { bits++; tmp >>= 1; }
    uint8_t exp = (uint8_t)(128 + bits);
    uint32_t mantissa = val << (32 - bits);
    uint8_t m1 = (mantissa >> 24), m2 = (mantissa >> 16) & 0xFF,
            m3 = (mantissa >> 8) & 0xFF, m4 = mantissa & 0xFF;
    uint8_t t; t = m1; m1 = m4; m4 = t; t = m2; m2 = m3; m3 = t;
    uint8_t ov = exp, r_exp = 0x80;
    while (m1 == 0) {
        if (r_exp <= 8) return seed;
        r_exp -= 8; m1 = m2; m2 = m3; m3 = m4; m4 = ov; ov = 0;
    }
    while (!(m1 & 0x80)) {
        if (r_exp <= 1) return seed;
        r_exp--;
        uint8_t c = ov >> 7; ov <<= 1;
        uint8_t c1 = m4 >> 7; m4 = (m4 << 1) | c; c = c1;
        c1 = m3 >> 7; m3 = (m3 << 1) | c; c = c1;
        c1 = m2 >> 7; m2 = (m2 << 1) | c; c = c1;
        m1 = (m1 << 1) | c;
    }
    if (ov & 0x80) {
        m4++; if (m4 == 0) { m3++; if (m3 == 0) { m2++; if (m2 == 0) {
            m1++; if (m1 == 0) { m1 = 0x80; r_exp++; }
        }}}
    }
    seed[0] = r_exp; seed[1] = m1 & 0x7F; seed[2] = m2; seed[3] = m3; seed[4] = m4;
    return seed;
}

int main() {
    for (uint32_t ns : cycle_seeds) {
        auto seed = rnd_neg_seed(ns);
        uint64_t mu, lambda;
        brent(seed, mu, lambda);

        // Walk to cycle entry
        C64Rnd walker(seed);
        for (uint64_t i = 0; i < mu; i++) walker.next();

        // Walk the cycle, analyze each state
        // For each state, record which mantissa bytes (m2, m3, m4 in packed form = seed[2..4]) are zero
        // Also record exponent distribution
        std::map<uint8_t, uint64_t> exp_dist;
        std::map<uint8_t, uint64_t> zero_pattern_dist;  // bitmask: bit0=m4==0, bit1=m3==0, bit2=m2==0
        uint64_t fadd_would_contribute = 0;

        C64Rnd cyc(walker.seed());
        for (uint64_t i = 0; i < lambda; i++) {
            auto s = cyc.seed();
            uint8_t exp = s[0];
            // Unpack: m1 = s[1]|0x80, m2 = s[2], m3 = s[3], m4 = s[4]
            uint8_t m2 = s[2], m3 = s[3], m4 = s[4];
            uint8_t pattern = (m4 == 0 ? 1 : 0) | (m3 == 0 ? 2 : 0) | (m2 == 0 ? 4 : 0);
            zero_pattern_dist[pattern]++;
            exp_dist[exp]++;

            // FADD contributes when post-multiply exponent is low enough
            // Post-multiply exp = seed_exp + $98 - $80 = seed_exp + $18
            // FADD constant exp = $68
            // Difference = post_mult_exp - $68
            // FADD contributes when difference < 40 (40-bit precision)
            uint16_t post_exp = (uint16_t)exp + 0x18;
            if (post_exp < 256) {
                int diff = (int)post_exp - 0x68;
                if (diff < 40) fadd_would_contribute++;
            }

            cyc.next();
        }

        printf("=== Cycle %llu (from RND(-%u), mu=%llu) ===\n",
               (unsigned long long)lambda, ns, (unsigned long long)mu);

        printf("  Zero-byte patterns (bit0=m4, bit1=m3, bit2=m2):\n");
        for (auto& [pat, cnt] : zero_pattern_dist) {
            printf("    %c%c%c: %llu (%.2f%%)\n",
                   (pat & 4) ? '0' : '-',
                   (pat & 2) ? '0' : '-',
                   (pat & 1) ? '0' : '-',
                   (unsigned long long)cnt, 100.0 * cnt / lambda);
        }

        printf("  Exponent range: ");
        printf("$%02X-$%02X\n", exp_dist.begin()->first, exp_dist.rbegin()->first);

        printf("  FADD contributes on %llu/%llu states (%.2f%%)\n",
               (unsigned long long)fadd_would_contribute, (unsigned long long)lambda,
               100.0 * fadd_would_contribute / lambda);

        printf("\n");
    }
    return 0;
}
