#pragma once
#include "msbasicfprnd.h"

// Applesoft BASIC RND implementation.
//
// Applesoft uses 5-byte MBF floats but was never patched to use 5-byte
// constants. The ROM stores RMULZC and RADDZC as 4 bytes each, adjacent
// in memory. FMULT reads 5 bytes starting at RMULZC, picking up the
// first byte of RADDZC ($68) as the 5th multiplier byte. FADD reads
// 5 bytes starting at RADDZC, picking up whatever byte follows in ROM.
//
// Byte swap: M1<->M4 only (non-Commodore, REALIO != 3).
//
// NOT YET VALIDATED against an emulator. Constants marked TODO where
// the actual ROM value is unverified.

class ApplesoftRnd : public MsBasicFpRnd {
public:
    ApplesoftRnd();
    explicit ApplesoftRnd(Seed s);

    const char* name() const override { return "Applesoft BASIC"; }

protected:
    void fmult(FAC& fac) override;
    void fadd(FAC& fac) override;
    void byte_swap(FAC& fac) override;

private:
    // Unfixed: 5th byte of multiplier is RADDZC[0] ($68).
    static constexpr uint8_t RMULZC[5] = {0x98, 0x35, 0x44, 0x7A, 0x68};
    // TODO: 5th byte depends on what follows RADDZC in Applesoft ROM.
    // Using $00 as placeholder until verified against ROM dump.
    static constexpr uint8_t RADDZC[5] = {0x68, 0x28, 0xB1, 0x46, 0x00};
};
