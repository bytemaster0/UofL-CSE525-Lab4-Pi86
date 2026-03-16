; =============================================================================
;  Lab Program 5 -- Custom Timer Interrupt Service Routine (ISR)
;  Target: 8088/V20 running under DOS (COM file)
;
;  Purpose:
;    Installs a custom IRQ0 (INT 8h) handler that:
;      - Increments a tick counter in memory
;      - Writes the low byte of the counter to port 0x80 (checkpoint/trigger)
;      - Chains to the original BIOS INT 8h handler
;
;    The main program arms the custom ISR and then busy-waits for 50 ticks
;    (~2.75 seconds at 18.2 Hz) before restoring the original vector and
;    exiting.
;
;  Observe on logic analyzer:
;    - The interrupt acknowledge (INTA) bus cycle sequence
;    - The interrupt vector number (0x08) placed on the data bus during INTA
;    - The push/pop sequence as the CPU saves and restores flags+CS+IP
;      (visible as a burst of memory writes then reads to the stack segment)
;    - The trigger pulse on GPIO 17 that coincides with the ISR writing to
;      port 0x80 — correlate this with the INTA bus cycle timing
;    - The effect of interrupt latency: how many CLKs between the INTR line
;      going HIGH and the first INTA pulse
;
;  Build:  nasm -f bin -o prog5.com prog5_isr.asm
; =============================================================================

    BITS 16
    ORG  0x100

PORT_CHECKPOINT  EQU  0x80
TICK_TARGET      EQU  50         ; wait for this many timer ticks

; ---------------------------------------------------------------------------
; Main program
; ---------------------------------------------------------------------------
start:
    push cs
    pop  ds

    ; Zero the tick counter
    mov  word [tick_count], 0

    ; Checkpoint: about to install ISR
    mov  al, 0xB0
    out  PORT_CHECKPOINT, al

    ; Save original INT 8h vector from IVT (address 0x0020)
    xor  ax, ax
    mov  es, ax
    mov  ax, [es:0x0020]        ; original ISR offset
    mov  [orig_isr_off], ax
    mov  ax, [es:0x0022]        ; original ISR segment
    mov  [orig_isr_seg], ax

    ; Install our custom ISR
    cli                          ; disable interrupts during vector swap
    mov  ax, cs
    mov  [es:0x0022], ax         ; new ISR segment = CS
    mov  ax, custom_isr
    mov  [es:0x0020], ax         ; new ISR offset
    sti                          ; re-enable interrupts

    ; Checkpoint: ISR installed, entering wait loop
    mov  al, 0xB1
    out  PORT_CHECKPOINT, al

    ; Busy-wait until tick_count reaches TICK_TARGET
wait_loop:
    mov  ax, [tick_count]
    cmp  ax, TICK_TARGET
    jb   wait_loop

    ; Checkpoint: wait complete, restoring original ISR
    mov  al, 0xB2
    out  PORT_CHECKPOINT, al

    ; Restore original INT 8h vector
    cli
    xor  ax, ax
    mov  es, ax
    mov  ax, [orig_isr_off]
    mov  [es:0x0020], ax
    mov  ax, [orig_isr_seg]
    mov  [es:0x0022], ax
    sti

    ; Final checkpoint
    mov  al, 0xB3
    out  PORT_CHECKPOINT, al

    mov  ah, 0x4C
    xor  al, al
    int  0x21

; ---------------------------------------------------------------------------
; Custom ISR -- replaces INT 8h (IRQ0 / System Timer)
;
; Bus cycles generated on each invocation (all observable on logic analyzer):
;   PUSH FLAGS  -> 2x memory writes (SP-=2, FLAGS stored)
;   PUSH CS     -> 2x memory writes
;   PUSH IP     -> 2x memory writes
;   [ISR body]  -> memory read (tick_count), memory write (tick_count++)
;                  I/O write  (OUT 0x80)
;   IRET        -> 2x memory reads (IP restored), 2x memory reads (CS),
;                  2x memory reads (FLAGS)  [CPU implicit]
; ---------------------------------------------------------------------------
custom_isr:
    push ax
    push ds

    ; Point DS at our data segment (same as CS for COM files)
    push cs
    pop  ds

    ; Increment tick counter
    inc  word [tick_count]

    ; Write low byte of tick counter to checkpoint port (-> GPIO trigger pulse)
    mov  ax, [tick_count]
    out  PORT_CHECKPOINT, al

    ; Chain to original BIOS INT 8h so BIOS timekeeping stays intact
    pop  ds
    pop  ax

    ; Execute the original ISR via a far jump through the saved vector
    ; We push the old vector as a far return address, then IRET to it.
    ; This preserves the FLAGS that were on the stack when INT 8h was called.
    pushf
    call far [cs:orig_isr_off]

    iret

; ---------------------------------------------------------------------------
; Variables
; ---------------------------------------------------------------------------
tick_count:     dw  0
orig_isr_off:   dw  0
orig_isr_seg:   dw  0
