#include "c64rnd.h"
#include <cstdio>
#include <cstring>

int main() {
    C64Rnd rng;

    printf("C64 RND(1) sequence from default seed:\n");
    printf("Seed: %02X %02X %02X %02X %02X\n\n",
           rng.seed()[0], rng.seed()[1], rng.seed()[2],
           rng.seed()[3], rng.seed()[4]);

    // Print the first 20 values
    for (int i = 1; i <= 20; i++) {
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

    // --- Cycle verification ---
    // Brent's algorithm found: tail (mu) = 71549, cycle (lambda) = 46813
    // Verify by comparing the output at position mu with position mu+lambda.
    printf("\n--- Cycle verification ---\n");
    const int mu = 71549;
    const int lambda = 46813;

    // Generate characters 0..mu+lambda+99, store the relevant windows
    rng = C64Rnd();

    // Store seed at position mu
    C64Rnd::Seed seed_at_mu{};
    // Store 100 chars starting at mu, and 100 chars starting at mu+lambda
    char chars_at_mu[100];
    char chars_at_mu_plus_lambda[100];

    for (int i = 0; i < mu + lambda + 100; i++) {
        if (i == mu) {
            seed_at_mu = rng.seed();
        }

        bool slash = rng.next_slash();
        char ch = slash ? '\\' : '/';

        if (i >= mu && i < mu + 100) {
            chars_at_mu[i - mu] = ch;
        }
        if (i >= mu + lambda && i < mu + lambda + 100) {
            chars_at_mu_plus_lambda[i - mu - lambda] = ch;
        }
    }

    printf("Seed at position %d: %02X %02X %02X %02X %02X\n",
           mu, seed_at_mu[0], seed_at_mu[1], seed_at_mu[2],
           seed_at_mu[3], seed_at_mu[4]);

    // Check if seed at mu+lambda matches seed at mu
    rng = C64Rnd();
    C64Rnd::Seed seed_at_mu_lambda{};
    for (int i = 0; i < mu + lambda; i++) {
        rng.next();
    }
    seed_at_mu_lambda = rng.seed();

    printf("Seed at position %d: %02X %02X %02X %02X %02X\n",
           mu + lambda, seed_at_mu_lambda[0], seed_at_mu_lambda[1],
           seed_at_mu_lambda[2], seed_at_mu_lambda[3], seed_at_mu_lambda[4]);

    bool seeds_match = memcmp(seed_at_mu.data(), seed_at_mu_lambda.data(), 5) == 0;
    printf("Seeds match: %s\n", seeds_match ? "YES" : "NO");

    bool chars_match = memcmp(chars_at_mu, chars_at_mu_plus_lambda, 100) == 0;
    printf("Output chars [mu..mu+99] == [mu+lambda..mu+lambda+99]: %s\n",
           chars_match ? "YES" : "NO");

    if (chars_match) {
        printf("\nCONFIRMED: The maze pattern repeats with period %d\n", lambda);
        printf("after a tail of %d non-repeating characters.\n", mu);
        printf("\nFirst 80 chars of the cycle:\n  ");
        for (int i = 0; i < 80; i++) printf("%c", chars_at_mu[i]);
        printf("\nSame 80 chars, one period later:\n  ");
        for (int i = 0; i < 80; i++) printf("%c", chars_at_mu_plus_lambda[i]);
        printf("\n");
    } else {
        printf("\nFAILED: Output does not repeat at the expected period.\n");
        printf("Chars at mu:          ");
        for (int i = 0; i < 40; i++) printf("%c", chars_at_mu[i]);
        printf("\nChars at mu+lambda:   ");
        for (int i = 0; i < 40; i++) printf("%c", chars_at_mu_plus_lambda[i]);
        printf("\n");
    }

    return 0;
}
