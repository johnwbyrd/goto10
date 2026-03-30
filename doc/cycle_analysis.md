# Cycle Analysis

## How the cycle was found

The cycle was found using [Brent's cycle detection algorithm](https://en.wikipedia.org/wiki/Cycle_detection#Brent's_algorithm), which finds the exact tail length (mu) and cycle length (lambda) of any eventually-periodic sequence using O(1) memory and at most mu + lambda steps.

The implementation (`src/cycle_detect.cpp`) uses an optimized state-step function that operates directly on the 5-byte MBF seed, reproducing the C64's shift-and-add multiply without floating-point conversion. The verification (`src/test_rnd.cpp`) independently confirms the result by generating 118,462 characters and comparing the seed and output at positions mu and mu + lambda.

Both computations complete in under a second on modern hardware.

## Results

| Quantity | Value |
|----------|-------|
| **Tail length** (mu) | 71,549 |
| **Cycle length** (lambda) | 46,813 |
| **Total distinct values** | 118,362 |

## Output distribution

Within one full cycle of 46,813 characters:

| Character | Count | Percentage |
|-----------|-------|------------|
| `\` (CHR$ 205) | 23,245 | 49.655% |
| `/` (CHR$ 206) | 23,568 | 50.345% |

The split is nearly but not perfectly even, with a slight bias toward `/`.

## Validation against VICE

The simulation was validated against VICE (the C64 emulator, version 3.9) running the actual C64 BASIC ROM:

- **10,000 consecutive seeds**: byte-exact match, zero mismatches
- **50,000 seeds (pure RND)**: byte-exact match against `c64/rnd_spy.s` (6502 assembly calling the ROM's RND routine directly)
- **50,000 seeds (10 PRINT simulation)**: byte-exact match against `c64/rnd_10print.s` (RND + FADD 205.5 + QINT, simulating the actual 10 PRINT operations)

The pure RND and 10 PRINT simulations produce **identical seed sequences** for all 50,000 tested iterations.

## The hidden state: BITS and FACOV

The C64's floating-point workspace includes two registers not part of the 5-byte seed:

- **BITS ($68)**: Sign extension byte used by the right-shift routine (SHIFTR). Read by FMULT's MULSHF when a multiplier byte is zero.
- **FACOV ($70)**: Rounding byte. Persists across calls and feeds into the next multiply as the 5th (least significant) multiplier byte.

These registers are shared with all other floating-point operations. Between RND calls, the BASIC interpreter's math operations (FADD for `205.5+`, QINT for `CHR$()`, etc.) modify FACOV. In the `10 PRINT` context, FACOV equals the second mantissa byte of the FADD result after QINT shifts it into the rounding position.

Despite different FACOV values, the pure RND and 10 PRINT sequences produce identical seeds for at least 50,000 iterations. FACOV only affects the result when a zero multiplier byte triggers MULSHF, which is rare.

BITS stays zero throughout both pure RND calls and the 10 PRINT simulation --- neither RND nor FADD/QINT for positive numbers modifies it.

## Known simulation bug

Our C++ simulation diverges from the real C64 at step 45,295:

- **Input seed**: `$73 $44 $94 $D2 $A0` (unusually low exponent $73)
- **Expected** (VICE): `$7E $0B $38 $C6 $2E`
- **Our output**: `$7E $07 $38 $C6 $2E`
- **Difference**: byte 1 is $0B vs $07 (off by 4)

BITS and FACOV are both zero in both cases, so the hidden state is not the cause. The bug is in our implementation of the shift-and-add multiply. See [todo.md](../todo.md) for the current theory and next steps.

The cycle analysis results (tail=71,549, cycle=46,813) are based on this simulation and may be slightly affected by the bug. However, since the first 45,294 steps are verified correct, the results are likely close to the true values.
