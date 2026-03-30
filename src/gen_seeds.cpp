// Generate the same output as rnd_spy.s: BITS FACOV SEED[5] in hex.
// Since MOVFM clears FACOV and RND doesn't touch BITS, both are always 00.
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
