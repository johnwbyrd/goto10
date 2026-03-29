#include "c64rnd.h"
#include <cstdio>

int main() {
    C64Rnd rng;

    printf("C64 RND(1) sequence from default seed:\n");
    printf("Seed: %02X %02X %02X %02X %02X\n\n",
           rng.seed()[0], rng.seed()[1], rng.seed()[2],
           rng.seed()[3], rng.seed()[4]);

    // Print the first 20 values
    for (int i = 1; i <= 20; i++) {
        auto before = rng.seed();
        double val = rng.next();
        auto after = rng.seed();
        printf("RND #%2d: %.10f  seed: %02X %02X %02X %02X %02X\n",
               i, val,
               after[0], after[1], after[2], after[3], after[4]);
    }

    // Known C64 RND(1) values from the default seed (first call):
    // The first RND(1) from a cold boot should be approximately 0.185564016
    // (This is a widely-verified value from C64 emulators)
    printf("\n--- Verification ---\n");
    printf("Expected first RND(1) ~ 0.185564016\n");

    // Reset and check
    rng = C64Rnd();
    double first = rng.next();
    printf("Got:                    %.10f\n", first);

    // Print first 1000 characters of the 10 PRINT maze
    printf("\n--- 10 PRINT output (first 1000 chars, 40 cols) ---\n");
    rng = C64Rnd();
    for (int i = 0; i < 1000; i++) {
        if (i > 0 && i % 40 == 0) printf("\n");
        printf("%c", rng.next_slash() ? '\\' : '/');
    }
    printf("\n");

    return 0;
}
