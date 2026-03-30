// Scan RND(-N) seeds with dynamic programming and threading.
// Shared known-state map with readers-writer lock.

#include "c64rnd.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <atomic>

static inline uint64_t encode(const C64Rnd::Seed& s) {
    return ((uint64_t)s[0] << 32) | ((uint64_t)s[1] << 24) |
           ((uint64_t)s[2] << 16) | ((uint64_t)s[3] << 8) | s[4];
}

static C64Rnd::Seed rnd_neg_seed(uint32_t n) {
    C64Rnd::Seed seed = {0, 0, 0, 0, 0};
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

// Shared state
struct CycleInfo { uint64_t length; uint32_t first_n; };
static std::unordered_map<uint64_t, size_t> g_known;
static std::vector<CycleInfo> g_cycles;
static std::shared_mutex g_mutex;
static std::atomic<uint64_t> g_best_cycle{0};
static std::atomic<uint64_t> g_seeds_done{0};

// Look up a state. Returns cycle index or SIZE_MAX if unknown.
static size_t lookup(uint64_t enc) {
    std::shared_lock lock(g_mutex);
    auto it = g_known.find(enc);
    return (it != g_known.end()) ? it->second : SIZE_MAX;
}

// Add a batch of states to the known map, all tagged with cycle_idx.
static void add_known(const std::vector<uint64_t>& states, size_t cycle_idx) {
    std::unique_lock lock(g_mutex);
    for (auto e : states) g_known[e] = cycle_idx;
}

// Register a new cycle. Returns the cycle index.
static size_t register_cycle(uint64_t length, uint32_t first_n, const C64Rnd::Seed& entry) {
    std::unique_lock lock(g_mutex);
    size_t idx = g_cycles.size();
    g_cycles.push_back({length, first_n});
    // Add all cycle states
    C64Rnd cyc(entry);
    for (uint64_t i = 0; i < length; i++) {
        g_known[encode(cyc.seed())] = idx;
        cyc.next();
    }
    return idx;
}

static void worker(uint32_t start, uint32_t end) {
    for (uint32_t n = start; n <= end; n++) {
        auto seed = rnd_neg_seed(n);
        uint64_t enc = encode(seed);
        if (enc == 0) { g_seeds_done++; continue; }

        // Quick check
        size_t idx = lookup(enc);
        if (idx != SIZE_MAX) { g_seeds_done++; continue; }

        // Walk forward collecting trail
        C64Rnd rng(seed);
        std::vector<uint64_t> trail;
        trail.push_back(enc);
        size_t cycle_idx = SIZE_MAX;

        for (uint64_t step = 0; step < 10000000; step++) {
            rng.next();
            uint64_t e = encode(rng.seed());
            idx = lookup(e);
            if (idx != SIZE_MAX) {
                cycle_idx = idx;
                break;
            }
            trail.push_back(e);
        }

        if (cycle_idx == SIZE_MAX) {
            // New cycle. Run Brent's.
            uint64_t mu, lambda;
            brent(seed, mu, lambda);

            // Find cycle entry
            C64Rnd walker(seed);
            for (uint64_t i = 0; i < mu; i++) walker.next();

            cycle_idx = register_cycle(lambda, n, walker.seed());

            // Add tail states
            std::vector<uint64_t> tail_states;
            C64Rnd tw(seed);
            for (uint64_t i = 0; i < mu; i++) {
                tail_states.push_back(encode(tw.seed()));
                tw.next();
            }
            add_known(tail_states, cycle_idx);

            uint64_t prev = g_best_cycle.load();
            while (lambda > prev && !g_best_cycle.compare_exchange_weak(prev, lambda));

            printf("  NEW CYCLE #%zu: len=%llu from RND(-%u)\n",
                   cycle_idx, (unsigned long long)lambda, n);
        } else {
            // Tag trail with known cycle
            add_known(trail, cycle_idx);
        }

        g_seeds_done++;
    }
}

int main(int argc, char* argv[]) {
    uint32_t max_n = 216000;  // 1 hour of jiffies
    unsigned num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            max_n = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc)
            num_threads = (unsigned)atoi(argv[++i]);
    }

    printf("Scanning RND(-1) through RND(-%u) with %u threads...\n", max_n, num_threads);
    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    uint32_t slice = max_n / num_threads;
    for (unsigned t = 0; t < num_threads; t++) {
        uint32_t s = t * slice + 1;
        uint32_t e = (t == num_threads - 1) ? max_n : (t + 1) * slice;
        threads.emplace_back(worker, s, e);
    }

    // Progress monitor
    while (true) {
        bool all_done = true;
        for (auto& th : threads) if (th.joinable()) { all_done = false; break; }
        if (all_done) break;

        uint64_t done = g_seeds_done.load();
        if (done >= max_n) break;

        auto now = std::chrono::steady_clock::now();
        double secs = std::chrono::duration<double>(now - t0).count();
        size_t nknown, ncycles;
        { std::shared_lock lock(g_mutex); nknown = g_known.size(); ncycles = g_cycles.size(); }
        printf("[%.0fs] %llu/%u seeds | %zu known | %zu cycles | best=%llu\n",
               secs, (unsigned long long)done, max_n, nknown, ncycles,
               (unsigned long long)g_best_cycle.load());

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    for (auto& th : threads) th.join();

    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    printf("\n=== RESULTS ===\n");
    printf("Time: %.1f seconds\n", secs);
    printf("Seeds tested: %u\n", max_n);
    printf("Known states: %zu\n", g_known.size());
    printf("Best cycle: %llu\n\n", (unsigned long long)g_best_cycle.load());

    std::sort(g_cycles.begin(), g_cycles.end(),
              [](const CycleInfo& a, const CycleInfo& b) { return a.length > b.length; });
    printf("All distinct cycles:\n");
    for (auto& c : g_cycles)
        printf("  cycle=%llu  first_n=%u\n", (unsigned long long)c.length, c.first_n);

    return 0;
}
