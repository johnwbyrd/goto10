; Debug: run RND 59780 times, then on 59781 manually call FMULT
; sub-steps and capture RES+FACOV after each multiplier byte.

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
    bcs error
    ldx #4
    jsr 0xFFC9
    bcs error

    lda #0
    sta count_lo
    sta count_hi

skip_loop:
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
    inc count_lo
    bne skip_nc
    inc count_hi
skip_nc:
    lda count_hi
    cmp #0xE9
    bne skip_loop
    lda count_lo
    cmp #0x84
    bne skip_loop

    ; === Step 59781: manual FMULT with captures ===

    ; Set up FAC = 1.0 for the RND argument
    lda #0x81
    sta 0x61
    lda #0x80
    sta 0x62
    lda #0x00
    sta 0x63
    sta 0x64
    sta 0x65
    sta 0x66

    ; Call MOVFM to unpack seed into FAC (same as RND does)
    lda #0x8B
    ldy #0x00
    jsr 0xBBA2

    ; Re-establish printer
    ldx #4
    jsr 0xFFC9

    ; Print seed and FAC before multiply
    lda #0x49       ; 'I'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Now do FMULT manually: CONUPK + MULDIV + clear RES + 5 byte rounds
    ; CONUPK: unpack constant at $E08D into ARG
    lda #0x8D
    ldy #0xE0
    jsr 0xBA8C      ; CONUPK

    ; MULDIV: combine exponents
    jsr 0xBAB7      ; MULDIV (sets FACEXP, FACSGN)

    ; Clear result registers
    lda #0
    sta 0x26        ; RESHO
    sta 0x27        ; RESMOH
    sta 0x28        ; RESMO
    sta 0x29        ; RESLO

    ; Re-establish printer
    ldx #4
    jsr 0xFFC9

    ; Byte 1: FACOV ($70)
    lda 0x70
    jsr 0xBA59      ; MLTPLY
    ldx #4
    jsr 0xFFC9
    lda #0x31       ; '1'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Byte 2: FACLO ($65)
    lda 0x65
    jsr 0xBA59      ; MLTPLY
    ldx #4
    jsr 0xFFC9
    lda #0x32       ; '2'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Byte 3: FACMO ($64)
    lda 0x64
    jsr 0xBA59      ; MLTPLY
    ldx #4
    jsr 0xFFC9
    lda #0x33       ; '3'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Byte 4: FACMOH ($63)
    lda 0x63
    jsr 0xBA59      ; MLTPLY
    ldx #4
    jsr 0xFFC9
    lda #0x34       ; '4'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Byte 5: FACHO ($62)
    lda 0x62
    jsr 0xBA5E      ; MLTPL1
    ldx #4
    jsr 0xFFC9
    lda #0x35       ; '5'
    jsr 0xFFD2
    lda #0x3A
    jsr 0xFFD2
    jsr print_res_facov

    ; Close
    jsr 0xFFCC
    lda #4
    jsr 0xFFC3
    lda #0xFF
    sta 0xC000
    rts

; Print RESHO($26) RESMOH($27) RESMO($28) RESLO($29) FACOV($70) + newline
print_res_facov:
    lda 0x26
    jsr print_hex_space
    lda 0x27
    jsr print_hex_space
    lda 0x28
    jsr print_hex_space
    lda 0x29
    jsr print_hex_space
    lda 0x70
    jsr print_hex_space
    lda #0x0D
    jsr 0xFFD2
    rts

error:
    jsr 0xFFCC
    lda #0x45
    jsr 0xFFD2
    lda #0x52
    jsr 0xFFD2
    lda #0x52
    jsr 0xFFD2
    lda #0x0D
    jsr 0xFFD2
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
count_hi:
    .byte 0
