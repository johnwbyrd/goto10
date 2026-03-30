/* Custom linker script for rnd_spy.
 * No zero page usage. No C runtime. BASIC ROM stays mapped.
 * Code loads at $0801 like a normal PRG.
 */

MEMORY {
    ram (rwx) : ORIGIN = 0x0801, LENGTH = 0x97FF
    himem (rw) : ORIGIN = 0xC000, LENGTH = 0x1000
}

SECTIONS {
    .text : { *(.text .text.*) } > ram
    .rodata : { *(.rodata .rodata.*) } > ram
    .data : { *(.data .data.*) } > ram
    .bss : { *(.bss .bss.*) } > himem
}

OUTPUT_FORMAT {
    SHORT(ORIGIN(ram))
    TRIM(ram)
}
