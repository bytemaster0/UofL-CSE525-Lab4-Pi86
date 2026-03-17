# Lab 4: 8088 Bus Cycles, Memory Decode, and Assembly Programming on the pi86

**Course:** Microprocessor-Based Systems
**Duration:** 2 weeks (two lab sessions + out-of-lab work)
**Team size:** 2–3 students
**Prerequisites:** Lecture material on 8088 bus timing, memory address decoding, interrupt architecture, and x86 real-mode assembly syntax

---

## Overview

In this lab you will work with a real Intel 8088-compatible processor (NEC V20) running at approximately 0.3 MHz on the **pi86** platform — a custom daughterboard that plugs an 8088 chip into the GPIO header of a Raspberry Pi. The Pi acts as the complete system "motherboard": it drives the clock, serves memory and I/O from RAM arrays in software, and handles interrupts.

This arrangement is uniquely well-suited for observation because the Pi's firmware records every bus transaction to a CSV file (`bus_trace.csv`) and exposes two logic-analyzer trigger signals on the GPIO header. You will connect a logic analyzer to the bus signals, write assembly programs that exercise specific bus behaviors, and correlate what you see on the analyzer with what you predicted from the 8088 datasheet.

By the end you will have:

- Connected a logic analyzer to a live 8088 bus and correctly decoded read, write, and interrupt-acknowledge cycles
- Measured the effect of wait-state insertion on memory bus bandwidth
- Demonstrated ROM write protection through address decoding
- Written a working interrupt service routine and observed it on the bus

---

## Equipment

| Item | Qty | Notes |
|------|-----|-------|
| Raspberry Pi (3B+ or 4) with pi86 daughterboard | 1 per team | Pre-assembled and tested by instructor |
| Logic analyzer (8+ channels) | 1 per team | Saleae Logic, DSLogic, or equivalent |
| Hook-tip probe leads | 8+ | Clip to the PCB test points listed below |
| microSD card with pi86 image | 1 | Pre-loaded; contains augmented firmware and DOS |
| Laptop with logic-analyzer software | 1 per team | Saleae Logic 2 or DSView |
| USB keyboard | 1 | For DOS interaction |
| HDMI monitor | 1 | For CGA display output |

### Logic Analyzer Probe Points

All test points are labeled on the pi86 PCB silkscreen.

| Channel | Signal | 8088 Pin | Description |
|---------|--------|----------|-------------|
| 0 | CLK | 19 | Processor clock |
| 1 | ALE | 25 | Address Latch Enable — HIGH when address is valid |
| 2 | AD0 | 9 | Address/Data bus bit 0 (multiplexed) |
| 3 | AD7 | 16 | Address/Data bus bit 7 |
| 4 | A15 | 38 | Upper address bit (always address, not data) |
| 5 | IO/M | 28 | LOW = memory cycle, HIGH = I/O cycle |
| 6 | DT/R | 27 | Data direction: HIGH = write (CPU→memory), LOW = read |

> **Note on debug trigger:** The pi86 daughterboard uses all 28 available GPIO header pins for 8088 bus signals, leaving no free GPIO for a separate trigger output. Use **ALE rising edge (Ch 1)** as your primary trigger. To locate checkpoint I/O writes (port 0x80), look for cycles where IO/M (Ch 5) is HIGH and DT/R (Ch 6) is HIGH simultaneously — this uniquely identifies any I/O write bus cycle.

**For Week 2 interrupt experiments, add:**

| Channel | Signal | Pi Header Pin | Description |
|---------|--------|---------------|-------------|
| 7 | INTR | 38 | Interrupt request line driven by Pi |
| 8 | INTA | 28 | Interrupt acknowledge from CPU |

> **Tip:** If your analyzer has only 8 channels, you can skip INTA (Ch 8) for most experiments and re-assign Ch 7 to A8 or A12 to see more address bits during the ALE window.

---

## Background

### The 8088 Bus Cycle

The 8088 executes all memory and I/O operations as **bus cycles**. Each cycle consists of a minimum of four clock periods (T-states: T1–T4). Additional wait states (Tw) may be inserted between T3 and T4 by holding the READY input LOW.

```
      ___     ___     ___     ___     ___
CLK  |   |___|   |___|   |___|   |___|   |___
      T1      T2      Tw      T3      T4

ALE  ___________
    |           |___________________________________    (latches address at T1 falling edge)

AD0  ====ADDRESS=====|============DATA==============
-A7  (valid T1-T2)    (valid T3 for writes; driven by
                       memory for reads at T3)
```

**Key signals and their meanings:**

