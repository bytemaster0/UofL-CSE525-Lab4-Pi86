; =============================================================================
;  Lab Program 3 -- Wait State Sensitivity Experiment
;  Target: 8088/V20 running under DOS (COM file)
;
;  Purpose:
;    Exercises the pi86 wait-state controller at I/O port 0x81.
;    For each wait-state count (0, 1, 2, 4, 7), the program:
;      1. Writes the count to port 0x81  (pi86 firmware will insert that many
;         extra CLK pulses before completing each subsequent bus cycle)
;      2. Writes a checkpoint to port 0x80 to mark the start of the timed
;         section on the logic analyzer
;      3. Copies a 128-byte block using REP MOVSB
;      4. Writes another checkpoint to mark the end
;
;    Students measure the time between checkpoints for each wait-state setting
;    using the logic analyzer's "time between cursors" feature, and plot the
;    relationship between wait states and memory bandwidth.
;
;  Expected observation:
;    With 0 wait states the copy takes T0 time.
;    With W wait states each of the 256 bus cycles (128 reads + 128 writes)
;    is extended by W clock periods, so total time ≈ T0 + 256 * W * T_clk.
;
;  Port 0x81 encoding:  bits [2:0] = wait state count (0-7), bits [7:3] = 0
;
;  Build:  nasm -f bin -o prog3.com prog3_waitstates.asm
; =============================================================================

    BITS 16
    ORG  0x100

PORT_CHECKPOINT  EQU  0x80
PORT_WAIT_STATES EQU  0x81
BLOCK_SIZE       EQU  128

start:
    push cs
    pop  ds
    push cs
    pop  es

    ; Announce start
    mov  al, 0xA0
    out  PORT_CHECKPOINT, al

    ; Iterate over each wait-state setting
    mov  bx, WS_VALUES      ; BX -> table of WS values
    mov  cx, 5              ; 5 entries in table

ws_loop:
    mov  al, [bx]           ; load wait-state count
    out  PORT_WAIT_STATES, al   ; program wait states in pi86 firmware

    ; Checkpoint encoding:  0x10 | wait_state_count
    or   al, 0x10
    out  PORT_CHECKPOINT, al    ; "start of timed block" marker

    ; Perform the block copy
    mov  si, src_block
    mov  di, dst_block
    push cx                     ; save outer loop counter
    mov  cx, BLOCK_SIZE
    cld
    rep  movsb
    pop  cx                     ; restore outer loop counter

    ; Checkpoint 0x20 = end of timed block
    mov  al, 0x20
    out  PORT_CHECKPOINT, al

    inc  bx
    dec  cx
    jnz  ws_loop

    ; Reset wait states to 0 before exiting
    xor  al, al
    out  PORT_WAIT_STATES, al

    ; Final checkpoint
    mov  al, 0xA1
    out  PORT_CHECKPOINT, al

    ; Exit
    mov  ah, 0x4C
    xor  al, al
    int  0x21

; ---------------------------------------------------------------------------
; Data  (must come AFTER code so execution starts at start: above)
; ---------------------------------------------------------------------------

; Wait-state test values table
WS_VALUES:
    db 0, 1, 2, 4, 7

src_block:
    times BLOCK_SIZE  db 0xA5   ; alternating pattern: easy to spot on bus

dst_block:
    times BLOCK_SIZE  db 0x00
