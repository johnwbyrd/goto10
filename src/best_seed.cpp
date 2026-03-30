#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

// --- Step function ---

static void step(uint8_t state[5]) {
    uint8_t fac_exp = state[0];
    uint8_t fac_m1  = state[1] | 0x80;
    uint8_t fac_m2  = state[2];
    uint8_t fac_m3  = state[3];
    uint8_t fac_m4  = state[4];
    const uint8_t arg_m1 = 0xB5, arg_m2 = 0x44, arg_m3 = 0x7A, arg_m4 = 0x00, arg_exp = 0x98;
    uint16_t new_exp = (uint16_t)fac_exp + (uint16_t)arg_exp - 128;
    if (new_exp > 255 || new_exp == 0) { memset(state, 0, 5); return; }
    uint8_t res1=0, res2=0, res3=0, res4=0, res_ov=0;
    uint8_t mul_bytes[5] = {0, fac_m4, fac_m3, fac_m2, fac_m1};
    for (int i = 0; i < 5; i++) {
        uint8_t mbyte = mul_bytes[i];
        if (mbyte == 0) { res_ov=res4; res4=res3; res3=res2; res2=res1; res1=0; continue; }
        uint8_t cb = mbyte & 1; mbyte = (mbyte >> 1) | 0x80;
        for (;;) {
            if (cb) {
                uint16_t s; s=(uint16_t)res4+arg_m4; res4=s&0xFF;
                s=(uint16_t)res3+arg_m3+(s>>8); res3=s&0xFF;
                s=(uint16_t)res2+arg_m2+(s>>8); res2=s&0xFF;
                s=(uint16_t)res1+arg_m1+(s>>8); res1=s&0xFF; cb=(s>>8)&1;
            } else { cb=0; }
            uint8_t nc;
            nc=res1&1; res1=(cb<<7)|(res1>>1); cb=nc;
            nc=res2&1; res2=(cb<<7)|(res2>>1); cb=nc;
            nc=res3&1; res3=(cb<<7)|(res3>>1); cb=nc;
            nc=res4&1; res4=(cb<<7)|(res4>>1); cb=nc;
            nc=res_ov&1; res_ov=(cb<<7)|(res_ov>>1);
            cb=mbyte&1; mbyte>>=1; if(mbyte==0) break;
        }
    }
    uint8_t r_exp=(uint8_t)new_exp, r_ov=res_ov;
    while(res1==0){if(r_exp<=8){memset(state,0,5);return;} r_exp-=8;res1=res2;res2=res3;res3=res4;res4=r_ov;r_ov=0;}
    while(!(res1&0x80)){if(r_exp<=1){memset(state,0,5);return;} r_exp--;uint8_t c0=r_ov>>7;r_ov<<=1;uint8_t c1=res4>>7;res4=(res4<<1)|c0;uint8_t c2=res3>>7;res3=(res3<<1)|c1;uint8_t c3=res2>>7;res2=(res2<<1)|c2;res1=(res1<<1)|c3;}
    uint8_t tmp; tmp=res1;res1=res4;res4=tmp; tmp=res2;res2=res3;res3=tmp;
    r_ov=r_exp; r_exp=0x80;
    while(res1==0){if(r_exp<=8){memset(state,0,5);return;} r_exp-=8;res1=res2;res2=res3;res3=res4;res4=r_ov;r_ov=0;}
    while(!(res1&0x80)){if(r_exp<=1){memset(state,0,5);return;} r_exp--;uint8_t c0=r_ov>>7;r_ov<<=1;uint8_t c1=res4>>7;res4=(res4<<1)|c0;uint8_t c2=res3>>7;res3=(res3<<1)|c1;uint8_t c3=res2>>7;res2=(res2<<1)|c2;res1=(res1<<1)|c3;}
    if(r_ov&0x80){res4++;if(res4==0){res3++;if(res3==0){res2++;if(res2==0){res1++;if(res1==0){res1=0x80;r_exp++;}}}}}
    state[0]=r_exp; state[1]=res1&0x7F; state[2]=res2; state[3]=res3; state[4]=res4;
}

static inline uint64_t encode(const uint8_t s[5]) {
    return ((uint64_t)s[0]<<32)|((uint64_t)s[1]<<24)|((uint64_t)s[2]<<16)|((uint64_t)s[3]<<8)|s[4];
}

static void decode(uint64_t e, uint8_t s[5]) {
    s[4]=e&0xFF; s[3]=(e>>8)&0xFF; s[2]=(e>>16)&0xFF; s[1]=(e>>24)&0xFF; s[0]=(e>>32)&0xFF;
}

