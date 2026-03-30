#pragma once
#include <cstdint>
#include <array>

// Bit-exact reimplementation of the Commodore 64 BASIC RND function.
//
// The C64 uses Microsoft Binary Format (MBF) 5-byte floats and a
// shift-and-add multiplier. This class reproduces the exact sequence
// of values that a real C64 produces when calling RND(1) repeatedly
// from the default power-on seed.
//
// Reference: C64 BASIC ROM disassembly at $E097-$E0F6
//   https://www.pagetable.com/c64ref/c64disasm/#E097

class C64Rnd {
public:
    // The 5-byte MBF seed as stored in memory at $8B-$8F.
    // Format: [exponent, mantissa1, mantissa2, mantissa3, mantissa4]
    // In memory, bit 7 of mantissa1 holds the sign (0=positive).
    using Seed = std::array<uint8_t, 5>;

    // Default power-on seed: $80 $4F $C7 $52 $58
    C64Rnd() : seed_{0x80, 0x4F, 0xC7, 0x52, 0x58} {}

    // Initialize with a custom seed (5 bytes, MBF packed format).
    explicit C64Rnd(Seed s) : seed_(s) {}

    // Advance the PRNG and return the result as a float in [0, 1).
    // This is equivalent to calling RND(1) on the C64.
    double next();

    // Advance the PRNG and return true (CHR$ 206, '\') or false (CHR$ 205, '/').
    // This is the core decision in: 10 PRINT CHR$(205.5+RND(1)); : GOTO 10
    bool next_slash();

    // Get current seed state (for inspection/serialization).
    Seed seed() const { return seed_; }

    // Set seed state.
    void set_seed(Seed s) { seed_ = s; }

private:
    // FAC1 (Floating Point Accumulator) - unpacked working registers
    struct FAC {
        uint8_t exp;   // $61 FACEXP
        uint8_t m1;    // $62 FACHO  (mantissa byte 1, MSB, implicit high bit set)
        uint8_t m2;    // $63 FACMOH (mantissa byte 2)
        uint8_t m3;    // $64 FACMO  (mantissa byte 3)
        uint8_t m4;    // $65 FACLO  (mantissa byte 4, LSB)
        uint8_t sign;  // $66 FACSGN
        uint8_t ov;    // $70 FACOV  (rounding byte)
    };

    // Unpack 5-byte MBF from memory into FAC (MOVFM, $BBA2)
    static FAC movfm(const uint8_t* mem);

    // Pack FAC back into 5-byte MBF memory (MOVMF, $BBD4)
    static void movmf(const FAC& fac, uint8_t* mem);

    // Multiply FAC by 5-byte MBF constant in memory (FMULT, $BA28)
    static void fmult(FAC& fac, const uint8_t* mem);

    // Add 5-byte MBF constant in memory to FAC (FADD, $B867)
    static void fadd(FAC& fac, const uint8_t* mem);

    // Normalize FAC (NORMAL, $B8D7)
    static void normalize(FAC& fac);

    // Convert FAC to double for output
    static double fac_to_double(const FAC& fac);

    Seed seed_;

    // ROM constants used by RND
    static constexpr uint8_t RMULZC[5] = {0x98, 0x35, 0x44, 0x7A, 0x00}; // 11879546
    static constexpr uint8_t RADDZC[5] = {0x68, 0x28, 0xB1, 0x46, 0x00}; // 3.927677739E-8
};
