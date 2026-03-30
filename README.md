# goto10

In 2012, ten authors published a 324-page book through the MIT Press about a single line of Commodore 64 BASIC:

```
10 PRINT CHR$(205.5+RND(1)); : GOTO 10
```

The book, [*10 PRINT CHR$(205.5+RND(1)); : GOTO 10*](https://10print.org/), treats the program as a philosophical touchstone.  The book covers the Commodore 64 as a platform, the BASIC language as a phenomenon, the maze as a motif in art and culture, and the nature of randomness in computing:

"The eponymous program is treated as a distinct cultural artifact, but it also serves as a grain of sand from which entire worlds become visible; as a Rosetta Stone that yields important access to the phenomenon of creative computing and the way computer programs exist in culture... Members of the working group had demonstrated they could interpret a large program, a substantial body of code, but could they usefully interpret a very spare program such as this one?"

It's 324 pages of self-important disquisition, about the rich tableau of social and philosophical implications of that one line of Commodore 64 code.

But for some reason, none of the ten authors bothered to analyze *the actual algorithm* underlying the one-liner.  They treated the resultant maze as "random" and managed to fill up a large book on that questionable principle.

This project is an attempt to do what the book didn't: reverse-engineer, reimplement, and analyze the C64's `RND(1)` function at the bit level, validated against the actual C64 ROM running in the VICE emulator.

## Documentation

- [The algorithm in detail](doc/algorithm.md) — how RND(1) actually works on the C64, including hidden state
- [Cycle analysis](doc/cycle_analysis.md) — preliminary cycle detection results and VICE validation
- [Cross-platform history](doc/history.md) — the same broken algorithm shipped on every 8-bit Microsoft BASIC for a decade
- [Algebraic analysis](doc/algebraic_analysis.md) — byte-level matrix decomposition of the two-step map
- [Current status and open questions](todo.md) — known simulation bug, theories, next steps

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
