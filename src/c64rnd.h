#pragma once
#include "msbasicfprnd.h"

// Bit-exact reimplementation of the C64 BASIC V2 RND function.
// ROM revision 901227-03. Validated against VICE 3.9 for 1,000,000 iterations.
//
// C64-specific properties:
//   - Fixed 5-byte constants (Commodore added $00 trailing byte)
//   - Full 4-byte mantissa reversal (REALIO=3)
//   - FMULT carry leak (6502 artifact, shared with all 6502 platforms)
//   - FADD rounding byte carry propagation at low exponents
//   - Default seed: $80 $4F $C7 $52 $58

class C64Rnd : public MsBasicFpRnd {
public:
    C64Rnd();
    explicit C64Rnd(Seed s);

    const char* name() const override { return "C64 BASIC V2"; }

protected:
    void fmult(FAC& fac) override;
    void fadd(FAC& fac) override;
    void byte_swap(FAC& fac) override;

private:
    static constexpr uint8_t RMULZC[5] = {0x98, 0x35, 0x44, 0x7A, 0x00};
    static constexpr uint8_t RADDZC[5] = {0x68, 0x28, 0xB1, 0x46, 0x00};
};