// Simulate RND(-N): convert integer N to MBF, then RND1 path
static void rnd_neg_seed(uint32_t n, uint8_t seed[5]) {
    if (n == 0) { memset(seed, 0, 5); return; }
    uint32_t val = n;
    int bits = 0; uint32_t tmp = val;
    while (tmp > 0) { bits++; tmp >>= 1; }
    uint8_t exp = (uint8_t)(128 + bits);
    uint32_t mantissa = val << (32 - bits);
    uint8_t m1=(mantissa>>24)&0xFF, m2=(mantissa>>16)&0xFF, m3=(mantissa>>8)&0xFF, m4=mantissa&0xFF;
    uint8_t t; t=m1;m1=m4;m4=t; t=m2;m2=m3;m3=t;
    uint8_t ov=exp, r_exp=0x80;
    while(m1==0){if(r_exp<=8){memset(seed,0,5);return;} r_exp-=8;m1=m2;m2=m3;m3=m4;m4=ov;ov=0;}
    while(!(m1&0x80)){if(r_exp<=1){memset(seed,0,5);return;} r_exp--;uint8_t c0=ov>>7;ov<<=1;uint8_t c1=m4>>7;m4=(m4<<1)|c0;uint8_t c2=m3>>7;m3=(m3<<1)|c1;uint8_t c3=m2>>7;m2=(m2<<1)|c2;m1=(m1<<1)|c3;}
    if(ov&0x80){m4++;if(m4==0){m3++;if(m3==0){m2++;if(m2==0){m1++;if(m1==0){m1=0x80;r_exp++;}}}}}
    seed[0]=r_exp; seed[1]=m1&0x7F; seed[2]=m2; seed[3]=m3; seed[4]=m4;
}

// Brent's cycle detection on a seed. Returns (tail, cycle_length).
static void brent(const uint8_t seed[5], uint64_t& mu, uint64_t& lambda) {
    uint8_t tortoise[5], hare[5];
    memcpy(tortoise, seed, 5);
    memcpy(hare, seed, 5);
    step(hare);

    uint64_t power = 1;
    lambda = 1;
    while (memcmp(tortoise, hare, 5) != 0) {
        if (power == lambda) {
            memcpy(tortoise, hare, 5);
            power *= 2;
            lambda = 0;
        }
        step(hare);
        lambda++;
    }

    // Find mu
    memcpy(tortoise, seed, 5);
    memcpy(hare, seed, 5);
    for (uint64_t i = 0; i < lambda; i++) step(hare);
    mu = 0;
    while (memcmp(tortoise, hare, 5) != 0) {
        step(tortoise); step(hare); mu++;
    }
}

struct Result {
    uint32_t n;
    uint8_t seed[5];
    uint64_t tail;
    uint64_t cycle_len;
};

