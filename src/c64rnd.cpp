#include "c64rnd.h"

C64Rnd::C64Rnd() {
    seed_ = {0x80, 0x4F, 0xC7, 0x52, 0x58};
}

C64Rnd::C64Rnd(Seed s) {
    seed_ = s;
}

void C64Rnd::fmult(FAC& fac) {
    fmult_6502(fac, RMULZC);
}

void C64Rnd::fadd(FAC& fac) {
    fadd_6502(fac, RADDZC);
}

// Full 4-byte mantissa reversal (Commodore REALIO=3).
void C64Rnd::byte_swap(FAC& fac) {
    uint8_t t;
    t = fac.m1; fac.m1 = fac.m4; fac.m4 = t;
    t = fac.m2; fac.m2 = fac.m3; fac.m3 = t;
}
