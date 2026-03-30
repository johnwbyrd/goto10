# VICE Automation Reference

Everything learned the hard way about automating VICE 3.9 (x64sc) on Windows.

## Launching VICE with printer output

The working command line for BASIC programs that use `OPEN 4,4` / `PRINT#4`:

```
x64sc -warp -device4 1 -iecdevice4 -virtualdev4 -pr4drv ascii -pr4output text -pr4txtdev 0 -prtxtdev1 "output.txt" -autostart program.prg
```

### What each flag does

| Flag | Purpose |
|------|---------|
| `-warp` | Run at maximum speed (no speed cap) |
| `-device4 1` | Set device 4 type to Filesystem (1). **Without this, OPEN 4,4 fails with DEVICE NOT PRESENT.** |
| `-iecdevice4` | Enable IEC device emulation for device 4. `-` means enable, `+` means disable. |
| `-virtualdev4` | Enable virtual device traps for device 4. Required for BASIC's OPEN to intercept Kernal calls. |
| `-pr4drv ascii` | Printer driver: ascii (plain text, no control codes). Alternatives: raw, mps803, nl10. |
| `-pr4output text` | Output mode: text. Alternative: graphics (produces .bmp). |
| `-pr4txtdev 0` | Which text device slot (0-2) to use for printer 4's output. |
| `-prtxtdev1 "file"` | File path for text device slot 0. Note: `-prtxtdev1` is slot 0 (1-indexed flag, 0-indexed slot). |
| `-autostart program.prg` | Load and RUN the .prg file automatically. |

### Common failures

- **Missing `-device4 1`**: `?DEVICE NOT PRESENT ERROR`. The most common mistake.
- **Using Kernal OPEN from ML without `-virtualdev4`**: The virtual device trap only intercepts calls through BASIC's ROM. Direct Kernal JSR $FFC0 from machine language does NOT trigger the trap. ML programs cannot use the printer through this mechanism.
- **Output file not created**: The file is only written when CLOSE is called or VICE exits. If VICE crashes or is killed before CLOSE, no file.

## Tokenizing BASIC programs

Use `petcat` from the VICE bin directory:

```
petcat -w2 -o program.prg -- program.bas
```

`-w2` selects C64 BASIC V2 tokenization.

## Assembling 6502 programs with llvm-mos

### The BASIC SYS stub

Every .prg needs a BASIC stub so `RUN` works. The correct format (matching petcat output):

```asm
.byte 0x0c, 0x08       ; pointer to next line ($080C)
.byte 0x0a, 0x00       ; line number 10
.byte 0x9e             ; SYS token
.byte 0x20             ; space (required!)
.byte 0x32, 0x30, 0x36, 0x32  ; "2062" in PETSCII
.byte 0x00             ; end of line
.byte 0x00, 0x00       ; end of program
; ML entry point here at $080E = 2062
```

**Critical details:**
- The next-line pointer must point to the end-of-program bytes ($080C), not to the ML entry point.
- There must be a space ($20) after the SYS token.
- SYS 2062 = $080E, which is where ML code begins.
- The previous broken stub had the wrong next-line pointer ($080E instead of $080C) and no space after SYS, causing BASIC to parse ML bytes as tokens (screen garbage, crashes).

### Linker script

The default llvm-mos C64 linker script (`link.ld`) is unsuitable for programs that call BASIC ROM routines because it:
1. Uses zero page $02-$8F for compiler variables, stomping FAC1 ($61-$66), BITS ($68), FACOV ($70), RNDX ($8B-$8F)
2. Unmaps the BASIC ROM entirely (`INPUT(unmap-basic.o)`)

Custom linker script for assembly-only programs (`c64_spy.ld`):

```
MEMORY {
    ram (rwx) : ORIGIN = 0x0801, LENGTH = 0x97FF
}
SECTIONS {
    .text : { *(.text .text.*) } > ram
    .rodata : { *(.rodata .rodata.*) } > ram
    .data : { *(.data .data.*) } > ram
    .bss : { *(.bss .bss.*) } > ram
}
OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}
```

Build command:
```
clang --target=mos -Tc64_spy.ld -nostdlib -o program.prg program.s
```

### Assembly notes

- llvm-mos uses standard 6502 mnemonics in `.s` files
- Hex constants use `0x` prefix, not `$`
- Kernal addresses: CHROUT=0xFFD2, SETLFS=0xFFBA, SETNAM=0xFFBD, OPEN=0xFFC0, CLOSE=0xFFC3, CHKOUT=0xFFC9, CLRCHN=0xFFCC
- BASIC ROM RND entry: 0xE097
- Use `.section .text,"ax",@progbits` and `.globl _start` for the entry section
- Variables in `.section .bss,"aw",@nobits`

## Making VICE quit when the program finishes

VICE has no built-in mechanism for a C64 program to exit the emulator. The `quit` command cannot be attached to breakpoints.

### Solution: Remote monitor polling from Python

Launch VICE with `-remotemonitor`. A Python controller (`vice_run.py`) connects to the text monitor on port 6510, resumes execution, and periodically reads a sentinel address. When the value stops changing, the program is done.

```
python vice_run.py program.prg [--printer output.txt] [--timeout 10]
```

### How it works

1. VICE launches with `-remotemonitor -remotemonitoraddress ip4://127.0.0.1:6510`
2. Python connects to localhost:6510 (retries until VICE is ready)
3. Drains the initial monitor banner
4. Sends `x` to resume C64 execution
5. Every N seconds, sends `m C000 C000` to read the sentinel byte (this briefly pauses the CPU)
6. Sends `x` to resume
7. When two consecutive reads return the same value, the program is done
8. Sends `quit` to exit VICE
9. Waits for VICE process to die; kills it if it doesn't exit within timeout

### Critical details

- The text remote monitor is stop-and-go: sending ANY command while the CPU is running pauses it. Reading the response, then sending `x` resumes it.
- The C64 program should write to $C000 as its last act (before RTS). The specific value doesn't matter — the controller detects "value stopped changing."
- Do NOT use `watch store` — it fires during boot/autostart memory initialization, not just from the user program.
- Always kill VICE if it doesn't exit. Never leave orphaned processes.
- Never use sleep-based waits for VICE startup. Retry the socket connection instead.

## Zero page and the float workspace

The C64's floating point routines use these zero page locations:

| Address | Name | Purpose |
|---------|------|---------|
| $61 | FACEXP | FAC1 exponent |
| $62 | FACHO | FAC1 mantissa 1 |
| $63 | FACMOH | FAC1 mantissa 2 |
| $64 | FACMO | FAC1 mantissa 3 |
| $65 | FACLO | FAC1 mantissa 4 |
| $66 | FACSGN | FAC1 sign |
| $68 | BITS | Float shift sign extension — leaks into FMULT via MULSHF |
| $70 | FACOV | FAC1 rounding byte — persists across RND calls |
| $8B-$8F | RNDX | RND seed (5 bytes) |

**PEEK() calls float routines**, which clobber $68 and $70. You cannot use PEEK to observe BITS or FACOV without destroying them. Machine language (LDA $68 / STA elsewhere) is required.

**Any C compiler** that uses zero page $02-$8F will stomp these locations. Assembly is the only safe option for code that needs to preserve float state.
