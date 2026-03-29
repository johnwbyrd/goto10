#include "c64rnd.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>

// Optimized state-step function that operates directly on the 5-byte seed.
// Avoids float conversion entirely. Reproduces the exact same state
// transitions as C64Rnd::next().
static void step(uint8_t state[5]) {
    // --- MOVFM: unpack seed into FAC ---
    uint8_t fac_exp = state[0];
    uint8_t fac_m1  = state[1] | 0x80;  // restore implicit high bit
    uint8_t fac_m2  = state[2];
    uint8_t fac_m3  = state[3];
    uint8_t fac_m4  = state[4];
    // sign is always 0 for RND seeds, FACOV = 0 after unpack

    // --- FMULT by RMULZC ($98 $35 $44 $7A $00) ---
    // Multiplier mantissa with implicit bit restored
    const uint8_t arg_m1 = 0xB5;
    const uint8_t arg_m2 = 0x44;
    const uint8_t arg_m3 = 0x7A;
    const uint8_t arg_m4 = 0x00;
    const uint8_t arg_exp = 0x98;

    // New exponent
    uint16_t new_exp = (uint16_t)fac_exp + (uint16_t)arg_exp - 128;
    if (new_exp > 255 || new_exp == 0) {
        // Overflow or underflow -- shouldn't happen in normal RND sequence
        // but handle gracefully
        memset(state, 0, 5);
        return;
    }

    // Shift-and-add multiply, exactly as the C64 does it.
    // Process 5 multiplier bytes: FACOV(=0), m4, m3, m2, m1
    uint8_t res1 = 0, res2 = 0, res3 = 0, res4 = 0, res_ov = 0;
    uint8_t mul_bytes[5] = { 0, fac_m4, fac_m3, fac_m2, fac_m1 };

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

        uint8_t carry_bit = mbyte & 1;
        mbyte = (mbyte >> 1) | 0x80;

        for (;;) {
            if (carry_bit) {
                uint16_t sum;
                sum = (uint16_t)res4 + (uint16_t)arg_m4;
                res4 = sum & 0xFF;
                sum = (uint16_t)res3 + (uint16_t)arg_m3 + (sum >> 8);
                res3 = sum & 0xFF;
                sum = (uint16_t)res2 + (uint16_t)arg_m2 + (sum >> 8);
                res2 = sum & 0xFF;
                sum = (uint16_t)res1 + (uint16_t)arg_m1 + (sum >> 8);
                res1 = sum & 0xFF;
                carry_bit = (sum >> 8) & 1;
            } else {
                carry_bit = 0;
            }

            uint8_t nc;
            nc = res1 & 1; res1 = (carry_bit << 7) | (res1 >> 1); carry_bit = nc;
            nc = res2 & 1; res2 = (carry_bit << 7) | (res2 >> 1); carry_bit = nc;
            nc = res3 & 1; res3 = (carry_bit << 7) | (res3 >> 1); carry_bit = nc;
            nc = res4 & 1; res4 = (carry_bit << 7) | (res4 >> 1); carry_bit = nc;
            nc = res_ov & 1; res_ov = (carry_bit << 7) | (res_ov >> 1);

            carry_bit = mbyte & 1;
            mbyte >>= 1;
            if (mbyte == 0) break;
        }
    }

    // --- Post-multiply normalize ---
    uint8_t r_exp = (uint8_t)new_exp;
    uint8_t r_ov = res_ov;

    while (res1 == 0) {
        if (r_exp <= 8) { memset(state, 0, 5); return; }
        r_exp -= 8;
        res1 = res2; res2 = res3; res3 = res4; res4 = r_ov; r_ov = 0;
    }
    while (!(res1 & 0x80)) {
        if (r_exp <= 1) { memset(state, 0, 5); return; }
        r_exp--;
        uint8_t c0 = r_ov >> 7;   r_ov <<= 1;
        uint8_t c1 = res4 >> 7;   res4 = (res4 << 1) | c0;
        uint8_t c2 = res3 >> 7;   res3 = (res3 << 1) | c1;
        uint8_t c3 = res2 >> 7;   res2 = (res2 << 1) | c2;
        res1 = (res1 << 1) | c3;
    }

    // --- FADD is a no-op (exponent difference too large) ---

    // --- Byte swap: m1<->m4, m2<->m3 ---
    uint8_t tmp;
    tmp = res1; res1 = res4; res4 = tmp;
    tmp = res2; res2 = res3; res3 = tmp;

    // --- Force into [0,1): sign=0, ov=exp, exp=$80, normalize ---
    r_ov = r_exp;
    r_exp = 0x80;

    while (res1 == 0) {
        if (r_exp <= 8) { memset(state, 0, 5); return; }
        r_exp -= 8;
        res1 = res2; res2 = res3; res3 = res4; res4 = r_ov; r_ov = 0;
    }
    while (!(res1 & 0x80)) {
        if (r_exp <= 1) { memset(state, 0, 5); return; }
        r_exp--;
        uint8_t c0 = r_ov >> 7;   r_ov <<= 1;
        uint8_t c1 = res4 >> 7;   res4 = (res4 << 1) | c0;
        uint8_t c2 = res3 >> 7;   res3 = (res3 << 1) | c1;
        uint8_t c3 = res2 >> 7;   res2 = (res2 << 1) | c2;
        res1 = (res1 << 1) | c3;
    }

    // --- MOVMF: round and pack ---
    if (r_ov & 0x80) {
        res4++;
        if (res4 == 0) { res3++; if (res3 == 0) { res2++; if (res2 == 0) {
            res1++;
            if (res1 == 0) { res1 = 0x80; r_exp++; }
        }}}
    }

    state[0] = r_exp;
    state[1] = res1 & 0x7F;  // clear implicit bit, sign=0
    state[2] = res2;
    state[3] = res3;
    state[4] = res4;
}

