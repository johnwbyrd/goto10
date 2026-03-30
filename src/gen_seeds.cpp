// gen_seeds.cpp — Generate RND(1) seed sequence in hex for validation.
//
// Output format matches rnd_spy.s: "BITS FACOV EXP M1 M2 M3 M4" per line.
// BITS and FACOV are always 00 (MOVFM clears FACOV; RND doesn't touch BITS).
// Used by diffing against VICE output to validate the C++ simulation.
//
// Usage: gen_seeds [count]
#include "c64rnd.h"
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    int count = 50000;
    if (argc > 1) count = atoi(argv[1]);

    C64Rnd rng;
    for (int i = 0; i < count; i++) {
        rng.next();
        auto s = rng.seed();
        printf("00 00 %02X %02X %02X %02X %02X \n",
               s[0], s[1], s[2], s[3], s[4]);
    }
    return 0;
}
