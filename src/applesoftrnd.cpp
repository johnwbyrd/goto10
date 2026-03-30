#include "applesoftrnd.h"

// TODO: Verify default seed from Applesoft ROM dump.
// Using C64 seed as placeholder -- may differ.
ApplesoftRnd::ApplesoftRnd() {
    seed_ = {0x80, 0x4F, 0xC7, 0x52, 0x58};
}

ApplesoftRnd::ApplesoftRnd(Seed s) {
    seed_ = s;
}

void ApplesoftRnd::fmult(FAC& fac) {
    fmult_6502(fac, RMULZC);
}

void ApplesoftRnd::fadd(FAC& fac) {
    fadd_6502(fac, RADDZC);
}

// 2-swap only: M1<->M4, M2 and M3 stay in place.
void ApplesoftRnd::byte_swap(FAC& fac) {
    uint8_t t = fac.m1;
    fac.m1 = fac.m4;
    fac.m4 = t;
}