static bool state_eq(const uint8_t a[5], const uint8_t b[5]) {
    return memcmp(a, b, 5) == 0;
}

static void state_copy(uint8_t dst[5], const uint8_t src[5]) {
    memcpy(dst, src, 5);
}

int main() {
    // Verify the optimized step matches C64Rnd
    printf("Verifying optimized stepper against C64Rnd...\n");
    {
        C64Rnd rng;
        uint8_t st[5] = {0x80, 0x4F, 0xC7, 0x52, 0x58};
        bool ok = true;
        for (int i = 0; i < 1000; i++) {
            rng.next();
            step(st);
            auto seed = rng.seed();
            if (memcmp(st, seed.data(), 5) != 0) {
                printf("MISMATCH at step %d!\n", i + 1);
                printf("  C64Rnd: %02X %02X %02X %02X %02X\n",
                       seed[0], seed[1], seed[2], seed[3], seed[4]);
                printf("  fast:   %02X %02X %02X %02X %02X\n",
                       st[0], st[1], st[2], st[3], st[4]);
                ok = false;
                break;
            }
        }
        if (ok) printf("First 1000 steps match.\n");
        else return 1;
    }

    // --- Brent's cycle detection ---
    // https://en.wikipedia.org/wiki/Cycle_detection#Brent's_algorithm
    //
    // Finds mu (tail length) and lambda (cycle length) such that
    // f^mu(x0) = f^(mu+lambda)(x0), with lambda minimal.

    printf("\nRunning Brent's cycle detection...\n");
    auto t0 = std::chrono::steady_clock::now();

    const uint8_t x0[5] = {0x80, 0x4F, 0xC7, 0x52, 0x58};

    uint8_t tortoise[5], hare[5];
    state_copy(tortoise, x0);
    state_copy(hare, x0);
    step(hare);

    uint64_t power = 1;
    uint64_t lambda = 1;
    uint64_t steps = 1;
    uint64_t report_interval = 1ULL << 28;  // ~268M
    uint64_t next_report = report_interval;

    while (!state_eq(tortoise, hare)) {
        if (power == lambda) {
            // Move tortoise to hare's position, reset lambda
            state_copy(tortoise, hare);
            power *= 2;
            lambda = 0;
        }
        step(hare);
        lambda++;
        steps++;

        if (steps >= next_report) {
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration<double>(now - t0).count();
            double rate = steps / secs / 1e6;
            printf("  %llu steps (%.1f M/sec, %.1f sec elapsed)...\n",
                   (unsigned long long)steps, rate, secs);
            next_report += report_interval;
        }
    }

    printf("\nCycle length (lambda) = %llu\n", (unsigned long long)lambda);

    // Find mu: the index where the cycle starts.
    // Reset both to x0, advance hare by lambda, then step both until they meet.
    state_copy(tortoise, x0);
    state_copy(hare, x0);
    for (uint64_t i = 0; i < lambda; i++) {
        step(hare);
    }

    uint64_t mu = 0;
    while (!state_eq(tortoise, hare)) {
        step(tortoise);
        step(hare);
        mu++;
    }

    auto t1 = std::chrono::steady_clock::now();
    double total_secs = std::chrono::duration<double>(t1 - t0).count();

    printf("Tail length (mu) = %llu\n", (unsigned long long)mu);
    printf("Total time: %.1f seconds\n", total_secs);
    printf("\nThe sequence from the default seed enters a cycle of length %llu\n",
           (unsigned long long)lambda);
    printf("after %llu non-repeating values.\n", (unsigned long long)mu);

    if (mu == 0) {
        printf("mu=0 means the initial seed is ON the cycle.\n");
        printf("The entire sequence is periodic with period %llu.\n",
               (unsigned long long)lambda);
    }

    // --- Also find the period of the output bit sequence ---
    // The output bit (/ vs \) depends on whether the value >= 0.5,
    // which is determined by whether exponent == $80.
    // If the state period is lambda and starts at mu, the output period
    // divides lambda. Check if it's shorter.
    printf("\nChecking if the output bit sequence (/ vs \\) has a shorter period...\n");

    // Advance to the cycle start
    uint8_t st[5];
    state_copy(st, x0);
    for (uint64_t i = 0; i < mu; i++) step(st);

    // Record the first output bit of the cycle, then check when the
    // output sequence repeats. The output period must divide lambda.
    // Collect output bits in chunks and compare.

    // For practicality, just check all divisors of lambda that are
    // also cycle lengths. Since the output is derived from state,
    // the output period must divide the state period.

    // Quick check: does the output repeat at lambda/2, lambda/3, etc.?
    // First, collect the output bits for one full cycle.

    if (lambda <= 100000000ULL) {
        // Small enough to store the full output bit sequence
        printf("Collecting %llu output bits...\n", (unsigned long long)lambda);

        // Allocate bit array
        uint64_t byte_count = (lambda + 7) / 8;
        uint8_t* bits = new(std::nothrow) uint8_t[byte_count];
        if (bits) {
            memset(bits, 0, byte_count);
            state_copy(st, x0);
            for (uint64_t i = 0; i < mu; i++) step(st);

            for (uint64_t i = 0; i < lambda; i++) {
                // Output bit: '\' if value < 0.5 (exp < $80), '/' if >= 0.5
                bool is_backslash = (st[0] < 0x80);
                if (is_backslash) bits[i / 8] |= (1 << (i % 8));
                step(st);
            }

            // Check divisors of lambda
            printf("Checking divisors of lambda for shorter output period...\n");

            // Find all divisors of lambda up to lambda/2
            uint64_t output_period = lambda;
            for (uint64_t d = 1; d * d <= lambda; d++) {
                if (lambda % d != 0) continue;
                // Check both d and lambda/d
                uint64_t candidates[2] = { d, lambda / d };
                for (int ci = 0; ci < 2; ci++) {
                    uint64_t p = candidates[ci];
                    if (p >= output_period) continue;
                    // Check if output repeats with period p
                    bool match = true;
                    for (uint64_t i = 0; i < lambda && match; i++) {
                        bool b1 = (bits[i / 8] >> (i % 8)) & 1;
                        bool b2 = (bits[(i % p) / 8] >> ((i % p) % 8)) & 1;
                        if (b1 != b2) match = false;
                    }
                    if (match) {
                        output_period = p;
                        printf("  Output repeats at period %llu (divides lambda)\n",
                               (unsigned long long)p);
                    }
                }
            }

            printf("\nOutput bit sequence period = %llu\n",
                   (unsigned long long)output_period);
            if (output_period == lambda) {
                printf("Same as the full state period.\n");
            }

            // Print some stats
            uint64_t backslash_count = 0;
            for (uint64_t i = 0; i < lambda; i++) {
                if ((bits[i / 8] >> (i % 8)) & 1) backslash_count++;
            }
            printf("\nIn one full cycle:\n");
            printf("  '\\' count: %llu (%.4f%%)\n",
                   (unsigned long long)backslash_count,
                   100.0 * backslash_count / lambda);
            printf("  '/' count:  %llu (%.4f%%)\n",
                   (unsigned long long)(lambda - backslash_count),
                   100.0 * (lambda - backslash_count) / lambda);

            delete[] bits;
        } else {
            printf("Could not allocate memory for %llu bits.\n",
                   (unsigned long long)lambda);
        }
    } else {
        printf("Cycle too long (%llu) to store output bits in memory.\n",
               (unsigned long long)lambda);
        printf("Skipping output period sub-analysis.\n");
    }

    return 0;
}
