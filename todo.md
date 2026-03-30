# Research Status and Next Steps

## Confirmed findings

### RND(1) is deterministic for a given calling context
- BITS ($68) stays 0 throughout pure RND calls and 10 PRINT simulation — RND does not modify it, and FADD/QINT for positive numbers do not modify it
- FACOV ($70) is set by the FADD+QINT operations between calls in the 10 PRINT context (equals M2 of the FADD result mantissa after QINT shifts it into FACOV)
- Despite different FACOV values, the pure RND and 10 PRINT simulations produce **identical seed sequences** for at least 50,000 iterations (verified against VICE)

### Cycle analysis (from default seed)
- Tail length: 71,549
- Cycle length: 46,813
- Total distinct values: 118,362
- Verified against VICE for 10,000 consecutive seeds (byte-exact match)
- **Caveat**: our simulation has a multiply bug (see below) that may affect results after step 45,295

### The algorithm was never fixed
- Same multiply constant ($B5447A00), same dead additive constant, same byte swap shipped in every 8-bit Microsoft BASIC from 1975-1985
- Commodore fixed the 4-byte/5-byte constant width bug starting with the VIC-20; Applesoft never fixed it
- Microsoft replaced the algorithm entirely for GW-BASIC (proper 24-bit integer LCG)

## Known bug in our simulation

### Symptom
Our C++ shift-and-add multiply (in `src/c64rnd.cpp` `fmult()` and the `step()` functions in `src/cycle_detect.cpp` and `src/best_seed.cpp`) diverges from the real C64 at step 45,295.

### Input that triggers it
Seed: `$73 $44 $94 $D2 $A0` (step 45,294 output, exponent $73 = unusually low)

### Expected output (from VICE)
`$7E $0B $38 $C6 $2E`

### Our output
`$7E $07 $38 $C6 $2E`

### Difference
Byte 1: $0B vs $07 (difference of 4). Exponent and bytes 2-4 are correct.

### What we know
- BITS ($68) = 0 and FACOV ($70) = 0 in both the real C64 and our simulation — the hidden state is NOT the cause
- FACOV = 0 means the first multiplier byte processed is 0, which triggers the MULSHF byte-shift path
- MULSHF fills the MSB of the result from BITS ($68) = 0, which our code also does (`res1 = 0` in the zero-byte case)
- The bits_sweep analysis showed that FACOV values 0 and 1 produce $07, while values >= 2 produce $0B — but VICE produces $0B even with FACOV=0
- This means our MULSHF path (or the subsequent shift-and-add rounds) has a carry propagation error

### Theory
The MULSHF path in the C64 ROM at $B983 does more than just shift bytes and fill from BITS. It also modifies FACOV ($70) — specifically, `STY $70` at $B987 stores the old LSB byte into FACOV before the shift. Our code doesn't do this — we only set `res_ov = res4` (moving the old byte 4 into the rounding position) but we may not be updating the FACOV register that feeds into subsequent processing.

Alternatively: the byte-level shift in SHFTR2 uses indexed addressing (`LDY 4,X / STY $70 / LDY 3,X / STY 4,X / ...`) which shifts the RESULT register bytes, not the FAC bytes. When called from MULSHF (X=$25, pointing at RESHO-1), the indexing is different from when called from QINT (X=$61, pointing at FAC). Our code may be conflating these two contexts.

### Next step
Trace the multiply for input `$73 $44 $94 $D2 $A0` instruction by instruction through the 6502 ROM code (FMULT at $BA28), comparing against our C++ implementation at each step. The first divergence will reveal the exact carry error.

## Open questions

1. **Does the multiply bug affect the cycle analysis?** The first 45,294 steps are correct. The bug occurs when exponent drops very low ($73), which is rare. The cycle entry point and length may still be correct, or may be off by a small amount.

2. **What is the true cycle structure of 10 PRINT?** Since FACOV doesn't affect the seed sequence for 50,000 iterations, the 10 PRINT cycle is likely the same as the pure RND cycle. But we haven't proven this beyond 50K.

3. **Can the multiply be expressed algebraically?** The byte-level analysis in `doc/algebraic_analysis.md` showed a symmetric two-step matrix with zero corners, but the carry corrections (which are the source of our bug) make the exact algebra nonlinear.