- **ALE (Address Latch Enable):** The CPU asserts ALE HIGH during T1 to indicate that AD0–AD7 and A8–A19 carry a valid address. External hardware (or the Pi) samples the address before ALE goes LOW.
- **IO/M:** Distinguishes memory cycles (LOW) from I/O cycles (HIGH).
- **DT/R (Data Transmit/Receive):** Indicates data direction. HIGH = CPU is writing (transmitting) to memory/device. LOW = CPU is reading (receiving).
- **The address is multiplexed:** AD0–AD7 carry address bits A0–A7 during T1, then switch to data bits D0–D7 during T2–T4.
- **Wait states:** Extra T-states inserted by asserting READY LOW. The pi86 augmented firmware simulates this in software by inserting additional CLK pulses before driving the data bus.

### The pi86 Debug Extensions

The augmented firmware adds several features specifically for this lab:

| I/O Port | Direction | Function |
|----------|-----------|----------|
| `0x80` | Write | **Debug checkpoint:** logs to `bus_trace.csv` and prints `[POST] Checkpoint 0xNN` to Pi terminal |
| `0x81` | Read/Write | **Wait-state count:** bits [2:0] set the number of extra CLK cycles inserted per bus transaction (0–7) |
| `0x82` | Write `0x00` | **Reset bus statistics counters** |
| `0x83`–`0x8A` | Read | **Bus statistics:** memory read count (2 bytes), memory write count, I/O read count, I/O write count |

**bus\_trace.csv** is created in the current working directory when pi86 is launched with `PI86_LOG=1`. Logging is off by default to avoid unnecessary disk writes. Enable it when a deliverable requires CSV analysis:

```bash
PI86_LOG=1 ./run_pi86.sh
```

Each row records one bus cycle:

```
cycle, type, address, data, wait_states
1,     MEM_RD, 0xFFFF0, 0xEA, 0
2,     MEM_RD, 0xFFFF1, 0x00, 0
...
```

---

## Week 1: Hardware Setup and Bus-Cycle Observation

### Pre-lab (complete before arriving)

1. Read Intel 8088 datasheet sections 2.3 ("Bus Operation") and 2.4 ("Wait States"). Note the exact timing of ALE, DT/R, IO/M relative to the CLK edges.
2. Predict the sequence of bus cycles generated by the instruction `MOV AX, [BX]` executed from address 0x00200 with BX = 0x1000. Sketch the waveforms for CLK, ALE, AD0–AD7, IO/M, and DT/R.
3. Review the pi86 GitHub repository README to understand the hardware connections.

### Part 1.1 — System Bring-Up

1. Connect the logic analyzer probes to the test points listed in the Equipment table (channels 0–7 minimum).
2. Power on the Pi. The pi86 firmware will start automatically and boot DOS from the floppy image.
3. Configure your logic analyzer:
   - Sample rate: at least 2 MS/s (10 MS/s recommended)
   - Trigger: rising edge on ALE (channel 1)
   - Pre-trigger: 2–4 CLK periods
4. Capture a 50 ms window at boot and export it.

**Deliverable 1.1:** Annotated screenshot of the boot capture identifying at least:
- One memory read cycle (BIOS fetch)
- One I/O write cycle (BIOS initialization)
- The ALE, address, and data phases labeled
- The IO/M and DT/R signal values for each cycle you identified

### Part 1.2 — Measuring a Single Bus Cycle

1. Run Program 1 (`prog1.com`) from the DOS prompt.
2. Trigger on the rising edge of ALE (channel 1). Capture 1 ms of data.
3. In the captured data, locate the burst of regular memory read/write cycles from the REP MOVSB block (look for a long run of alternating MEM\_RD and MEM\_WR at consecutive addresses). Zoom in to a single memory read cycle within that burst.
4. Measure:
   - T_CLK: the period of one CLK cycle (in microseconds)
   - T_ALE: the duration of the ALE HIGH pulse
   - T_ADDR: the time from ALE falling edge to address lines changing (address hold time)
   - T_DATA: the time from T2 rising edge until data is stable on the bus
   - T_CYCLE: total duration from T1 rising edge to T4 falling edge

**Deliverable 1.2:** Table of measurements + annotated waveform screenshot showing all five measurements. Compare to the datasheet minimum specifications.

### Part 1.3 — Distinguishing Cycle Types

Run Program 2 (`prog2.com`). This generates a pure sequence of I/O write cycles to address 0x80 with a walking-bit data pattern.

1. Capture 200 µs around the first trigger pulse.
2. On the same capture, find one I/O write cycle and compare it to any memory read cycles present.

