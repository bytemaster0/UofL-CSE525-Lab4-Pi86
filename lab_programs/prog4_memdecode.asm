; =============================================================================
;  Lab Program 4 -- Memory Address Decode Explorer
;  Target: 8088/V20 running under DOS (COM file)
;
;  Purpose:
;    Demonstrates memory address decoding concepts by:
;      a) Writing distinct sentinel values to RAM addresses in different
;         memory regions (conventional, upper memory, ROM shadow), then
;         reading them back and checking which writes "took" (RAM) and
;         which were silently discarded (ROM).
;      b) Writing a checkerboard pattern to a 1KB block and reading it
;         back to show address line connectivity.
;
;    The pi86 firmware (augmented) write-protects 0xF0000–0xFFFFF so
;    writes there succeed on the bus but are silently discarded.
;
;  Observe on logic analyzer:
;    - Every write generates a MEM_WR bus cycle regardless of address
;    - The address bits A16-A19 distinguish conventional (0xxx) from
;      upper (Axxx-Exxx) from ROM (Fxxxx) regions
;    - Read-back after ROM write returns original ROM content (not 0xAA)
;
;  Build:  nasm -f bin -o prog4.com prog4_memdecode.asm
; =============================================================================

    BITS 16
    ORG  0x100

PORT_CHECKPOINT  EQU  0x80

; Segment addresses (paragraph = address >> 4)
SEG_CONVENTIONAL EQU  0x4000   ; 0x040000 -- conventional RAM (safely above FreeDOS footprint)
SEG_UPPER        EQU  0xB000   ; 0x0B0000 -- upper memory area (UMA)
SEG_ROM          EQU  0xF800   ; 0x0F8000 -- ROM region (write-protected)

SENTINEL_RAM     EQU  0xAA
SENTINEL_ROM_TRY EQU  0x55

start:
    push cs
    pop  ds

    ; -----------------------------------------------------------------------
    ; Test A: Conventional RAM (should succeed)
    ; -----------------------------------------------------------------------
    mov  al, 0x10
    out  PORT_CHECKPOINT, al        ; Checkpoint: about to write conventional RAM

    mov  ax, SEG_CONVENTIONAL
    mov  es, ax
    mov  byte [es:0], SENTINEL_RAM  ; Write 0xAA to 0x040000

    mov  al, 0x11
    out  PORT_CHECKPOINT, al        ; Checkpoint: about to read back

    mov  al, [es:0]                 ; Read back
    cmp  al, SENTINEL_RAM
    jne  test_a_fail

    mov  al, 0x12                   ; 0x12 = RAM write succeeded (expected)
    out  PORT_CHECKPOINT, al
    jmp  test_b

test_a_fail:
    mov  al, 0xE1                   ; 0xE1 = unexpected failure
    out  PORT_CHECKPOINT, al

    ; -----------------------------------------------------------------------
    ; Test B: ROM region write (should be silently discarded by pi86)
    ; -----------------------------------------------------------------------
test_b:
    mov  al, 0x20
    out  PORT_CHECKPOINT, al        ; Checkpoint: about to write ROM space

    mov  ax, SEG_ROM
    mov  es, ax
    mov  bl, [es:0]                 ; Save original ROM byte
    mov  byte [es:0], SENTINEL_ROM_TRY  ; Attempt write to ROM

    mov  al, 0x21
    out  PORT_CHECKPOINT, al        ; Checkpoint: about to read ROM back

    mov  al, [es:0]                 ; Read back
    cmp  al, SENTINEL_ROM_TRY
    je   test_b_written             ; if equal, write took effect (shouldn't)

    cmp  al, bl                     ; verify original byte is still there
    je   test_b_protected           ; original preserved = ROM protection working

    mov  al, 0xE2                   ; unexpected value
    out  PORT_CHECKPOINT, al
    jmp  test_c

test_b_protected:
    mov  al, 0x22                   ; 0x22 = ROM write correctly discarded
    out  PORT_CHECKPOINT, al
    jmp  test_c

test_b_written:
    mov  al, 0xE3                   ; 0xE3 = ROM write was NOT protected (unexpected)
    out  PORT_CHECKPOINT, al

    ; -----------------------------------------------------------------------
    ; Test C: Address-line checker -- write unique value to each power-of-2
    ; offset in a 1 KB window to verify all address lines toggle correctly
    ; -----------------------------------------------------------------------
test_c:
    mov  al, 0x30
    out  PORT_CHECKPOINT, al        ; Checkpoint: address line test start

    mov  ax, SEG_CONVENTIONAL
    mov  es, ax

    ; Write 0xNN to address offset (1 << N) for N = 0..9
    xor  bx, bx                 ; BX = bit index
    mov  cx, 10                 ; 10 address lines to test (A0..A9)
    mov  di, 1                  ; DI = 1 << BX offset

addr_write_loop:
    mov  al, bl                 ; data = bit index (distinguishes each location)
    mov  [es:di], al
    shl  di, 1                  ; next power-of-two address
    inc  bx
    dec  cx
    jnz  addr_write_loop

    ; Read back and verify
    mov  bx, 0
    mov  cx, 10
    mov  di, 1

addr_read_loop:
    mov  al, [es:di]
    cmp  al, bl
    jne  addr_fail
    shl  di, 1
    inc  bx
    dec  cx
    jnz  addr_read_loop

    mov  al, 0x31               ; 0x31 = address line test passed
    out  PORT_CHECKPOINT, al
    jmp  done

addr_fail:
    ; AL = value read, BL = expected value (= bit index that should be 1)
    ; Write 0xC0 | BL to report which address line failed
    mov  al, bl
    or   al, 0xC0
    out  PORT_CHECKPOINT, al

done:
    mov  al, 0x3F               ; Final checkpoint
    out  PORT_CHECKPOINT, al

    mov  ah, 0x4C
    xor  al, al
    int  0x21