int main(int argc, char* argv[]) {
    uint32_t max_n = 5184000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            max_n = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--test") == 0)
            max_n = 1000;
        else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--max N] [--test]\n", argv[0]);
            return 0;
        }
    }

    printf("Scanning RND(-1) through RND(-%u)...\n", max_n);

    auto t0 = std::chrono::steady_clock::now();

    // Track unique seeds to skip duplicates
    std::unordered_set<uint64_t> seen_seeds;
    // All known states that lie on any discovered cycle, mapped to cycle index
    std::unordered_map<uint64_t, size_t> known_cycle_states;
    // All visited states (tails + cycles) — just need membership test
    std::unordered_set<uint64_t> all_visited;
    // All discovered cycles: index -> (length, first_n)
    struct CycleInfo { uint64_t length; uint32_t first_n; };
    std::vector<CycleInfo> cycles;
    // All results (only store the interesting ones to save memory)
    std::vector<Result> top_results;
    uint64_t best_cycle = 0;
    uint64_t skipped = 0;
    uint64_t total_unique = 0;
    uint64_t total_steps = 0;
    uint64_t brent_runs = 0;
    uint64_t fast_lookups = 0;

    // Helper: add all states on a cycle to known_cycle_states + all_visited
    auto register_cycle = [&](const uint8_t entry[5], uint64_t lambda, size_t cycle_idx) {
        uint8_t st[5];
        memcpy(st, entry, 5);
        for (uint64_t i = 0; i < lambda; i++) {
            uint64_t e = encode(st);
            known_cycle_states[e] = cycle_idx;
            all_visited.insert(e);
            step(st);
        }
    };

    for (uint32_t n = 1; n <= max_n; n++) {
        uint8_t seed[5];
        rnd_neg_seed(n, seed);
        uint64_t enc = encode(seed);

        if (enc == 0) { skipped++; continue; }
        if (seen_seeds.count(enc)) { skipped++; continue; }
        seen_seeds.insert(enc);
        total_unique++;

        uint64_t tail = 0;
        uint64_t cycle_len = 0;

        if (!all_visited.empty()) {
            // Fast path: walk until we hit ANY known state (tail or cycle)
            uint8_t st[5];
            memcpy(st, seed, 5);
            std::vector<uint64_t> trail;
            trail.reserve(256);
            bool found = false;
            for (uint64_t i = 0; i < 100000000ULL; i++) {
                uint64_t e = encode(st);
                if (all_visited.count(e)) {
                    // We hit a known state. Walk from here to find which cycle.
                    // (The hit state might be on a tail, so keep walking until
                    // we reach an actual cycle state.)
                    uint64_t extra = 0;
                    while (!known_cycle_states.count(e) && extra < 100000000ULL) {
                        step(st);
                        e = encode(st);
                        extra++;
                    }
                    if (known_cycle_states.count(e)) {
                        size_t cid = known_cycle_states[e];
                        tail = i;
                        cycle_len = cycles[cid].length;
                        found = true;
                        total_steps += i + extra;
                        fast_lookups++;
                        // Add our trail to all_visited for future seeds
                        for (auto s : trail) all_visited.insert(s);
                    }
                    break;
                }
                trail.push_back(e);
                step(st);
            }
            if (found) {
                if (cycle_len > best_cycle) {
                    best_cycle = cycle_len;
                    printf("  NEW BEST: RND(-%u) -> cycle=%llu tail=%llu\n",
                           n, (unsigned long long)cycle_len, (unsigned long long)tail);
                }
                if (cycle_len >= 1000) {
                    top_results.push_back({n, {seed[0],seed[1],seed[2],seed[3],seed[4]}, tail, cycle_len});
                }
                goto next_n;
            }
            // If we walked 100M steps and didn't hit anything known, fall through to Brent's
        }

        {
            // Slow path: full Brent's cycle detection
            uint64_t mu, lambda;
            brent(seed, mu, lambda);
            brent_runs++;
            total_steps += mu + lambda;
            tail = mu;
            cycle_len = lambda;

            // Find the cycle entry point, adding tail states to all_visited
            uint8_t entry[5];
            memcpy(entry, seed, 5);
            for (uint64_t i = 0; i < mu; i++) {
                all_visited.insert(encode(entry));
                step(entry);
            }

            // Check if this is a known cycle length (different entry to same cycle)
            // by checking if entry is already known
            uint64_t entry_enc = encode(entry);
            if (!known_cycle_states.count(entry_enc)) {
                // New cycle discovered
                size_t cid = cycles.size();
                cycles.push_back({lambda, n});
                printf("  NEW CYCLE #%zu: len=%llu from RND(-%u) (Brent's run #%llu)\n",
                       cid, (unsigned long long)lambda, n, (unsigned long long)brent_runs);
                register_cycle(entry, lambda, cid);
                printf("    Registered %llu cycle states (%zu total known)\n",
                       (unsigned long long)lambda, known_cycle_states.size());
            }

            if (cycle_len > best_cycle) {
                best_cycle = cycle_len;
                printf("  NEW BEST: RND(-%u) -> cycle=%llu tail=%llu\n",
                       n, (unsigned long long)cycle_len, (unsigned long long)tail);
            }
            if (cycle_len >= 1000) {
                top_results.push_back({n, {seed[0],seed[1],seed[2],seed[3],seed[4]}, tail, cycle_len});
            }
        }

        next_n:
        // Progress
        if (n % 100000 == 0) {
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration<double>(now - t0).count();
            double rate = n / secs;
            double eta = (max_n - n) / rate;
            printf("[%7u/%u] %.0fs | %llu unique | %zu cycles | best=%llu | %llu brents %llu fast | %.0f seeds/s | ETA %.0fs\n",
                   n, max_n, secs, (unsigned long long)total_unique,
                   cycles.size(), (unsigned long long)best_cycle,
                   (unsigned long long)brent_runs, (unsigned long long)fast_lookups,
                   rate, eta);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double total_secs = std::chrono::duration<double>(t1 - t0).count();

    printf("\n=== RESULTS ===\n\n");
    printf("Time: %.1f seconds (%.1f minutes)\n", total_secs, total_secs / 60.0);
    printf("Seeds tested: %u\n", max_n);
    printf("Unique seeds: %llu\n", (unsigned long long)total_unique);
    printf("Skipped (dupes/zero): %llu\n", (unsigned long long)skipped);
    printf("Distinct cycles: %zu\n", cycles.size());
    printf("Brent's runs: %llu\n", (unsigned long long)brent_runs);
    printf("Fast lookups: %llu\n", (unsigned long long)fast_lookups);
    printf("Total steps: %llu\n", (unsigned long long)total_steps);
    printf("Best cycle: %llu\n\n", (unsigned long long)best_cycle);

    // Sort top results by cycle length
    std::sort(top_results.begin(), top_results.end(),
              [](const Result& a, const Result& b) { return a.cycle_len > b.cycle_len; });

    printf("Top 20 seeds by cycle length:\n\n");
    printf("  Rank | RND(-N)     |  Cycle len |     Tail | Seed\n");
    printf("  -----+-------------+------------+----------+------------------\n");
    size_t show = std::min(top_results.size(), (size_t)20);
    for (size_t i = 0; i < show; i++) {
        auto& r = top_results[i];
        printf("  %4zu | RND(-%6u) | %10llu | %8llu | %02X %02X %02X %02X %02X\n",
               i + 1, r.n,
               (unsigned long long)r.cycle_len, (unsigned long long)r.tail,
               r.seed[0], r.seed[1], r.seed[2], r.seed[3], r.seed[4]);
    }

    printf("\nAll distinct cycles found:\n");
    for (size_t i = 0; i < cycles.size(); i++) {
        printf("  #%zu: length=%llu  first_n=%u\n",
               i, (unsigned long long)cycles[i].length, cycles[i].first_n);
    }

    return 0;
}
