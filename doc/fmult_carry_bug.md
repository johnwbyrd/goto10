# The FMULT Carry Leak

## Summary

The C64's floating-point multiply routine (FMULT, at $BA28 in BASIC ROM) has a carry flag leak that causes zero mantissa bytes to be processed incorrectly. When a multiplier byte is $00, the result is shifted right by either 8 or 9 bits instead of exactly 8, depending on the least significant bit of the previous multiplier byte.

## How FMULT works

FMULT multiplies FAC by a constant from memory. It processes the FAC mantissa one byte at a time, from least significant (FACOV at $70) to most significant (FACHO at $62). For each byte, there are two code paths:

**Nonzero byte** (MLTPLY → MLTPL1): An 8-round shift-and-add loop. Each round conditionally adds ARG to the result, then shifts the entire result right by 1 bit (ROR chain). The loop uses a sentinel bit: the byte is right-shifted via `LSR A`, with bit 7 set as a marker. When the sentinel shifts out, A=0 and the loop exits via `BNE`. The carry flag holds the last data bit shifted out.

**Zero byte** (MLTPLY → MULSHF): `JMP MULSHF` at $BA5B. MULSHF ($B983) sets X=#RESHO-1 and falls into SHFTR2 ($B985), which shifts the result register (RESHO-RESLO and FACOV) down by one byte position — a logical right shift by 8 bits. SHFTR2 then falls through into SHIFTR ($B999).

## The bug

SHIFTR starts with `ADC #8`. The intent is to add 8 to the shift count in A. But A contains the zero multiplier byte (0), and the carry flag contains whatever the previous byte's last `LSR A` left.

- **Carry = 1** (previous byte's bit 0 was 1): `ADC #8` → A = 9. SHIFTR computes `SBC #8` → A = 1, carry set. `BCS SHFTRT` taken — exits immediately. **Total shift: 8 bits. Correct.**

- **Carry = 0** (previous byte's bit 0 was 0): `ADC #8` → A = 8. SHIFTR computes `SBC #8` → result depends on carry from ADC. The ADC of 0+8+0=8 clears carry. SBC: 8 - 8 - (1-0) = -1 = $FF, carry clear. `TAY` → Y = $FF. `BCS SHFTRT` — not taken. Falls into SHFTR3 bit-shift loop. Y=$FF, loop does `INY; BNE` → one iteration (Y goes $FF → $00). **Total shift: 9 bits. One extra bit of precision lost.**

## When it triggers

The bug triggers when ALL of:
1. A multiplier byte (FACOV, FACLO, FACMO, or FACMOH) is exactly $00
2. The previous multiplier byte's least significant bit was 0

FACHO (the MSB) is never zero because it has the implicit leading 1 bit. FACOV is always 0 after MOVFM (confirmed from ROM source). So the first byte always triggers MULSHF, and its carry depends on the state left by MULDIV, not a previous byte.

For the remaining bytes (FACLO, FACMO, FACMOH): a zero byte requires that the packed seed has $00 in that position — uncommon but not rare. The previous byte's LSB being 0 is a ~50% chance.

## Effect on RND

In the RND sequence from the default seed, this bug first affects the result at step 59,781. The seed at that point is `$80 $34 $00 $00 $D1`, which has two zero mantissa bytes (M2 and M3). After MOVFM unpacks: FACOV=$00, FACLO=$D1, FACMO=$00, FACMOH=$00, FACHO=$B4.

The processing of FACLO=$D1 ends with carry=1 (bit 0 of $D1 is 1). So the first zero byte (FACMO=$00) gets an 8-bit shift — correct. But after that 8-bit shift, the state within SHIFTR leaves carry=0 (from the BCS SHFTRT path). The second zero byte (FACMOH=$00) enters with this carry=0 and gets a 9-bit shift — one extra bit lost.

The 1-bit precision loss in RESLO propagates through the post-multiply normalize, byte swap, and second normalize to produce a different final seed than a mathematically correct multiply would.

## Effect on all C64 math

This bug is not specific to RND. It affects every floating-point multiply on the C64 where a mantissa byte is $00 and the previous byte ended with its LSB clear. This includes all BASIC arithmetic (`*` operator), scientific functions (which use FMULT internally), and any ROM routine that calls FMULT.

The effect is small — 1 bit of the least significant result byte — and only manifests for specific mantissa values. It has been present in every C64 ever manufactured and in every program ever run on one.

## Discovery

Found by comparing a C++ simulation of FMULT against the VICE emulator (running the actual C64 ROM) over 100,000 RND iterations. The simulation matched for 59,780 iterations, then diverged. Byte-by-byte state capture from VICE (using a 6502 assembly spy program calling MLTPLY individually for each byte) revealed that the third multiplier byte produced different intermediate results. Tracing the carry flag through the ROM source code identified the leak.
