# goto10

A bit-exact reimplementation of the Commodore 64 BASIC program:

```
10 PRINT CHR$(205.5+RND(1)); : GOTO 10
```

## The Algorithm

The program prints an infinite stream of two diagonal line characters,
chosen pseudo-randomly: PETSCII 205 (`\`) or 206 (`/`). The choice
depends entirely on whether `RND(1)` returns a value less than 0.5 or
not. Everything interesting about the program therefore reduces to the
question: **what does `RND(1)` actually compute?**

### State representation

The generator's state is a single number stored in Microsoft Binary
Format (MBF), a 5-byte floating point encoding used throughout
Commodore BASIC. An MBF float consists of:

| Byte | Name     | Meaning |
|------|----------|---------|
| 0    | Exponent | Biased by 128. An exponent byte of $80 means 2^0 = 1. A byte of $00 means the entire number is zero. |
| 1    | M1       | Mantissa byte 1 (most significant). Bit 7 stores the sign when packed in memory (0 = positive). When unpacked for computation, bit 7 is replaced with the implicit leading 1. |
| 2    | M2       | Mantissa byte 2 |
| 3    | M3       | Mantissa byte 3 |
| 4    | M4       | Mantissa byte 4 (least significant) |

The mantissa has an implicit leading 1 bit, exactly like IEEE 754. The
value of a positive, nonzero MBF float is:

```
value = 2^(exponent - 128) * (1.M1M2M3M4 in binary)
```

where the mantissa fraction `1.M1M2M3M4` lies in [1.0, 2.0), making
the full value lie in [2^(e-128), 2^(e-127)).

The initial seed loaded from ROM at power-on is:

```
$80  $4F  $C7  $52  $58
```

This encodes the value 0.811635157... (exponent $80 = 2^0, mantissa
$CF $C7 $52 $58 with the implicit bit restored).

### The RND(1) algorithm, step by step

Each call to `RND(1)` transforms the seed into a new seed and returns
the new value. The transformation has five stages:

#### Stage 1: Multiply

The seed is multiplied by the constant **11879546**, stored in MBF as
`$98 $35 $44 $7A $00`.

The multiplication is performed by the C64's floating-point multiply
routine, which works as a binary shift-and-add algorithm operating on
the 32-bit mantissas. This is equivalent to:

1. Compute the full 64-bit integer product of the two 32-bit mantissas
   (each with its implicit leading 1 restored).
2. Take the upper 32 bits as the result mantissa.
3. The next 8 bits become the "rounding byte" (extra precision carried
   forward).
4. Sum the exponents and subtract the bias: `new_exp = exp_seed +
   exp_const - 128`.

There is a subtlety: the C64's multiply routine also processes a
5th "rounding byte" from the input, making it effectively a 40-bit
times 32-bit multiply that produces a 40-bit result (32 mantissa bits
plus 8 rounding bits). On the first call after seeding, this rounding
byte is zero. On subsequent calls, it contains the previous
iteration's recycled exponent (see Stage 4). This means the generator
carries slightly more state than just the 4 mantissa bytes.

After multiplication, the result is normalized: if the leading bit of
the mantissa is not set, the mantissa is shifted left and the exponent
decremented until it is.

#### Stage 2: Add (no-op)

The constant **3.927677739 x 10^-8** (MBF bytes `$68 $28 $B1 $46
$00`) is added to the product. In a proper linear congruential
generator, this would serve as the additive constant in the recurrence
`x_{n+1} = a * x_n + c`.

In practice, this addition does nothing. The product from Stage 1 has
an exponent around $98 (2^24), while the additive constant has
exponent $68 (2^-24). The difference of 48 in exponent means the
constant would need to be right-shifted by 48 bits to align with the
product for addition. Since the mantissa is only 32 bits wide, the
constant is shifted entirely off the end and contributes zero. The
original Microsoft BASIC source code comments acknowledge this:

> *"ALSO, CONSTANTS ARE TRUNCATED"*
>
> *"<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"*

This is not a quirk of this particular seed or iteration---it is
structurally guaranteed to do nothing for any input the generator can
produce, because the multiply always yields a product far larger than
the additive constant.

#### Stage 3: Byte swap

The four mantissa bytes are permuted:

```
Before:  M1  M2  M3  M4
After:   M4  M2' M3' M1

where M2' = M3 (old byte 3 moves to position 2)
      M3' = M2 (old byte 2 moves to position 3)
```

In other words: bytes 1 and 4 are swapped, and bytes 2 and 3 are
swapped. This is equivalent to reversing the byte order of the 32-bit
mantissa.

This is the scrambling step. Without it, the generator would be a
straightforward multiplicative congruential generator on the mantissa,
and consecutive outputs would be highly correlated. The byte reversal
breaks up the linear structure by moving the least-significant
multiplication results into the most-significant position and vice
versa. The original source comments this as an attempt to *"SHUFFLE HI
AND LO BYTES"* and *"TO SUPPOSEDLY MAKE IT MORE RANDOM"*.

#### Stage 4: Force into [0, 1)

The result is forced into the half-open interval [0.5, 1.0) by three
operations:

1. **Force positive**: The sign byte is cleared.

2. **Recycle the exponent**: The current exponent (which is whatever
   resulted from the multiply and normalize) is moved into the
   rounding byte. This is the extra state mentioned in Stage 1---it
   feeds back into the next iteration's multiply as the low 8 bits of
   the multiplier, providing a small amount of additional mixing.

3. **Set exponent to $80**: Exponent $80 means 2^0. A normalized MBF
   float with this exponent has its mantissa in [1.0, 2.0), so the
   value lies in [0.5, 1.0) when interpreted as a fraction (the
   implicit leading 1 sits at the 0.5 position). If the byte swap
   happened to clear the leading mantissa bit, the subsequent
   normalization will shift it back in---but this also decrements the
   exponent below $80, producing a value smaller than 0.5. The result
   always lies in (0, 1.0).

4. **Normalize**: Shift the mantissa left until the leading bit is set,
   pulling in bits from the rounding byte. Decrement the exponent for
   each shift. The rounding byte provides up to 8 bits of extra
   precision during this step.

The resulting value is both returned to the caller and stored back as
the new seed.

#### Stage 5: Pack and store

The value is packed back into the 5-byte MBF format in memory. Before
packing, the rounding byte is examined: if its high bit is set, the
mantissa is incremented by 1 (with carry propagation). This is
standard MBF rounding from 40-bit internal precision to 32-bit stored
precision. In rare cases where the increment overflows all 32 mantissa
bits, the mantissa is reset to $80000000 and the exponent is
incremented.

The packed sign bit (bit 7 of byte 1) is set from the sign register,
which is always zero for RND output, so bit 7 of byte 1 is always
clear.

### The character decision

The one-liner evaluates `CHR$(205.5 + RND(1))`.

On the C64, `CHR$()` truncates its argument to an integer (toward
zero, which for positive numbers is equivalent to floor). So:

- If `RND(1) < 0.5`: the sum is in [205.5, 206.0), truncated to **205**.
- If `RND(1) >= 0.5`: the sum is in [206.0, 206.5), truncated to **206**.

In the PETSCII character set (uppercase/graphics mode):

- **CHR$(205)** ($CD) is the diagonal line from upper-left to
  lower-right: `\`
- **CHR$(206)** ($CE) is the diagonal line from upper-right to
  lower-left: `/`

The semicolon after `CHR$(...)` suppresses the newline, so characters
are printed sequentially, filling the 40-column screen. `GOTO 10`
loops forever.

### Summary as pseudocode

```
state = {0x80, 0x4F, 0xC7, 0x52, 0x58}   // 5-byte MBF seed

forever:
    fac = mbf_unpack(state)                 // restore implicit leading 1
    fac = mbf_multiply(fac, 11879546.0)     // multiply (40x32 -> 40-bit)
    // mbf_add(fac, 3.927e-8) is a no-op
    fac.mantissa = byte_reverse(fac.mantissa)  // swap M1<->M4, M2<->M3
    fac.sign = positive
    fac.rounding_byte = fac.exponent        // recycle exponent as extra state
    fac.exponent = 0x80                     // force into [0, 1) range
    mbf_normalize(fac)                      // shift left until MSB set
    state = mbf_pack_with_rounding(fac)     // round and store new seed
    value = mbf_to_float(fac)
    if value < 0.5: print '\'
    else:           print '/'
```

### Quality of the generator

The original Microsoft BASIC source is blunt:

> *"VERY POOR RND ALGORITHM"*

The generator is a multiplicative congruential generator with a broken
additive constant and a byte-reversal scramble. Its weaknesses include:

- **The additive constant does nothing**: The `c` in `x = a*x + c` is
  too small to survive the floating-point addition, reducing the
  generator to a purely multiplicative recurrence with a byte swap.
  This means the generator can never escape a cycle once it enters
  one, and zero is an absorbing state.

- **The multiplier is poor**: 11879546 was not chosen for good
  spectral properties. The original source notes the constants were
  "TRUNCATED", suggesting they were adapted from a different precision
  format without re-optimizing.

- **MBF arithmetic quirks**: The 40-bit shift-and-add multiply, the
  rounding during pack, and the exponent recycling all introduce
  nonlinear perturbations that make formal analysis difficult but do
  not improve statistical quality.

For the purpose of drawing a maze pattern on screen, none of this
matters. The generator produces a visually pleasing ~50/50 split of
the two characters with enough local variation to look random to the
human eye. That is all it ever needed to do.

## Cycle Analysis

A deterministic map on a finite state space must eventually cycle. The
question is: how soon?

### Theoretical bounds

The generator state is the 5-byte packed seed. Not all 2^40 possible
byte patterns are reachable:

- Byte 0 (exponent) is at most $80, since it is forced to $80 before
  normalization, and normalization can only decrease it.
- Byte 1, bit 7 is always 0 (sign forced positive).

This gives an upper bound of roughly 129 x 128 x 256^3 ~= 2^38
reachable states. A well-designed generator with this state size could
theoretically achieve a period approaching 2^38 (~274 billion).

### Actual measured period

Using Brent's cycle detection algorithm on the default power-on seed:

| Quantity | Value |
|----------|-------|
| **Tail length** (mu) | 71,549 |
| **Cycle length** (lambda) | 46,813 |
| **Total before first repetition** | 118,362 |

The generator visits **118,362** distinct states before repeating.
That is 0.000004% of the theoretical state space.

The output bit sequence (`/` vs `\`) has the same period as the
full internal state---there is no shorter sub-cycle in the maze
pattern.

### What this means for 10 PRINT

On a 40-column, 25-row C64 screen:

- Each screenful is 1,000 characters.
- The first **71.5 screenfuls** are unique.
- Then a **46.8-screenful** cycle begins.
- After roughly **118 screenfuls**, the maze repeats exactly.

At the C64's native output speed, it takes a few minutes to fill 118
screens. No one watching the program would notice the repetition---the
pattern is long enough to be visually indistinguishable from an
infinite non-repeating sequence. But it does repeat, and it repeats
surprisingly soon.

### Distribution within the cycle

Within one full cycle of 46,813 characters:

| Character | Count | Percentage |
|-----------|-------|------------|
| `\` (CHR$ 205) | 23,245 | 49.655% |
| `/` (CHR$ 206) | 23,568 | 50.345% |

The split is nearly but not perfectly even, with a slight bias toward
`/`. The deviation from 50/50 is small enough to be invisible in
practice.

### Verification

The cycle has been independently verified in two ways:

1. **Brent's algorithm** (in `cycle_detect.cpp`) identifies the exact
   tail and cycle lengths by advancing through states in O(1) memory.

2. **Direct comparison** (in `test_rnd.cpp`) generates 118,462
   characters, confirms that the seed at position 71,549 is identical
   to the seed at position 118,362 (= 71,549 + 46,813), and verifies
   that the output characters match across the cycle boundary.

## Building

```
cmake -B build
cmake --build build --config Release
./build/Release/test_rnd
```

## References

- [C64 BASIC ROM Disassembly](https://www.pagetable.com/c64ref/c64disasm/) by Michael Steil
- [C64 Memory Map](https://www.pagetable.com/c64ref/c64mem/) by Michael Steil
- [*10 PRINT CHR$(205.5+RND(1)); : GOTO 10*](https://10print.org/) (MIT Press, 2012)
