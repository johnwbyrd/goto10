#pragma once
#include <cstdint>
#include <array>

// Abstract base class for all Microsoft BASIC RND implementations.
// Guarantees only what is universal across all platforms, regardless
// of CPU architecture (6502, Z80, 8080) or float width (4-byte, 5-byte MBF).

class MsBasicRnd {
public:
    static constexpr int MAX_SEED_BYTES = 5;
    using Seed = std::array<uint8_t, MAX_SEED_BYTES>;

    virtual ~MsBasicRnd() = default;

    virtual double next() = 0;
    bool next_slash() { return next() < 0.5; }

    virtual Seed seed() const = 0;
    virtual void set_seed(Seed s) = 0;
    virtual int seed_bytes() const = 0;

    virtual const char* name() const = 0;
};