**Deliverable 1.3:** Answer the following questions with supporting waveform evidence:
- What is the value on IO/M during an I/O write cycle vs. a memory read cycle?
- What is the value on DT/R during a write vs. a read?
- Can you see the address 0x80 on the address bus during the ALE window? Which specific signal lines are HIGH?
- What data values appear in sequence on AD0–AD7 after the ALE window? Do they match the walking-bit pattern in the source code?

### Part 1.4 — Bus Statistics

Still running (or re-run) Program 1. After the program exits, open `bus_trace.csv` from the Pi's filesystem (accessible via SSH or the built-in file browser).

1. Count the rows of each type (MEM\_RD, MEM\_WR).
2. Compare to what you predict from the source code: `REP MOVSB` for 256 bytes generates how many reads? How many writes?
3. Are there additional bus cycles beyond those from the MOVSB? Where do they come from?

**Deliverable 1.4:** Annotated `bus_trace.csv` excerpt (first 20 rows) identifying the fetch cycles, the REP MOVSB reads and writes, and the checkpoint I/O writes.

---

## Week 2: Assembly Programming and Advanced Bus Analysis

### Pre-lab (complete before arriving)

1. Install NASM on your laptop: `sudo apt install nasm` (Linux) or download the Windows binary.
2. Assemble and inspect the binary of Program 3:
   ```
   nasm -f bin -o prog3.com prog3_waitstates.asm
   ndisasm -b 16 -o 0x100 prog3.com | head -40
   ```
   Predict the sequence of checkpoint writes and the approximate cycle counts between them for each wait-state setting.
3. Write the IVT entry address for INT 8h in real-mode memory. What segment:offset would you find stored at physical address 0x00020?

### Part 2.1 — Wait State Experiment

Run Program 3 (`prog3.com`).

1. Trigger on ALE (Ch 1). Capture the full program run (roughly 10 ms).
2. For each wait-state setting (0, 1, 2, 4, 7), locate the two surrounding I/O write cycles at port 0x81 (wait-state change) and 0x80 (checkpoint codes 0x10|n and 0x20) and measure the elapsed time between the start and end checkpoints. Use `bus_trace.csv` to identify the cycle numbers of the checkpoint writes, then navigate to those positions in the Logic 2 timeline.
3. Plot **block-copy time vs. wait states** on graph paper or a spreadsheet.

**Deliverable 2.1:**
- Graph with measured points and a best-fit line
- Extracted T_CLK from your measurement and calculated clock frequency
- Annotated waveform showing a 0-wait-state cycle next to a 4-wait-state cycle from the same capture, with the extra CLK pulses labeled
- Answer: Using your measurements, calculate the effective memory bandwidth (bytes/second) at 0 and 4 wait states.

### Part 2.2 — Memory Address Decoding

Run Program 4 (`prog4.com`).

1. Trigger on ALE (Ch 1). Capture the entire run.
2. Locate the memory write cycle that attempts to write to the ROM region (look for addresses in the 0xF8000 range — A19=HIGH, A18=HIGH, A17=HIGH, A16=HIGH on the address bus during the ALE window).
3. Locate the subsequent read cycle at the same address.

**Deliverable 2.2:**
- Annotated waveform showing:
  - The ROM-write bus cycle (address and data visible on the bus)
  - The ROM-read bus cycle that follows, showing the original ROM data — NOT the written value
- Written explanation (3–5 sentences): Even though the write bus cycle completes normally and the address and data appear on the bus, why doesn't the data "stick"? What would a real hardware system need to do at the PCB level to implement write protection?
- Describe what signals a glue-logic chip (e.g., a 74LS138 decoder) would need to observe to distinguish a ROM write from a RAM write, and what output it would drive to disable the write.

### Part 2.3 — Custom Interrupt Service Routine

Run Program 5 (`prog5.com`).

1. Add channels 7 (INTR) and 8 (INTA) to the logic analyzer (see Equipment table).
2. Trigger on the rising edge of INTR (Ch 7).
3. Capture a 500 µs window showing the interrupt request through to the first ISR instruction.

**Deliverable 2.3:**
- Annotated waveform showing ALL of the following:
  1. INTR going HIGH (Pi asserts it)
  2. The CPU acknowledging: INTA going LOW twice (8088 INTA cycle has two LOW pulses)
  3. The interrupt vector number (0x08) placed on the data bus by the Pi during the second INTA pulse
  4. The stack memory writes as CPU pushes FLAGS, CS, IP (3 word writes = 6 bytes total)
  5. The fetch of the first ISR instruction from your custom ISR address
  6. The I/O write to port 0x80 from inside the ISR (with DEBUG\_TRIG pulse)

