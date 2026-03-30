# Bugs in Microsoft's BASIC Floating Point

Two bugs were discovered in the C64 BASIC ROM's floating-point routines by comparing a C++ simulation against the VICE emulator over 1,000,000 RND iterations. Both bugs exist in every C64 ever manufactured and affect all BASIC floating-point arithmetic, not just RND.

## Bug 1: FMULT carry leak

### Summary

The floating-point multiply routine (FMULT, at $BA28) has a carry flag leak between multiplier bytes. When a multiplier byte is $00, the result is shifted right by either 8 or 9 bits instead of exactly 8, depending on the least significant bit of the previous multiplier byte.

### Mechanism

FMULT processes the FAC mantissa one byte at a time. For nonzero bytes, an 8-round shift-and-add loop runs, ending with `LSR A` which puts the last data bit into the 6502 carry flag. For zero bytes, `JMP MULSHF` does a byte-level shift, then falls through into SHIFTR which executes `ADC #8`.

The `ADC #8` uses the carry flag from the previous byte's last `LSR A`. Nobody clears it between bytes. The result:

- **Carry = 1** (previous byte's bit 0 was 1): `ADC #8` gives A = 9. SHIFTR exits immediately via `BCS SHFTRT`. Total shift: 8 bits. Correct.
- **Carry = 0** (previous byte's bit 0 was 0): `ADC #8` gives A = 8. SHIFTR's `SBC #8` produces $FF with carry clear. The bit-shift loop runs once (Y goes $FF → $00). Total shift: 9 bits. One extra bit of precision lost.

### When it triggers

All three conditions must hold:
1. A multiplier byte (FACOV, FACLO, FACMO, or FACMOH) is exactly $00
2. The previous byte's least significant bit was 0
3. FACHO is never zero (implicit leading 1 bit), so only bytes 0-3 are affected

FACOV is always 0 after MOVFM (code20 line 30: `STY FACOV` where Y=0). The MULDIV carry entering the first MULSHF is always 0 for exponents in the RND range. So the first byte always gets a 9-bit shift.

### Effect on RND

First affects the RND sequence at step 59,781 (seed `$80 $34 $00 $00 $D1` — two zero mantissa bytes). The 1-bit precision loss in RESLO propagates through normalize and byte swap to produce a different final seed.

### Effect on all C64 math

Affects every floating-point multiply where a mantissa byte is $00 and the previous byte's LSB is 0. This includes the `*` operator, scientific functions, and any ROM routine calling FMULT. The effect is 1 bit of the least significant result byte.

### ROM source reference

The bug is in the interaction between MLTPLY (code19 line 50), MULSHF (code18 line 85), and SHIFTR (code18 line 96). MULSHF sets X and falls into SHFTR2, which falls through to SHIFTR. SHIFTR's `ADC #8` inherits the carry flag from whatever instruction last set it — which is the previous byte's `LSR A` at code19 line 75.

## Bug 2: FADD is not a no-op

### Summary

The RND function adds a small constant (3.927677739 × 10⁻⁸) to the multiply result via FADD. Every existing disassembly commentary describes this addition as doing nothing because the constant is too small relative to the product. This is wrong. The addition occasionally modifies the result through rounding byte carry propagation.

### Mechanism

FADD saves the current FACOV (rounding byte from the multiply) into a temporary variable OLDOV before shifting ARG right to align exponents. After the alignment shift, ARG's mantissa is entirely zero — the constant really is too small to contribute directly. But the alignment shift leaves a nonzero rounding byte in the A register (the shifted-down remnant of ARG's mantissa).

FADD2 then executes `ADC OLDOV`, adding this remnant to the saved FACOV from the multiply. When the sum exceeds $FF, the carry propagates into `ADC ARGLO`. Since ARGLO is $00 (shifted to zero), the carry adds 1 to FACLO. This +1 in the mantissa LSB propagates through the byte swap and normalize to change the final seed.

### When it triggers

The alignment shift count is (FACEXP - ARGEXP). The FADD constant has exponent $68. For the common case (FACEXP = $98, from seed exponent $80), the shift is $98 - $68 = $30 = 48 bits — larger than the 40-bit internal precision. The constant is completely shifted out and FADD truly does nothing.

But when the seed exponent drops below $80, the post-multiply exponent drops below $98, and the shift count can be less than 40. Enough bits of the constant survive in the rounding byte to carry into the mantissa.

### Effect on RND

First affects the RND sequence at step 45,295 (seed `$73 $44 $94 $D2 $A0`, exponent $73). FACLO changes from $21 to $22. The +1 propagates to a 4-unit difference in the packed output byte.

### ROM source reference

FADD entry at code17 line 99. OLDOV save at code17 lines 102-103. Alignment via SHIFTR at code17 line 97 / code18 line 96. Addition at FADD2, code18 lines 18-32.

### The disassembly commentary was wrong

The pagetable.com community commentary says:

> "<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"

This is incorrect. The addition does nothing for most inputs but modifies the result when the exponent is low enough for the rounding byte to carry.

## Discovery

Both bugs were found by the same method: comparing a C++ simulation against the VICE emulator and binary-searching for the first divergence. At each divergence point, a 6502 assembly spy program captured the complete zero-page state (FAC, ARG, RES, BITS, FACOV) before and after each sub-step of the RND function, allowing instruction-level comparison against the ROM source.
