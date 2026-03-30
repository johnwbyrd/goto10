; rnd_10print.s — RND(1) + FADD 205.5 + QINT, 50000 iterations.
; Print BITS, FACOV, and seed after each RND call.
; 16-bit counter. JMP for loop (no branch range issues).

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
    ; Open printer
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

    ; Init 16-bit counter to 0
    lda #0
    sta count_lo
    sta count_mi
    sta count_hi

mainloop:
    ; Capture BITS and FACOV before RND
    lda 0x68
    sta saved_bits
    lda 0x70
    sta saved_facov

    ; Set FAC = 1.0
    lda #0x81
    sta 0x61
    lda #0x80
    sta 0x62
    lda #0x00
    sta 0x63
    sta 0x64
    sta 0x65
    sta 0x66

    ; RND(1)
    jsr 0xE097

    ; Save seed
    lda 0x8B
    sta saved_seed
    lda 0x8C
    sta saved_seed+1
    lda 0x8D
    sta saved_seed+2
    lda 0x8E
    sta saved_seed+3
    lda 0x8F
    sta saved_seed+4

    ; FADD 205.5
    lda #<const_205_5
    ldy #>const_205_5
    jsr 0xB867

    ; QINT
    jsr 0xBC9B

    ; Re-establish printer
    ldx #4
    jsr 0xFFC9
    bcs loop_chkout_error

    ; Print 7 hex bytes
    lda saved_bits
    jsr print_hex_space
    lda saved_facov
    jsr print_hex_space
    lda saved_seed
    jsr print_hex_space
    lda saved_seed+1
    jsr print_hex_space
    lda saved_seed+2
    jsr print_hex_space
    lda saved_seed+3
    jsr print_hex_space
    lda saved_seed+4
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2

    ; Increment 24-bit counter
    inc count_lo
    bne no_carry
    inc count_mi
    bne no_carry
    inc count_hi
no_carry:

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
    ; Close printer
    jsr 0xFFCC
    lda #4
    jsr 0xFFC3

    ; Sentinel
    lda #0xFF
    sta 0xC000
    rts

; === Errors ===
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

; === Hex print ===
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

; === Constants ===
const_205_5:
    .byte 0x88, 0x4D, 0x80, 0x00, 0x00

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
saved_seed:
    .byte 0, 0, 0, 0, 0
