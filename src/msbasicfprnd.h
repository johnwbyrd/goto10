#pragma once
#include "msbasicrnd.h"

// Intermediate class for Microsoft BASIC RND on 6502 platforms with
// 5-byte MBF floats.
//
// Owns the floating-point engine state: seed, and any hidden state
// discovered through ROM analysis (BITS, FACOV carry-over, etc.).
//
// Provides default virtual implementations for algorithm steps that
// are identical across 6502 platforms. Steps that require platform-
// specific constants or behavior are pure virtual.
//
// next() is the template method: calls the steps in ROM order.

class MsBasicFpRnd : public MsBasicRnd {
public:
    Seed seed() const override { return seed_; }
    void set_seed(Seed s) override { seed_ = s; }
    int seed_bytes() const override { return 5; }

    double next() override;

    struct FAC {
        uint8_t exp;   // $61 FACEXP
        uint8_t m1;    // $62 FACHO (MSB, implicit high bit set when unpacked)
        uint8_t m2;    // $63 FACMOH
        uint8_t m3;    // $64 FACMO
        uint8_t m4;    // $65 FACLO (LSB)
        uint8_t sign;  // $66 FACSGN
        uint8_t ov;    // $70 FACOV (rounding byte)
    };

protected:
    // Shared across all 6502 5-byte platforms -- override if a ROM differs.
    virtual FAC movfm(const uint8_t* mem);
    virtual void movmf(const FAC& fac, uint8_t* mem);
    virtual void normalize(FAC& fac);
    virtual double fac_to_double(const FAC& fac);

    // Platform-specific -- each concrete class must implement.
    virtual void fmult(FAC& fac) = 0;
    virtual void fadd(FAC& fac) = 0;
    virtual void byte_swap(FAC& fac) = 0;

    // Shared 6502 FMULT/FADD implementation including the carry leak.
    // Concrete classes call these with their own constants.
    void fmult_6502(FAC& fac, const uint8_t* constant);
    void fadd_6502(FAC& fac, const uint8_t* constant);

    Seed seed_{};
};
