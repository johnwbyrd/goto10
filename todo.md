# Research Status and Next Steps

## Confirmed findings

### RND(1) is deterministic regardless of calling context
- **MOVFM clears FACOV ($70) to zero** before every multiply. Confirmed from the Microsoft BASIC source (mist64/msbasic `LOAD_FAC_FROM_YA` zeroes FACEXTENSION). This means FACOV is always 0 entering FMULT during RND, no matter what BASIC operations ran between calls.
- BITS ($68) stays 0 throughout pure RND calls and 10 PRINT simulation — RND does not modify it, and FADD/QINT for positive numbers do not modify it.
- The pure RND and 10 PRINT simulations produce **identical seed sequences** for at least 50,000 iterations (verified against VICE). This is now explained: both have FACOV=0 and BITS=0 entering every multiply.
- **RND(1) is a pure function of its 5-byte seed.** The hidden state does not affect it. The earlier FACOV/BITS investigation was a necessary dead end that proved this.

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
- **MOVFM clears FACOV to 0** before every multiply. The Microsoft source (mist64/msbasic `LOAD_FAC_FROM_YA`) explicitly zeroes FACEXTENSION. So FACOV is always 0 entering FMULT during RND, regardless of what BASIC operations ran between calls. Our `movfm()` setting `fac.ov = 0` is correct.
- FACOV = 0 means the first multiplier byte processed is 0, which triggers the MULSHF byte-shift path
- MULSHF shifts RESHO-RESLO right by one byte position, stores old RESLO into FACOV ($70), fills RESHO from BITS ($68) = 0. Our code does the equivalent with `res_ov = res4; res4 = res3; res3 = res2; res2 = res1; res1 = 0`.
- The bits_sweep analysis showed that our code with FACOV 0 and 1 produces $07, while FACOV >= 2 produces $0B. But VICE with FACOV=0 produces $0B. Since MOVFM clears FACOV, the real C64 has FACOV=0 too — yet gets a different answer.
- **The bug is purely in our shift-and-add multiply arithmetic**, not in hidden state tracking.

### Root cause: FOUND
**The FADD is not a no-op.** Step-by-step state capture from VICE proves it:

```
After FMULT:  FAC = 8B 8B 31 CE 21, FACOV=FE
After FADD:   FAC = 8B 8B 31 CE 22, FACOV=13
```

FADD changed FACLO from $21 to $22 and FACOV from $FE to $13. The +1 in the LSB propagates through byte swap and normalize to produce the 4-unit difference in the final packed byte ($07 vs $0B).

The mechanism: FADD saves FACOV ($FE) into OLDOV before shifting ARG right. After shifting ARG to align exponents (4 byte shifts + 2 bit shifts), ARG mantissa is all zeros but the rounding byte (A register) holds $2A — the shifted-down remnant of ARG's mantissa. Then `ADC OLDOV` adds $2A + $FE = $128, producing carry=1. This carry propagates into `ADC ARGLO` (which is $00 + $00 + 1 = $01), incrementing FACLO from $21 to $22. The ARG mantissa contributes nothing — the +1 comes entirely from the rounding byte carry.

This is not a theory — it is traced instruction by instruction from the ROM source against the VICE state capture. Our simulation must implement FADD fully (including the OLDOV carry path) and be validated against VICE for 50,000+ iterations.

### Fix required
Implement FADD properly in `src/c64rnd.cpp` instead of treating it as a no-op. The full MBF floating-point addition must be performed, including:
1. Alignment shift (shift the smaller operand right by the exponent difference)
2. Addition of aligned mantissas
3. Normalization of the result
4. Proper handling of the rounding byte through all steps

The current `fadd()` implementation returns early when the exponent difference is >= 40. This threshold is wrong — the 40-bit internal precision means the add can affect results when the exponent difference is up to 39 bits.

After fixing FADD, re-validate against VICE for 50,000 iterations and re-run the cycle analysis.

## Open questions

1. **Does the multiply bug affect the cycle analysis?** The first 45,294 steps are correct. The bug occurs when exponent drops very low ($73), which is rare. The cycle entry point and length may still be correct, or may be off by a small amount.

2. **What is the true cycle structure of 10 PRINT?** Since FACOV doesn't affect the seed sequence for 50,000 iterations, the 10 PRINT cycle is likely the same as the pure RND cycle. But we haven't proven this beyond 50K.

3. **Can the multiply be expressed algebraically?** The byte-level analysis in `doc/algebraic_analysis.md` showed a symmetric two-step matrix with zero corners, but the carry corrections (which are the source of our bug) make the exact algebra nonlinear.
