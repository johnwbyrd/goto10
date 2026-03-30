// best_seed.cpp — Scan RND(-N) seeds to find all reachable cycles.
//
// For each integer N (simulating RND(-TI) where TI is the jiffy timer),
// computes the seed, walks forward until it hits a known state, and
// records which cycle it reaches and how long the tail is.
//
// Uses a shared hash map of all visited states with a readers-writer lock
// for thread safety. New cycles are discovered via Brent's algorithm.
// After the first few hundred seeds populate the map, most lookups
// terminate in 1-2 steps.
//
// Usage: best_seed [--max N] [--threads N]
//   --max N       Scan RND(-1) through RND(-N). Default: 216000 (1 hour).
//   --threads N   Worker threads. Default: hardware concurrency.

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

struct StateInfo {
    uint32_t cycle_id;
    uint32_t distance;  // steps from this state to the cycle entry point
};

struct CycleInfo { uint64_t length; uint32_t first_n; };

static std::unordered_map<uint64_t, StateInfo> g_known;
static std::vector<CycleInfo> g_cycles;
static std::shared_mutex g_mutex;
static std::atomic<uint64_t> g_best_cycle{0};
static std::atomic<uint64_t> g_best_total{0};  // best tail + cycle
static std::atomic<uint32_t> g_best_total_n{0};
static std::atomic<uint64_t> g_seeds_done{0};

struct LookupResult { bool found; uint32_t cycle_id; uint32_t distance; };

static LookupResult lookup(uint64_t enc) {
    std::shared_lock lock(g_mutex);
    auto it = g_known.find(enc);
    if (it != g_known.end())
        return {true, it->second.cycle_id, it->second.distance};
    return {false, 0, 0};
}

// Add trail states with computed distances. trail[0] = seed, trail[last] = one step before hit.
// hit_distance = distance of the state that was hit.
static void add_trail(const std::vector<uint64_t>& trail, uint32_t cycle_id, uint32_t hit_distance) {
    std::unique_lock lock(g_mutex);
    uint32_t n = (uint32_t)trail.size();
    for (uint32_t i = 0; i < n; i++) {
        uint32_t dist = (n - i) + hit_distance;
        g_known[trail[i]] = {cycle_id, dist};
    }
}

static size_t register_cycle(uint64_t length, uint32_t first_n, const C64Rnd::Seed& entry,
                              const C64Rnd::Seed& seed, uint64_t mu) {
    std::unique_lock lock(g_mutex);
    size_t idx = g_cycles.size();
    g_cycles.push_back({length, first_n});

    // Cycle states: distance = 0
    C64Rnd cyc(entry);
    for (uint64_t i = 0; i < length; i++) {
        g_known[encode(cyc.seed())] = {(uint32_t)idx, 0};
        cyc.next();
    }

    // Tail states: distance = mu - i
    C64Rnd tw(seed);
    for (uint64_t i = 0; i < mu; i++) {
        g_known[encode(tw.seed())] = {(uint32_t)idx, (uint32_t)(mu - i)};
        tw.next();
    }

    return idx;
}

static void worker(uint32_t start, uint32_t end) {
    for (uint32_t n = start; n <= end; n++) {
        auto seed = rnd_neg_seed(n);
        uint64_t enc = encode(seed);
        if (enc == 0) { g_seeds_done++; continue; }

        auto lr = lookup(enc);
        if (lr.found) {
            // Update best total
            uint64_t clen;
            { std::shared_lock lock(g_mutex); clen = g_cycles[lr.cycle_id].length; }
            uint64_t total = lr.distance + clen;
            uint64_t prev = g_best_total.load();
            while (total > prev && !g_best_total.compare_exchange_weak(prev, total)) {}
            if (total > prev) g_best_total_n.store(n);
            g_seeds_done++;
            continue;
        }

        // Walk forward
        C64Rnd rng(seed);
        std::vector<uint64_t> trail;
        trail.push_back(enc);
        uint32_t hit_cycle = UINT32_MAX;
        uint32_t hit_distance = 0;

        for (uint64_t step = 0; step < 10000000; step++) {
            rng.next();
            uint64_t e = encode(rng.seed());
            auto lr2 = lookup(e);
            if (lr2.found) {
                hit_cycle = lr2.cycle_id;
                hit_distance = lr2.distance;
                break;
            }
            trail.push_back(e);
        }

        if (hit_cycle == UINT32_MAX) {
            // New cycle
            uint64_t mu, lambda;
            brent(seed, mu, lambda);
            C64Rnd walker(seed);
            for (uint64_t i = 0; i < mu; i++) walker.next();
            size_t idx = register_cycle(lambda, n, walker.seed(), seed, mu);

            uint64_t prev = g_best_cycle.load();
            while (lambda > prev && !g_best_cycle.compare_exchange_weak(prev, lambda)) {}

            uint64_t total = mu + lambda;
            prev = g_best_total.load();
            while (total > prev && !g_best_total.compare_exchange_weak(prev, total)) {}
            if (total > prev) g_best_total_n.store(n);

            printf("  NEW CYCLE #%zu: len=%llu from RND(-%u) mu=%llu\n",
                   idx, (unsigned long long)lambda, n, (unsigned long long)mu);
        } else {
            add_trail(trail, hit_cycle, hit_distance);

            uint64_t clen;
            { std::shared_lock lock(g_mutex); clen = g_cycles[hit_cycle].length; }
            uint64_t tail = (uint32_t)trail.size() + hit_distance;
            uint64_t total = tail + clen;
            uint64_t prev = g_best_total.load();
            while (total > prev && !g_best_total.compare_exchange_weak(prev, total)) {}
            if (total > prev) g_best_total_n.store(n);
        }

        g_seeds_done++;
    }
}

int main(int argc, char* argv[]) {
    uint32_t max_n = 216000;
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

    std::atomic<bool> done{false};
    std::thread monitor([&]() {
        while (!done.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (done.load()) break;
            uint64_t d = g_seeds_done.load();
            auto now = std::chrono::steady_clock::now();
            double secs = std::chrono::duration<double>(now - t0).count();
            size_t nk, nc;
            { std::shared_lock lock(g_mutex); nk = g_known.size(); nc = g_cycles.size(); }
            printf("[%.0fs] %llu/%u | %zu known | %zu cycles | best_cycle=%llu best_total=%llu (n=%u)\n",
                   secs, (unsigned long long)d, max_n, nk, nc,
                   (unsigned long long)g_best_cycle.load(),
                   (unsigned long long)g_best_total.load(),
                   g_best_total_n.load());
        }
    });

    for (auto& th : threads) th.join();
    done.store(true);
    monitor.join();

    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    printf("\n=== RESULTS ===\n");
    printf("Time: %.1f seconds\n", secs);
    printf("Seeds: %u | Known states: %zu\n", max_n, g_known.size());
    printf("Best cycle: %llu\n", (unsigned long long)g_best_cycle.load());
    printf("Best total (tail+cycle): %llu from RND(-%u)\n",
           (unsigned long long)g_best_total.load(), g_best_total_n.load());

    printf("\nAll distinct cycles:\n");
    std::sort(g_cycles.begin(), g_cycles.end(),
              [](const CycleInfo& a, const CycleInfo& b) { return a.length > b.length; });
    for (auto& c : g_cycles)
        printf("  cycle=%llu  first_n=%u\n", (unsigned long long)c.length, c.first_n);

    return 0;
}
