#include "c64rnd.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char* argv[]) {
    const char* vice_file = "vice_seeds.txt";
    if (argc > 1) vice_file = argv[1];

    FILE* fp = fopen(vice_file, "r");
    if (!fp) {
        printf("Cannot open %s\n", vice_file);
        return 1;
    }

    C64Rnd rng;
    char line[256];
    int n = 0;
    int mismatches = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;

        // Parse 5 decimal byte values from VICE output
        // Format from BASIC PRINT#4: " 128 79 199 82 88" (space-separated with leading space)
        int v[5] = {};
        int parsed = sscanf(line, " %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4]);
        if (parsed != 5) {
            printf("Line %d: parse error (got %d values): %s", n + 1, parsed, line);
            continue;
        }

        // Advance our simulation
        rng.next();
        n++;
        auto seed = rng.seed();

        // Compare
        bool match = true;
        for (int i = 0; i < 5; i++) {
            if ((uint8_t)v[i] != seed[i]) { match = false; break; }
        }

        if (!match) {
            mismatches++;
            printf("MISMATCH at RND #%d:\n", n);
            printf("  VICE: %3d %3d %3d %3d %3d  (%02X %02X %02X %02X %02X)\n",
                   v[0], v[1], v[2], v[3], v[4],
                   (uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3], (uint8_t)v[4]);
            printf("  C++:  %3d %3d %3d %3d %3d  (%02X %02X %02X %02X %02X)\n",
                   seed[0], seed[1], seed[2], seed[3], seed[4],
                   seed[0], seed[1], seed[2], seed[3], seed[4]);
            if (mismatches >= 10) {
                printf("  (stopping after 10 mismatches)\n");
                break;
            }
        } else if (n <= 5 || n % 50 == 0) {
            printf("  RND #%d: OK  (%02X %02X %02X %02X %02X)\n",
                   n, seed[0], seed[1], seed[2], seed[3], seed[4]);
        }
    }

    fclose(fp);
    printf("\nCompared %d values, %d mismatches.\n", n, mismatches);
    return mismatches > 0 ? 1 : 0;
}
