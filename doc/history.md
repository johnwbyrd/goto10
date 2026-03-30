# History: The Same Broken Algorithm, Everywhere, for a Decade

The RND algorithm analyzed in this project was not unique to the Commodore 64. It shipped, with the **exact same constants**, in every 8-bit Microsoft BASIC from 1975 to 1985. The multiplier ($98 $35 $44 $7A = 11,879,546) and the useless additive constant ($68 $28 $B1 $46 = 3.927677739E-8) appear byte-for-byte in Altair BASIC, MBASIC for CP/M, Applesoft BASIC, Commodore PET BASIC, VIC-20 BASIC, C64 BASIC, C128 BASIC, TRS-80 Level II BASIC, and MSX BASIC. The dead FADD was never fixed on any of these platforms.

## The constant-width bug

Microsoft's original 6502 BASIC source defines the RND constants as **4 bytes each**. This is correct for platforms using 4-byte (32-bit) MBF floats. But platforms using 5-byte (40-bit) MBF floats --- which includes the PET, Apple II, VIC-20, C64, and C128 --- need 5-byte constants. The floating-point multiply and add routines expect 5-byte operands and will read one byte past the end of a 4-byte constant.

Since the two constants are adjacent in ROM (RMULZC immediately followed by RADDZC), the multiply routine reads the first byte of RADDZC ($68) as the 5th byte of the multiplier. The add routine reads whatever byte follows RADDZC in ROM.

**Commodore noticed and fixed this** starting with the VIC-20 (1981) by appending a `$00` byte to each constant, making them proper 5-byte MBF values. This fix carried forward to the C64, C128, and TED-based machines (C16, Plus/4).

**Applesoft BASIC was never fixed.** It shipped with the original 4-byte constants in a 5-byte-float system for the entire production life of the Apple II. The PET's early BASIC V1 and V4 ROMs have the same bug.

## Microsoft finally gave up and replaced it

When Microsoft moved to the IBM PC (GW-BASIC, BASICA, and later QBasic), they threw out the entire multiply-add-byteswap approach and replaced it with a proper 24-bit integer linear congruential generator:

```
x(n+1) = (x(n) * 214013 + 2531011) mod 2^24
```

The GW-BASIC source code ([released by Microsoft in 2020](https://github.com/microsoft/GW-BASIC)) contains the comment:

> *"DO NOT CHANGE THESE WITHOUT CONSULTING KNUTH VOL 2 CHAPTER 3 FIRST."*

So by the 8086 era, someone at Microsoft had read Knuth. The constants 214013 and 2531011 are properly chosen for a power-of-two modulus LCG. The additive constant actually works because it's integer addition, not floating-point. It was a clean break from a decade of shipping the same broken algorithm.

## The byte swap varies by platform

On Commodore hardware (PET, VIC-20, C64, C128), all four mantissa bytes are swapped (M1<->M4 and M2<->M3, a full byte-reversal). On non-Commodore 6502 platforms (Applesoft, OSI), only bytes 1 and 4 are swapped --- bytes 2 and 3 stay in place. This is controlled by a conditional assembly flag (`REALIO=3` for Commodore). The different swap patterns mean the cycle structure would differ between platforms even with identical constants.

## The "VERY POOR RND ALGORITHM" comment

The comments *"VERY POOR RND ALGORITHM"*, *"ALSO, CONSTANTS ARE TRUNCATED"*, and *"<<<THIS DOES NOTHING, DUE TO SMALL EXPONENT>>>"* that appear in the [pagetable.com disassembly](https://www.pagetable.com/c64ref/c64disasm/) are from **community-authored commentaries**, not from Microsoft's original source code. The original Microsoft 6502 BASIC source ([released by Microsoft in 2025](https://github.com/microsoft/BASIC-M6502)) describes the constants neutrally as "RANDOM" and the byte swap as providing *"A RANDOM CHANCE OF GETTING A NUMBER LESS THAN OR GREATER THAN .5"*. There is no indication in the original source that the developers knew the additive constant was dead or that the algorithm was poor. They may simply never have tested it.

## Platform summary

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
