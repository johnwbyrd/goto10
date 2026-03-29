# goto10

In 2012, ten authors published a 324-page book through the MIT Press about a single line of Commodore 64 BASIC:

```
10 PRINT CHR$(205.5+RND(1)); : GOTO 10
```

The book, [*10 PRINT CHR$(205.5+RND(1)); : GOTO 10*](https://10print.org/), treats the program as a philosophical touchstone.  The book covers the Commodore 64 as a platform, the BASIC language as a phenomenon, the maze as a motif in art and culture, and the nature of randomness in computing:

"The eponymous program is treated as a distinct cultural artifact, but it also serves as a grain of sand from which entire worlds become visible; as a Rosetta Stone that yields important access to the phenomenon of creative computing and the way computer programs exist in culture... Members of the working group had demonstrated they could interpret a large program, a substantial body of code, but could they usefully interpret a very spare program such as this one?"

It's 324 pages of self-important disquisition, about the rich tableau of social and philosophical implications of that one line of Commodore 64 code.

But for some reason, none of the ten authors bothered to analyze *the actual algorithm* underlying the one-liner.  They treated the resultant maze as "random" and managed to fill up a large book on that questionable principle.

## The findings

The `RND(1)` function in Commodore 64 BASIC is a pseudo-random number generator. Like all PRNGs, it is a deterministic function that maps a finite state to a new state. It must therefore eventually repeat.

We reimplemented the algorithm in C++ with bit-exact fidelity to the C64's MBF floating-point arithmetic, then applied Brent's cycle detection algorithm. The computation took a tenth of a second. The results:

| Quantity | Value |
|----------|-------|
| **Tail length** | 71,549 |
| **Cycle length** | 46,813 |
| **Total distinct values** | 118,362 |

The sequence of random numbers -- and therefore the maze of `/` and `\` characters that the program draws -- forms a **rho-shaped loop**:

```
start ----71,549 unique values----> cycle entry
                                       |
                                  46,813 values
                                  repeat forever
                                       |
                                  cycle entry <---+
                                       |          |
                                       +----------+
```

The first 71,549 values are produced once and never revisited. Then the generator enters a permanent cycle of 46,813 values. The maze pattern repeats exactly after this point, forever.

The C64 screen is 40 columns by 25 rows, or 1,000 characters. The maze pattern is unique for the first ~72 screenfuls. Then a ~47-screen cycle begins. After roughly 118 screenfuls, you are watching a rerun.

### How bad is this?

The generator's 5-byte state could theoretically support ~2^38 (~274 billion) distinct values. It uses **118,362** of them. That is 0.000004% of the state space.

The original Microsoft BASIC source code, which shipped in the C64's ROM, contains the comment:

> *"VERY POOR RND ALGORITHM"*

The developers knew.

### The output distribution

Within one full cycle, the character split is nearly even:

| Character | Count | Percentage |
|-----------|-------|------------|
| `\` (CHR$ 205) | 23,245 | 49.655% |
| `/` (CHR$ 206) | 23,568 | 50.345% |

This is close enough to 50/50 that no human observer would notice a bias. The visual quality of the maze is fine. It is the *duration* of the maze that is absurdly short.

## The algorithm in detail

The program prints an infinite stream of two diagonal line characters, chosen pseudo-randomly: PETSCII 205 (`\`) or 206 (`/`). The choice depends entirely on whether `RND(1)` returns a value less than 0.5. Everything interesting about the program therefore reduces to the question: what does `RND(1)` actually compute?

### State representation

The generator's state is a single number stored in Microsoft Binary Format (MBF), a 5-byte floating point encoding used throughout Commodore BASIC:

| Byte | Name     | Meaning |
|------|----------|---------|
| 0    | Exponent | Biased by 128. An exponent byte of $80 means 2^0 = 1. A byte of $00 means the entire number is zero. |
| 1    | M1       | Mantissa byte 1 (most significant). Bit 7 stores the sign when packed in memory (0 = positive). When unpacked for computation, bit 7 is replaced with the implicit leading 1. |
| 2    | M2       | Mantissa byte 2 |
| 3    | M3       | Mantissa byte 3 |
| 4    | M4       | Mantissa byte 4 (least significant) |

The mantissa has an implicit leading 1 bit, exactly like IEEE 754. The value of a positive, nonzero MBF float is:

```
value = 2^(exponent - 128) * (1.M1M2M3M4 in binary)
```

where the mantissa fraction `1.M1M2M3M4` lies in [1.0, 2.0), making the full value lie in [2^(e-128), 2^(e-127)).

The initial seed loaded from ROM at power-on is:

```
$80  $4F  $C7  $52  $58
```

This encodes the value 0.811635157... (exponent $80 = 2^0, mantissa $CF $C7 $52 $58 with the implicit bit restored).

### The RND(1) algorithm, step by step

Each call to `RND(1)` transforms the seed into a new seed and returns the new value. The transformation has five stages:

#### Stage 1: Multiply

The seed is multiplied by the constant **11879546**, stored in MBF as `$98 $35 $44 $7A $00`.

The multiplication is performed by the C64's floating-point multiply routine, which works as a binary shift-and-add algorithm operating on the 32-bit mantissas. This is equivalent to:

1. Compute the full 64-bit integer product of the two 32-bit mantissas (each with its implicit leading 1 restored).
2. Take the upper 32 bits as the result mantissa.
3. The next 8 bits become the "rounding byte" (extra precision carried forward).
4. Sum the exponents and subtract the bias: `new_exp = exp_seed + exp_const - 128`.

There is a subtlety: the C64's multiply routine also processes a 5th "rounding byte" from the input, making it effectively a 40-bit times 32-bit multiply that produces a 40-bit result (32 mantissa bits plus 8 rounding bits). On the first call after seeding, this rounding byte is zero. On subsequent calls, it contains the previous iteration's recycled exponent (see Stage 4). This means the generator carries slightly more state than just the 4 mantissa bytes.

After multiplication, the result is normalized: if the leading bit of the mantissa is not set, the mantissa is shifted left and the exponent decremented until it is.

#### Stage 2: Add (no-op)

The constant **3.927677739 x 10^-8** (MBF bytes `$68 $28 $B1 $46 $00`) is added to the product. In a proper linear congruential generator, this would serve as the additive constant in the recurrence `x_{n+1} = a * x_n + c`.

In practice, this addition does nothing. The product from Stage 1 has an exponent around $98 (2^24), while the additive constant has exponent $68 (2^-24). The difference of 48 in exponent means the constant would need to be right-shifted by 48 bits to align with the product for addition. Since the mantissa is only 32 bits wide, the constant is shifted entirely off the end and contributes zero. The original Microsoft BASIC source code comments acknowledge this:

> *"ALSO, CONSTANTS ARE TRUNCATED"*
>
> *"<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"*

This is not a quirk of this particular seed or iteration---it is structurally guaranteed to do nothing for any input the generator can produce, because the multiply always yields a product far larger than the additive constant.

#### Stage 3: Byte swap

The four mantissa bytes are permuted:

```
Before:  M1  M2  M3  M4
After:   M4  M2' M3' M1

where M2' = M3 (old byte 3 moves to position 2)
      M3' = M2 (old byte 2 moves to position 3)
```

In other words: bytes 1 and 4 are swapped, and bytes 2 and 3 are swapped. This is equivalent to reversing the byte order of the 32-bit mantissa.

This is the scrambling step. Without it, the generator would be a straightforward multiplicative congruential generator on the mantissa, and consecutive outputs would be highly correlated. The byte reversal breaks up the linear structure by moving the least-significant multiplication results into the most-significant position and vice versa. The original source comments this as an attempt to *"SHUFFLE HI AND LO BYTES"* and *"TO SUPPOSEDLY MAKE IT MORE RANDOM"*.

#### Stage 4: Force into [0, 1)

The result is forced into the interval (0, 1.0) by four operations:

1. **Force positive**: The sign byte is cleared.

2. **Recycle the exponent**: The current exponent (which is whatever resulted from the multiply and normalize) is moved into the rounding byte. This is the extra state mentioned in Stage 1---it feeds back into the next iteration's multiply as the low 8 bits of the multiplier, providing a small amount of additional mixing.

3. **Set exponent to $80**: Exponent $80 means 2^0. A normalized MBF float with this exponent has its mantissa in [1.0, 2.0), so the value lies in [0.5, 1.0) when interpreted as a fraction (the implicit leading 1 sits at the 0.5 position). If the byte swap happened to clear the leading mantissa bit, the subsequent normalization will shift it back in---but this also decrements the exponent below $80, producing a value smaller than 0.5. The result always lies in (0, 1.0).

4. **Normalize**: Shift the mantissa left until the leading bit is set, pulling in bits from the rounding byte. Decrement the exponent for each shift. The rounding byte provides up to 8 bits of extra precision during this step.

The resulting value is both returned to the caller and stored back as the new seed.

#### Stage 5: Pack and store

The value is packed back into the 5-byte MBF format in memory. Before packing, the rounding byte is examined: if its high bit is set, the mantissa is incremented by 1 (with carry propagation). This is standard MBF rounding from 40-bit internal precision to 32-bit stored precision. In rare cases where the increment overflows all 32 mantissa bits, the mantissa is reset to $80000000 and the exponent is incremented.

The packed sign bit (bit 7 of byte 1) is set from the sign register, which is always zero for RND output, so bit 7 of byte 1 is always clear.

### The character decision

The one-liner evaluates `CHR$(205.5 + RND(1))`.

On the C64, `CHR$()` truncates its argument to an integer (toward zero, which for positive numbers is equivalent to floor). So:

- If `RND(1) < 0.5`: the sum is in [205.5, 206.0), truncated to **205**.
- If `RND(1) >= 0.5`: the sum is in [206.0, 206.5), truncated to **206**.

In the PETSCII character set (uppercase/graphics mode):

- **CHR$(205)** ($CD) is the diagonal line from upper-left to lower-right: `\`
- **CHR$(206)** ($CE) is the diagonal line from upper-right to lower-left: `/`

The semicolon after `CHR$(...)` suppresses the newline, so characters are printed sequentially, filling the 40-column screen. `GOTO 10` loops forever.

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

### Why the generator fails

The generator is a multiplicative congruential generator with a broken additive constant and a byte-reversal scramble:

- **The additive constant does nothing**: The `c` in `x = a*x + c` is too small to survive the floating-point addition, reducing the generator to a purely multiplicative recurrence with a byte swap. This means the generator can never escape a cycle once it enters one, and zero is an absorbing state.

- **The multiplier is poor**: 11879546 was not chosen for good spectral properties. The original source notes the constants were "TRUNCATED", suggesting they were adapted from a different precision format without re-optimizing.

- **MBF arithmetic quirks**: The 40-bit shift-and-add multiply, the rounding during pack, and the exponent recycling all introduce nonlinear perturbations that make formal analysis difficult but do not improve statistical quality.

The combination of these flaws produces a generator that visits 118,362 states out of a possible 274 billion before cycling. It is adequate for drawing a maze on screen. It is not adequate for much else.

## How the cycle was found

The cycle was found using [Brent's cycle detection algorithm](https://en.wikipedia.org/wiki/Cycle_detection#Brent's_algorithm), which finds the exact tail length (mu) and cycle length (lambda) of any eventually-periodic sequence using O(1) memory and at most mu + lambda steps.

The implementation (`cycle_detect.cpp`) uses an optimized state-step function that operates directly on the 5-byte MBF seed, reproducing the exact C64 shift-and-add multiply without floating-point conversion. The verification (`test_rnd.cpp`) independently confirms the result by generating 118,462 characters and comparing the seed and output at positions mu and mu + lambda.

Both computations complete in under a second on modern hardware.

## History: the same broken algorithm, everywhere, for a decade

The RND algorithm analyzed above was not unique to the Commodore 64. It shipped, with the **exact same constants**, in every 8-bit Microsoft BASIC from 1975 to 1985. The multiplier ($98 $35 $44 $7A = 11,879,546) and the useless additive constant ($68 $28 $B1 $46 = 3.927677739E-8) appear byte-for-byte in Altair BASIC, MBASIC for CP/M, Applesoft BASIC, Commodore PET BASIC, VIC-20 BASIC, C64 BASIC, C128 BASIC, TRS-80 Level II BASIC, and MSX BASIC. The dead FADD was never fixed on any of these platforms.

### It gets worse: the constant-width bug

Microsoft's original 6502 BASIC source defines the RND constants as **4 bytes each**. This is correct for platforms using 4-byte (32-bit) MBF floats. But platforms using 5-byte (40-bit) MBF floats --- which includes the PET, Apple II, VIC-20, C64, and C128 --- need 5-byte constants. The floating-point multiply and add routines expect 5-byte operands and will read one byte past the end of a 4-byte constant.

Since the two constants are adjacent in ROM (RMULZC immediately followed by RADDZC), the multiply routine reads the first byte of RADDZC ($68) as the 5th byte of the multiplier. The add routine reads whatever byte follows RADDZC in ROM.

**Commodore noticed and fixed this** starting with the VIC-20 (1981) by appending a `$00` byte to each constant, making them proper 5-byte MBF values. This fix carried forward to the C64, C128, and TED-based machines (C16, Plus/4).

**Applesoft BASIC was never fixed.** It shipped with the original 4-byte constants in a 5-byte-float system for the entire production life of the Apple II. The PET's early BASIC V1 and V4 ROMs have the same bug.

### Microsoft finally gave up and replaced it

When Microsoft moved to the IBM PC (GW-BASIC, BASICA, and later QBasic), they threw out the entire multiply-add-byteswap approach and replaced it with a proper 24-bit integer linear congruential generator:

```
x(n+1) = (x(n) * 214013 + 2531011) mod 2^24
```

The GW-BASIC source code ([released by Microsoft in 2020](https://github.com/microsoft/GW-BASIC)) contains the comment:

> *"DO NOT CHANGE THESE WITHOUT CONSULTING KNUTH VOL 2 CHAPTER 3 FIRST."*

So by the 8086 era, someone at Microsoft had read Knuth. The constants 214013 and 2531011 are properly chosen for a power-of-two modulus LCG. The additive constant actually works because it's integer addition, not floating-point. It was a clean break from a decade of shipping the same broken algorithm.

### The byte swap varies by platform

On Commodore hardware (PET, VIC-20, C64, C128), all four mantissa bytes are swapped (M1<->M4 and M2<->M3, a full byte-reversal). On non-Commodore 6502 platforms (Applesoft, OSI), only bytes 1 and 4 are swapped --- bytes 2 and 3 stay in place. This is controlled by a conditional assembly flag (`REALIO=3` for Commodore). The different swap patterns mean the cycle structure would differ between platforms even with identical constants.

### The "VERY POOR RND ALGORITHM" comment

The comments *"VERY POOR RND ALGORITHM"*, *"ALSO, CONSTANTS ARE TRUNCATED"*, and *"<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"* that appear in the [pagetable.com disassembly](https://www.pagetable.com/c64ref/c64disasm/) are from **community-authored commentaries**, not from Microsoft's original source code. The original Microsoft 6502 BASIC source ([released by Microsoft in 2025](https://github.com/microsoft/BASIC-M6502)) describes the constants neutrally as "RANDOM" and the byte swap as providing *"A RANDOM CHANCE OF GETTING A NUMBER LESS THAN OR GREATER THAN .5"*. There is no indication in the original source that the developers knew the additive constant was dead or that the algorithm was poor. They may simply never have tested it.

### Platform summary

| Platform | Float size | Constant fix? | Byte swaps | Algorithm |
|----------|-----------|---------------|------------|-----------|
| Altair BASIC (8080) | 4-byte MBF | N/A (correct width) | 2 (M1<->M4) | Multiply-add-swap |
| MBASIC / CP/M (Z80) | 4-byte MBF | N/A (correct width) | 2 | Multiply-add-swap |
| Applesoft BASIC | 5-byte MBF | **No (bug)** | 2 | Multiply-add-swap |
| PET BASIC V1/V4 | 5-byte MBF | **No (bug)** | 4 (full reversal) | Multiply-add-swap |
| VIC-20 BASIC | 5-byte MBF | **Yes (fixed)** | 4 | Multiply-add-swap |
| C64 BASIC V2 | 5-byte MBF | **Yes (fixed)** | 4 | Multiply-add-swap |
| C128 BASIC 7.0 | 5-byte MBF | **Yes (fixed)** | 4 | Multiply-add-swap |
| TED BASIC (C16/Plus4) | 5-byte MBF | **Yes (fixed)** | 4 | Multiply-add-swap |
| TRS-80 Level II (Z80) | 4-byte MBF | N/A (correct width) | 2 | Multiply-add-swap |
| GW-BASIC / BASICA | IEEE-ish | N/A | N/A | **24-bit integer LCG** |
| QBasic / QuickBASIC | IEEE-ish | N/A | N/A | **24-bit integer LCG** |

The same broken RND shipped on every 8-bit home computer that ran Microsoft BASIC, unchanged, for ten years. Millions of programs on millions of machines used it. It was finally replaced only when Microsoft moved to a completely different processor architecture.

## Building

```
cmake -B build
cmake --build build --config Release
./build/Release/test_rnd
./build/Release/cycle_detect
```

## References

- [C64 BASIC ROM Disassembly](https://www.pagetable.com/c64ref/c64disasm/) by Michael Steil
- [C64 Memory Map](https://www.pagetable.com/c64ref/c64mem/) by Michael Steil
- [*10 PRINT CHR$(205.5+RND(1)); : GOTO 10*](https://10print.org/) (MIT Press, 2012)
- [Microsoft BASIC for 6502 Original Source](https://github.com/microsoft/BASIC-M6502) (released by Microsoft, 2025)
- [Microsoft GW-BASIC Source](https://github.com/microsoft/GW-BASIC) (released by Microsoft, 2020)
- [mist64/msbasic reconstructed source](https://github.com/mist64/msbasic) by Michael Steil
- [mist64/cbmsrc Commodore original sources](https://github.com/mist64/cbmsrc) by Michael Steil
