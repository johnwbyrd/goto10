# The RND(1) Algorithm

The program `10 PRINT CHR$(205.5+RND(1)); : GOTO 10` prints an infinite stream of two diagonal line characters, chosen pseudo-randomly: PETSCII 205 (`\`) or 206 (`/`). The choice depends entirely on whether `RND(1)` returns a value less than 0.5.

## State

The generator's visible state is a 5-byte number stored in Microsoft Binary Format (MBF) at zero-page addresses $8B-$8F:

| Byte | Name     | Meaning |
|------|----------|---------|
| 0    | Exponent | Biased by 128. $80 = 2^0. $00 = number is zero. |
| 1    | M1       | Mantissa MSB. Bit 7 is sign in memory (0 = positive); replaced with implicit 1 when unpacked. |
| 2    | M2       | Mantissa byte 2 |
| 3    | M3       | Mantissa byte 3 |
| 4    | M4       | Mantissa byte 4 (LSB) |

The value of a positive, nonzero MBF float is `2^(exponent - 128) * (1.M1M2M3M4 in binary)`, where the mantissa fraction lies in [1.0, 2.0).

The initial seed loaded from ROM at power-on is `$80 $4F $C7 $52 $58`, encoding 0.811635157...

### Hidden state

Two additional zero-page registers participate in the computation but are not part of the stored seed:

- **FACOV ($70)**: The rounding byte. It is the 5th (least significant) byte of the multiplier during FMULT. After the RND function completes, FACOV retains whatever value the normalize and round operations left in it. It persists across calls because nothing in the RND function zeroes it. However, any floating-point operation that runs between RND calls (FADD, QINT, etc.) will overwrite it.

- **BITS ($68)**: The sign extension byte used by SHIFTR, the general-purpose right-shift routine. During FMULT, when a multiplier byte is zero, the MULSHF routine shifts the result register right by 8 bits and fills the MSB from BITS. RND itself does not modify BITS. It is written by SHIFTR during FADD alignment and by QINT during float-to-integer conversion. For positive numbers (which is all RND produces), these operations set BITS to 0.

In the `10 PRINT` one-liner, the operations between RND calls are `FADD` (adding 205.5) and `QINT` (for CHR$). These set FACOV to a value derived from the addition result and set BITS to 0. We have verified against VICE that the pure RND sequence and the `10 PRINT` sequence produce identical seeds for at least 50,000 iterations, despite having different FACOV values. FACOV only affects the result when a zero multiplier byte triggers MULSHF, which is rare.

## The RND(1) transformation

Each call to `RND(1)` transforms the seed through these operations, implemented in BASIC ROM at $E097-$E0F6:

### 1. Unpack seed into FAC (MOVFM, $BBA2)

The 5-byte seed is loaded into the Floating Point Accumulator (FAC). The implicit leading 1 is restored in the mantissa MSB. The sign bit moves to FACSGN ($66). FACOV ($70) is **not** cleared — it retains its value from the previous call.

### 2. Multiply by 11,879,546 (FMULT, $BA28)

The constant $98 $35 $44 $7A $00 (MBF encoding of 11,879,546) is unpacked into ARG. The exponents are summed (minus bias). Then the mantissas are multiplied via a binary shift-and-add loop.

The multiply processes five bytes of the FAC mantissa from LSB to MSB: FACOV, M4, M3, M2, M1. For each byte:

- If the byte is zero, the MULSHF path ($B983) shifts the entire result register right by 8 bits. The byte shifted into the MSB comes from **BITS ($68)**, not from zero. The byte shifted out goes into FACOV ($70).

- If the byte is nonzero, it is processed bit by bit. The byte is shifted right, and for each 1-bit, the ARG mantissa is added to the result. After each bit (0 or 1), the entire result is shifted right one position (ROR chain through all 5 result bytes).

The result is a 40-bit value: 32-bit mantissa in RESHO-RESLO ($26-$29), plus 8 rounding bits in FACOV ($70). This is copied back to FAC and normalized.

The fourth byte of the multiplier constant is $00. This means that during every multiply, the fourth byte processed (M4 of ARG = $00) contributes nothing to the product. The effective multiplier is 24 bits ($B5447A), shifted left 8 by the zero byte.

### 3. Add 3.927677739 x 10^-8 (FADD, $B867) — no-op

The constant $68 $28 $B1 $46 $00 is added to the product. This does nothing. The product has exponent ~$98 (2^24). The constant has exponent $68 (2^-24). The 48-bit exponent difference means the constant is shifted entirely off the 32-bit mantissa before the add. A community-authored disassembly commentary notes:

> *"<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"*

Microsoft's original source describes the constants neutrally as "RANDOM" and gives no indication the developers knew the add was dead.

### 4. Byte swap ($E0D3-$E0E1)

The four mantissa bytes are reversed: M1 swaps with M4, M2 swaps with M3.

```
Before:  M1  M2  M3  M4
After:   M4  M3  M2  M1
```

This breaks the linear structure of the multiply by moving the least significant result bytes into the most significant positions and vice versa. The source comments: *"TO SUPPOSEDLY MAKE IT MORE RANDOM"*.

### 5. Force into [0, 1) ($E0E3-$E0F1)

Four operations:

1. **Clear sign** ($E0E3): FACSGN = 0. The result is always positive.

2. **Recycle exponent** ($E0E7): The current exponent is moved to FACOV. This becomes the 5th multiplier byte on the next call.

3. **Set exponent to $80** ($E0EB): Forces the value into the range [0.5, 1.0) when normalized.

4. **Normalize** ($E0EF, calls $B8D7): Shift the mantissa left until the MSB is set, pulling in bits from FACOV. Decrement the exponent for each shift. If the byte swap left the MSB clear (which depends on the value of M4 before the swap), normalization shifts left and the exponent drops below $80, producing a value less than 0.5.

### 6. Round and pack (MOVMF, $BBD4)

The ROUND routine ($BC1B) tests FACOV: if bit 7 is set, the mantissa is incremented by 1 (with carry). This is destructive — `ASL FACOV` at $BC1F shifts FACOV left, discarding the tested bit. The rounded mantissa is packed into the 5-byte seed at $8B-$8F, with the sign bit replacing the implicit 1 in byte 1.

## The character decision

The one-liner evaluates `CHR$(205.5 + RND(1))`.

`CHR$()` truncates its argument to an integer:

- `RND(1) < 0.5` → sum in [205.5, 206.0) → truncated to 205 → `\`
- `RND(1) >= 0.5` → sum in [206.0, 206.5) → truncated to 206 → `/`

PETSCII 205 ($CD) is the diagonal from upper-left to lower-right. PETSCII 206 ($CE) is the diagonal from upper-right to lower-left. The semicolon suppresses the newline. `GOTO 10` loops forever.

## Why the generator fails

- **The additive constant does nothing.** The `c` in `x = a*x + c` is too small to survive the floating-point addition. The generator is purely multiplicative with a byte swap. Zero is an absorbing state.

- **The multiplier is poor.** 11,879,546 was not chosen for good spectral properties. The source notes the constants were "TRUNCATED", suggesting adaptation from a different precision format without re-optimization.

- **The effective state is small.** Despite a 5-byte (40-bit) seed, the generator visits only 118,362 distinct states out of ~274 billion possible before cycling. The byte swap, while intended to improve randomness, constrains the reachable state space rather than expanding it.
