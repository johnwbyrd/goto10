#pragma once
#include <cstdint>
#include <array>

// Bit-exact reimplementation of the C64 BASIC RND function.

class C64Rnd {
public:
    using Seed = std::array<uint8_t, 5>;  // [exp, m1, m2, m3, m4] packed MBF at $8B-$8F

    C64Rnd() : seed_{0x80, 0x4F, 0xC7, 0x52, 0x58} {}  // ROM default seed
    explicit C64Rnd(Seed s) : seed_(s) {}

    double next();       // Advance RNG, return value in [0, 1). Equivalent to RND(1).
    bool next_slash();   // Advance RNG, return true for '\' (CHR$205), false for '/' (CHR$206).

    Seed seed() const { return seed_; }
    void set_seed(Seed s) { seed_ = s; }

private:
    struct FAC {
        uint8_t exp;   // $61
        uint8_t m1;    // $62 FACHO (MSB, implicit high bit set when unpacked)
        uint8_t m2;    // $63 FACMOH
        uint8_t m3;    // $64 FACMO
        uint8_t m4;    // $65 FACLO (LSB)
        uint8_t sign;  // $66 FACSGN
        uint8_t ov;    // $70 FACOV (rounding byte)
    };

    static FAC movfm(const uint8_t* mem);                // code20 line 11
    static void movmf(const FAC& fac, uint8_t* mem);     // code20 line 39
    static void fmult(FAC& fac, const uint8_t* mem);     // code19 line 30
    static void fadd(FAC& fac, const uint8_t* mem);      // code17 line 99
    static void normalize(FAC& fac);                      // code17 line 153
    static double fac_to_double(const FAC& fac);

    Seed seed_;
    static constexpr uint8_t RMULZC[5] = {0x98, 0x35, 0x44, 0x7A, 0x00};
    static constexpr uint8_t RADDZC[5] = {0x68, 0x28, 0xB1, 0x46, 0x00};
};
