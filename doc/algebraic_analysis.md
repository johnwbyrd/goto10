# Algebraic Analysis of the C64 RND Map

## Status

This analysis is incomplete. The results below were derived before the FMULT carry leak and the FADD correction were discovered. The two-step matrix is wrong. It is preserved here as a record of the approach, not as correct results.

## The approach

The goal was to express the RND map as a matrix operation on the 4 mantissa bytes, enabling computation of f^n (the n-th iterate) via matrix exponentiation in O(log n) time.

## Why it doesn't work cleanly

The RND map is not a single linear function. It is piecewise-linear, with the behavior depending on:

1. **Which mantissa bytes are zero.** Zero bytes trigger the MULSHF path, which shifts by 8 or 9 bits depending on the carry flag from the previous byte (the carry leak). Different zero-byte configurations produce different effective multipliers.

2. **The exponent.** Low exponents cause the FADD to modify the mantissa LSB through rounding byte carry propagation. The exponent determines whether FADD contributes.

3. **The byte swap.** The byte reversal is a fixed permutation (linear), but it interacts with the variable-width shifts from the carry leak to produce different effective maps for different inputs.

## What we know from empirical analysis

From scanning 432,000 seeds (`best_seed.cpp`), the state space contains exactly 12 distinct cycles. Every integer-derived seed lands on one of them. Analysis of zero-byte patterns across all 12 cycles shows:

- All cycles have ~99% states with no zero mantissa bytes
- The smallest cycles (7, 23) have NO zero-byte states at all
- Smaller cycles have narrower exponent ranges (closer to $80)
- The carry leak and FADD are rare events on all cycles

The 12 cycles are NOT distinguished by which bytes are zero. The mechanism that partitions the state space into 12 basins of attraction remains unknown.

## The two-step matrix (OBSOLETE)

The matrix below was derived assuming FADD is a no-op and the multiply is a clean shift-and-add without carry leakage. Both assumptions are wrong. The matrix is included only for reference.

For the carry-free, FADD-free approximation, two consecutive RND iterations produce:

```
[a'']   [$0D  $7C  $42  $00] [a]
[b''] = [$7C  $0D  $7C  $42] [b]    mod 256
[c'']   [$42  $7C  $09  $14] [c]
[d'']   [$00  $42  $14  $F9] [d]
```

This matrix is symmetric with zero corners (a'' independent of d, d'' independent of a). Over GF(2) it reduces to the identity matrix, meaning all the interesting structure is in the carry bits — exactly the bits we got wrong.

## Open questions

1. Can the 12 cycles be predicted from algebraic properties of the effective multiplier in each zero-byte configuration?
2. Is there a GF(2^8) or Z/256 formulation that accounts for the carry leak?
3. Do non-integer seeds (reachable via RND(0) timer reads) access additional cycles beyond the 12 found from integer seeds?
