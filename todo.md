# Current Status and Open Questions

## Verified

- **C64Rnd simulation matches VICE for 1,000,000 iterations** of pure RND(1), validated byte-for-byte against the actual C64 ROM running in VICE 3.9.
- **100,000 seeds from the actual BASIC `10 PRINT` program** match the simulation, confirming that BASIC's float operations between RND calls do not affect the seed sequence.
- **MOVFM clears FACOV** (code20 line 30), making RND a pure function of its 5-byte seed regardless of calling context.
- **12 distinct cycles** found by scanning 432,000 integer seeds (RND(-1) through RND(-432000)). The longest is 58,078.
- **Two FMULT bugs** discovered and replicated: the carry leak between zero multiplier bytes, and the non-trivial FADD contribution at low exponents. Both are documented in `doc/fmult_carry_bug.md`.

## Open

1. **Full jiffy timer scan.** Scanning all 5,184,000 seeds (24 hours of TI values) to confirm no additional cycles exist beyond the 12 found. The threaded scanner (`best_seed.cpp`) can do this but takes ~10 minutes and ~4GB of RAM.

2. **Non-integer seeds.** RND(0) reads CIA timer registers directly, producing arbitrary 32-bit mantissa values that integer seeds cannot reach. These might access cycles not found by the integer scan. Random sampling of the full 5-byte state space would test this.

3. **Algebraic characterization.** Why exactly 12 cycles? The empirical analysis shows zero-byte patterns don't distinguish cycles. The mechanism partitioning the state space into 12 basins of attraction is not understood. See `doc/algebraic_analysis.md`.

4. **Longest tail.** The `best_seed` scanner tracks distance-to-cycle for every visited state. The seed with the longest tail (most unique values before repeating) gives the most non-repeating randomness achievable from a single `RND(-TI)` call.

5. **Cross-platform comparison.** The FMULT carry leak exists in all Microsoft BASIC 6502 implementations, but the byte swap and constant width differ across platforms. Applesoft, PET, and VIC-20 would have different cycle structures.
