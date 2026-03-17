; =============================================================================
;  Lab Program 1 -- Memory Copy Benchmark
;  Target: 8088/V20 running under DOS (COM file)
;
;  Purpose:
;    Copies a 256-byte block from SOURCE to DEST using REP MOVSB, then
;    verifies the copy with REP CMPSB.  POST-code checkpoints are written
;    to I/O port 0x80 before and after key operations so you can use them
;    as triggers on the logic analyzer.
;
;  Observe on logic analyzer:
;    - The ALE pulse at the start of each bus cycle
;    - The address appearing on AD0-AD7, A8-A19 during the ALE window
;    - The /RD or /WR control signals (via DTR and IO/M pins)
;    - How many clock cycles separate successive memory accesses
;    - The console output and CSV entry on each checkpoint write (PI86_LOG=1)
;
;  Build:  nasm -f bin -o prog1.com prog1_memcopy.asm
;  Run:    copy prog1.com to the pi86 DOS floppy image, then run from DOS
; =============================================================================

    BITS 16
    ORG  0x100          ; DOS COM file entry point

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
PORT_CHECKPOINT  EQU  0x80      ; debug checkpoint port
BLOCK_SIZE       EQU  256       ; bytes to copy

; ---------------------------------------------------------------------------
; Entry point
; ---------------------------------------------------------------------------
start:
    ; --- Checkpoint 0x01: program start ----------------------
    mov  al, 0x01
    out  PORT_CHECKPOINT, al

    ; Set up data segment (DS and ES both point to CS for a COM file)
    push cs
    pop  ds
    push cs
    pop  es

    ; --- Checkpoint 0x02: before memory copy -----------------
    mov  al, 0x02
    out  PORT_CHECKPOINT, al

    ; Set up REP MOVSB:  DS:SI -> source_data,  ES:DI -> dest_buffer
    mov  si, source_data
    mov  di, dest_buffer
    mov  cx, BLOCK_SIZE
    cld                         ; increment direction
    rep  movsb                  ; copy CX bytes  (CX memory reads + CX writes)

    ; --- Checkpoint 0x03: copy done, before verify -----------
    mov  al, 0x03
    out  PORT_CHECKPOINT, al

    ; Verify the copy with REP CMPSB
    mov  si, source_data
    mov  di, dest_buffer
    mov  cx, BLOCK_SIZE
    cld
    rep  cmpsb                  ; CX memory reads (two per iteration: SI and DI)
    jne  verify_fail

    ; --- Checkpoint 0x04: verify passed ----------------------
    mov  al, 0x04
    out  PORT_CHECKPOINT, al
    jmp  done

verify_fail:
    ; --- Checkpoint 0xFF: error sentinel ---------------------
    mov  al, 0xFF
    out  PORT_CHECKPOINT, al

done:
    ; --- Checkpoint 0x05: program end ------------------------
    mov  al, 0x05
    out  PORT_CHECKPOINT, al

    ; Return to DOS
    mov  ah, 0x4C
    mov  al, 0x00
    int  0x21

; ---------------------------------------------------------------------------
; Source data -- 256 bytes with a recognizable pattern
; Students can identify this pattern in the logic analyzer capture
; (look for the repeating 0x00..0xFF sequence on the data bus)
; ---------------------------------------------------------------------------
source_data:
%assign i 0
%rep 256
    db i
    %assign i i+1
%endrep

; ---------------------------------------------------------------------------
; Destination buffer -- zero-initialized
; ---------------------------------------------------------------------------
dest_buffer:
    times 256  db 0x00
