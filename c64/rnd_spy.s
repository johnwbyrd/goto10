; rnd_spy.s — Call RND(1) repeatedly from 6502 assembly and print
; the seed bytes to the VICE virtual printer after each call.
;
; No BASIC interpreter involvement. Sets FAC to 1.0, calls the ROM's
; RND entry at $E097, captures the seed at $8B-$8F. This isolates the
; RND function from any BASIC float operations that might modify BITS
; or FACOV between calls.
;
; Output: one line per call, hex format: "BITS FACOV EXP M1 M2 M3 M4"
; Change the 24-bit counter comparison to adjust iteration count.
;
; Build: clang --target=mos -Tc64_spy.ld -nostdlib -o rnd_spy.prg rnd_spy.s
; Run:   python vice_run.py c64/rnd_spy.prg --printer output.txt

.section .text,"ax",@progbits

.globl _start
_start:
    .byte 0x0c, 0x08
    .byte 0x0a, 0x00
    .byte 0x9e
    .byte 0x20
    .byte 0x32, 0x30, 0x36, 0x32
    .byte 0x00
    .byte 0x00, 0x00

entry:
    lda #4
    ldx #4
    ldy #0
    jsr 0xFFBA
    lda #0
    ldx #0
    ldy #0
    jsr 0xFFBD
    jsr 0xFFC0
    bcs open_error
    ldx #4
    jsr 0xFFC9
    bcs chkout_error

    lda #0
    sta count_lo
    sta count_mi
    sta count_hi

mainloop:
    lda 0x68
    sta saved_bits
    lda 0x70
    sta saved_facov

    lda #0x81
    sta 0x61
    lda #0x80
    sta 0x62
    lda #0x00
    sta 0x63
    sta 0x64
    sta 0x65
    sta 0x66

    jsr 0xE097

    ldx #4
    jsr 0xFFC9
    bcs loop_chkout_error

    lda saved_bits
    jsr print_hex_space
    lda saved_facov
    jsr print_hex_space
    lda 0x8B
    jsr print_hex_space
    lda 0x8C
    jsr print_hex_space
    lda 0x8D
    jsr print_hex_space
    lda 0x8E
    jsr print_hex_space
    lda 0x8F
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2

    ; Increment 24-bit counter
    inc count_lo
    bne no_carry1
    inc count_mi
    bne no_carry1
    inc count_hi
no_carry1:

    ; Compare to 70000 = $011170
    lda count_hi
    cmp #0x01
    bne not_done
    lda count_mi
    cmp #0x11
    bne not_done
    lda count_lo
    cmp #0x70
    beq done
not_done:
    jmp mainloop

done:
    jsr 0xFFCC
    lda #4
    jsr 0xFFC3
    lda #0xFF
    sta 0xC000
    rts

open_error:
    pha
    jsr 0xFFCC
    lda #0x4F
    jsr 0xFFD2
    lda #0x45
    jsr 0xFFD2
    pla
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2
    lda #0xFF
    sta 0xC000
    rts

chkout_error:
    pha
    jsr 0xFFCC
    lda #0x43
    jsr 0xFFD2
    lda #0x45
    jsr 0xFFD2
    pla
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2
    lda #4
    jsr 0xFFC3
    lda #0xFF
    sta 0xC000
    rts

loop_chkout_error:
    pha
    jsr 0xFFCC
    lda #0x4C
    jsr 0xFFD2
    lda #0x45
    jsr 0xFFD2
    pla
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2
    lda #4
    jsr 0xFFC3
    lda #0xFF
    sta 0xC000
    rts

print_hex_space:
    pha
    lsr
    lsr
    lsr
    lsr
    tax
    lda hextable,x
    jsr 0xFFD2
    pla
    and #0x0F
    tax
    lda hextable,x
    jsr 0xFFD2
    lda #0x20
    jsr 0xFFD2
    rts

hextable:
    .byte 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37
    .byte 0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46

.section .bss,"aw",@nobits
count_lo:
    .byte 0
count_mi:
    .byte 0
count_hi:
    .byte 0
saved_bits:
    .byte 0
saved_facov:
    .byte 0
