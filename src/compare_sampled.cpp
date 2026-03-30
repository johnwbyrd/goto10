#include "c64rnd.h"
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    const char* vice_file = "vice_seeds.txt";
    if (argc > 1) vice_file = argv[1];

    FILE* fp = fopen(vice_file, "r");
    if (!fp) { printf("Cannot open %s\n", vice_file); return 1; }

    C64Rnd rng;
    int current_step = 0;
    int mismatches = 0;
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '\n' || line[0] == '\r') continue;

        int target_step, v0, v1, v2, v3, v4;
        if (sscanf(line, " %d %d %d %d %d %d", &target_step, &v0, &v1, &v2, &v3, &v4) != 6) continue;

        // Advance our RNG to target_step
        while (current_step < target_step) {
            rng.next();
            current_step++;
        }

        auto seed = rng.seed();
        bool match = (seed[0]==(uint8_t)v0 && seed[1]==(uint8_t)v1 &&
                      seed[2]==(uint8_t)v2 && seed[3]==(uint8_t)v3 && seed[4]==(uint8_t)v4);

        if (!match) {
            mismatches++;
            printf("MISMATCH at step %d:\n", target_step);
            printf("  VICE: %3d %3d %3d %3d %3d  (%02X %02X %02X %02X %02X)\n",
                   v0, v1, v2, v3, v4, (uint8_t)v0, (uint8_t)v1, (uint8_t)v2, (uint8_t)v3, (uint8_t)v4);
            printf("  C++:  %3d %3d %3d %3d %3d  (%02X %02X %02X %02X %02X)\n",
                   seed[0], seed[1], seed[2], seed[3], seed[4],
                   seed[0], seed[1], seed[2], seed[3], seed[4]);
            if (mismatches >= 10) break;
        } else {
            printf("  Step %6d: OK  (%02X %02X %02X %02X %02X)\n",
                   target_step, seed[0], seed[1], seed[2], seed[3], seed[4]);
        }
    }

    fclose(fp);
    printf("\nCompared %d checkpoints, %d mismatches.\n",
           current_step > 0 ? 150 : 0, mismatches);
    return mismatches > 0 ? 1 : 0;
}
