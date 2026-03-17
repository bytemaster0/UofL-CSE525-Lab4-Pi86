; =============================================================================
;  Lab Program 2 -- I/O Port Toggle Pattern
;  Target: 8088/V20 running under DOS (COM file)
;
;  Purpose:
;    Writes a walking-bit pattern (0x01, 0x02, 0x04, ..., 0x80) to I/O port
;    0x80 (the debug checkpoint port) 1000 times, then terminates.
;
;    This generates a highly regular sequence of I/O Write bus cycles that
;    are easy to identify on a logic analyzer because:
;      - The address bus holds a constant value (0x80) for every cycle
;      - The IO/M line is HIGH (I/O cycle, not memory)
;      - The data bus cycles through a power-of-two pattern
;
;  Observe on logic analyzer:
;    - IO/M signal: HIGH throughout (contrast with memory access programs)
;    - Constant address 0x80 on A7..A0 (A8-A19 should be zero)
;    - Walking-bit data pattern on AD0-AD7
;    - Period between writes = total clock cycles for the inner loop
;
;  Build:  nasm -f bin -o prog2.com prog2_io_toggle.asm
; =============================================================================

    BITS 16
    ORG  0x100

PORT_CHECKPOINT  EQU  0x80
REPEAT_COUNT     EQU  1000

start:
    ; Outer loop: repeat REPEAT_COUNT times
    mov  bx, REPEAT_COUNT

outer_loop:
    ; Inner loop: walk a single bit across bits 0-7
    mov  cl, 8          ; 8 bits to walk
    mov  al, 0x01       ; starting bit position

bit_loop:
    out  PORT_CHECKPOINT, al    ; I/O write bus cycle
    shl  al, 1                  ; shift bit left
    dec  cl
    jnz  bit_loop

    dec  bx
    jnz  outer_loop

    ; Final sentinel
    mov  al, 0xAA
    out  PORT_CHECKPOINT, al

    ; Exit to DOS
    mov  ah, 0x4C
    xor  al, al
    int  0x21
