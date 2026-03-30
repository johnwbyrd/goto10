# Cycle Analysis

## Method

The cycle structure was found using [Brent's cycle detection algorithm](https://en.wikipedia.org/wiki/Cycle_detection#Brent's_algorithm), which finds the exact tail length (mu) and cycle length (lambda) of any eventually-periodic sequence using O(1) memory.

The implementation (`src/cycle_detect.cpp`) delegates each step to `C64Rnd::next()`, which replicates the C64's FMULT (including the carry leak between zero multiplier bytes), FADD (including the rounding byte carry), byte swap, normalize, and round/pack.

## Validation

The simulation was validated against VICE 3.9 running the actual C64 BASIC ROM:

- **1,000,000 consecutive seeds** from pure RND(1) calls via 6502 assembly (`c64/rnd_spy.s`): byte-exact match, zero differences.
- **100,000 seeds from the actual BASIC `10 PRINT` program** (`c64/10print.bas`), captured via breakpoint at the RND entry point (`src/vice_sample_rnd.py`): byte-exact match, zero differences.

The pure RND sequence and the `10 PRINT` sequence produce identical seeds. This is because MOVFM clears FACOV to zero before every multiply (confirmed from the ROM source at code20 line 30), so the FADD and QINT operations between RND calls do not affect the next call's state.

## Cycle catalog

`best_seed.cpp` scans seeds from `RND(-1)` through `RND(-432000)` (two hours of jiffy timer values). All seeds land on one of 12 distinct cycles:

| Cycle length | First seed found |
|-------------|-----------------|
| 58,078 | RND(-1) |
| 7,036 | RND(-25) |
| 5,660 | RND(-39) |
| 4,232 | RND(-503) |
| 2,644 | RND(-181) |
| 724 | RND(-2) |
| 371 | RND(-94836) |
| 295 | RND(-381) |
| 207 | RND(-7737) |
| 85 | RND(-152007) |
| 23 | RND(-164855) |
| 7 | RND(-77164) |

The dominant cycle (58,078) captures the vast majority of seeds. The smaller cycles are rare attractors reached by specific mantissa patterns.

## Properties of the dominant cycle

Run `cycle_detect` to get current values for:
- Tail length (mu): the number of unique values before entering the cycle
- Cycle length (lambda): the repeating period
- Output distribution: `\` vs `/` split within one cycle period

## Two bugs in Microsoft's FMULT

The simulation's accuracy depends on replicating two undocumented behaviors in the C64's FMULT routine. See [doc/microsoft_bugs.md](microsoft_bugs.md) for details.

1. **Carry leak**: When a multiplier byte is zero, the MULSHF routine falls through into SHIFTR, which uses the carry flag left by the previous byte's last LSR instruction. This causes a 9-bit shift instead of the expected 8-bit shift when the carry is 0.

2. **FADD is not a no-op**: The additive constant (3.927677739E-8) is small but not zero relative to the 40-bit internal precision. When the post-multiply exponent is low enough, the rounding byte carry from FADD's alignment shift propagates into the mantissa LSB.

Both behaviors were discovered by comparing the simulation against VICE and tracing the divergence to specific instructions in the ROM source.