- Answer the following in your report:
  - How many CLK cycles elapsed between INTR rising and the first INTA pulse? Is this consistent with the 8088 datasheet (minimum interrupt latency is defined by the current instruction's execution time + response time)?
  - What is the interrupt vector table entry for INT 8h? (Give the segment:offset stored at physical address 0x00020.) Did it change after your program ran?
  - If two interrupts (IRQ0 and IRQ1) are pending simultaneously, what does the pi86 firmware do? Refer to the `x86.cpp` source code in your answer.

### Part 2.4 — Design Your Own Experiment (Open-Ended)

Design and implement one additional experiment that uses at least two of the pi86 firmware features (checkpoint port, wait states, bus statistics, or ROM protection) and produces a measurable result. Your experiment must:

- Have a clearly stated hypothesis ("We predict that...")
- Include a new `.asm` program assembled with NASM
- Include a logic analyzer capture with at least one annotated waveform
- Produce at least one quantitative result

Suggested ideas (you are not limited to these):
- Measure the overhead of the ISR (cycles consumed by PUSH/POP/IRET vs. the ISR body)
- Show the difference in bus cycle count between word-aligned vs. byte-misaligned `MOV` instructions
- Time a tight software delay loop and back-calculate the cycles-per-instruction for a known instruction sequence
- Implement a simple memory-mapped I/O register (write a value to a fixed address, read it back, observe that the same bus cycles occur as for a normal memory access)

**Deliverable 2.4:** Source code (`.asm`), assembly listing, annotated logic analyzer waveform, and a one-page write-up with hypothesis, method, results, and conclusion.

---

## Deliverables Summary

Submit a single PDF lab report with all deliverables in order. Include the names of all team members on the cover page.

| Deliverable | Content | Points |
|-------------|---------|--------|
| 1.1 | Boot capture with annotated bus cycles | 10 |
| 1.2 | Single-cycle timing measurement table + waveform | 15 |
| 1.3 | I/O vs. memory cycle comparison, four questions answered | 15 |
| 1.4 | Annotated bus_trace.csv excerpt + cycle-count analysis | 10 |
| 2.1 | Wait-state graph, bandwidth calculation, annotated waveform | 20 |
| 2.2 | ROM-protection waveforms + written explanation + decoder design | 15 |
| 2.3 | ISR waveform with six features labeled + three questions answered | 25 |
| 2.4 | Open-ended experiment: source, capture, write-up | 20 |
| **Total** | | **130** |

Late submissions: 10% per day, maximum 3 days.

---

## Assembly Reference

### NASM DOS COM File Template

```nasm
    BITS 16
    ORG  0x100       ; COM files load at offset 0x100 in the PSP segment

start:
    ; your code here

    mov  ah, 0x4C    ; DOS "terminate program" function
    xor  al, al      ; exit code 0
    int  0x21

data_section:
    ; your data here
```

### Useful 8088 Instructions for This Lab

| Instruction | Bus cycles generated | Notes |
|-------------|---------------------|-------|
| `MOV AL, [addr]` | 1 fetch + 1 mem read | 2 total cycles |
| `MOV [addr], AL` | 1 fetch + 1 mem write | |
| `REP MOVSB` | N reads + N writes | N = CX |
| `IN AL, imm8` | 1 fetch + 1 I/O read | port = imm8 (0x00–0xFF) |
| `OUT imm8, AL` | 1 fetch + 1 I/O write | |
| `IN AL, DX` | 1 fetch + 1 I/O read | port = DX (0x0000–0xFFFF) |
| `PUSH AX` | 1 fetch + 1 mem write | SP decremented by 2 |
| `POP AX` | 1 fetch + 1 mem read | |
| `INT n` | fetch + 3 writes (stack) + 1 IVT read | triggers interrupt sequence |
| `IRET` | fetch + 3 reads (stack) | restores IP, CS, FLAGS |

### Debug Port Quick Reference

```nasm
; Write a POST code to the debug checkpoint port
; (Generates an I/O write bus cycle AND logs to bus_trace.csv + Pi terminal)
mov  al, 0xNN          ; value to log (0x00–0xFF)
out  0x80, al

; Set wait states (0–7)
mov  al, N             ; N = 0, 1, 2, 3, 4, 5, 6, or 7
out  0x81, al

; Read memory-read count into AX
in   al, 0x83          ; low byte
mov  ah, al
in   al, 0x84          ; high byte
xchg al, ah            ; AX = 16-bit mem-read count

; Reset all counters
xor  al, al
out  0x82, al
```

---

## Grading Rubric for Waveform Annotations

A full-credit annotation must include:
- Signal labels on every trace
- Cursor or bracket indicating the region being discussed
- Value labels on address and data buses (e.g., "0x80", "0xAA")
- Polarity callouts for IO/M and DT/R ("IO/M = 1 → I/O cycle")
- For timing measurements: a double-headed arrow with the measured value and unit

Partial credit (50%) for annotations missing two or more of the above.

---

## Setup and Build Instructions

### Assembling the Lab Programs (on any Linux machine or the Pi itself)

```bash
# Install NASM
sudo apt install nasm mtools

# Clone or copy lab_programs/ to the Pi
cd lab_programs
make              # builds all .com files
make install      # copies .com files onto the floppy.img using mtools
```

### Rebuilding the Augmented Firmware

```bash
# On the Raspberry Pi, after copying firmware/ files into pi86/code/v20/
cd ~/pi86/code/v20
# The original Makefile compiles pi86.cpp; add the new source files:
g++ -o pi86 pi86.cpp x86.cpp buslog.cpp cga.cpp vga.cpp timer.cpp drives.cpp \
    -lwiringPi -lSDL2 -lpthread -std=c++11 -O2
sudo ./pi86
```

### Accessing bus\_trace.csv

The CSV file is created in the directory where pi86 is launched (typically `~/pi86/code/v20/`).

```bash
# On the Pi, while pi86 is running
tail -f bus_trace.csv       # live view of new bus cycles

# From your laptop over SSH
scp pi@<pi-ip>:~/pi86/code/v20/bus_trace.csv .
```

You can also import `bus_trace.csv` directly into a spreadsheet. The columns are:

```
cycle, type, address, data, wait_states
```

---

## Appendix A: 8088 Control Bus Encoding

The pi86 firmware reads three control signals into a 3-bit value to determine the cycle type:

| DT/R | IO/M | INTA | Hex | Cycle type |
|------|------|------|-----|------------|
| 0 | 0 | 1 | 0x04 | Memory Read |
| 1 | 0 | 1 | 0x05 | Memory Write |
| 0 | 1 | 1 | 0x06 | I/O Read |
| 1 | 1 | 1 | 0x07 | I/O Write |
| 0 | 0 | 0 | 0x02 | Interrupt Acknowledge |

*Note: INTA is active-LOW on the 8088; the pi86 header reads it as a logic level, so it appears as 0 (asserted) during INTA cycles.*

## Appendix B: Memory Map of the pi86 System

| Address Range | Type | Contents |
|---------------|------|----------|
| `0x00000`–`0x003FF` | RAM | Real-mode Interrupt Vector Table (IVT) |
| `0x00400`–`0x004FF` | RAM | BIOS Data Area (BDA) |
| `0x00500`–`0x9FFFF` | RAM | Conventional memory (DOS, programs, data) |
| `0xA0000`–`0xBFFFF` | RAM | Video memory (CGA frame buffer at 0xB8000) |
| `0xC0000`–`0xEFFFF` | RAM | Upper Memory Area (UMA) — available |
| `0xF0000`–`0xFFFFF` | **ROM** | BIOS ROM (write-protected by augmented firmware) |

## Appendix C: pi86 Augmented Firmware Feature Summary

### Debug GPIO Outputs (software-only)

The augmented firmware includes `PIN_DEBUG_TRIGGER` (WiringPi 17) and `PIN_BUS_ACTIVITY` (WiringPi 18) for debug trigger and bus activity signals. However, WiringPi pins 17–18 correspond to BCM GPIOs 28–29, which are not accessible on the Pi's standard 40-pin GPIO header — all 28 header pins are already used by the 8088 bus interface. The `digitalWrite` calls for these pins are harmless no-ops.

The debug checkpoint mechanism remains fully functional via two other paths:
- **Console output:** every `OUT 0x80, al` prints `[POST] Checkpoint 0xNN  (cycle N)` to the Pi's terminal.
- **CSV log:** every bus cycle (including the I/O write) is recorded in `bus_trace.csv`.

### Bus Trace Log Format

```
cycle,type,address,data,wait_states
1,MEM_RD,0xFFFF0,0xEA,0
2,MEM_RD,0xFFFF1,0x00,0
```

- `cycle`: monotonically increasing counter since power-on
- `type`: MEM\_RD, MEM\_WR, IO\_RD, IO\_WR, INTR\_ACK
- `address`: hex address (20-bit for memory, 16-bit for I/O)
- `data`: hex byte transferred (for 8088 mode)
- `wait_states`: number of extra CLK pulses inserted for this cycle

The log is flushed to disk every 500 ms by a background thread. Call `BusLog_Reset()` from the Pi-side C++ code (or write 0 to I/O port 0x82 from ASM) to clear the buffer for a new experiment.
