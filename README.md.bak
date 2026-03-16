# 8088 Bus Cycles & Assembly Programming Lab

A 2-week hands-on lab for University of Louisville CSE 525 Class (Microcomputer Design) built on the [pi86](https://github.com/homebrew8088/pi86) platform — a Raspberry Pi daughterboard that runs a real NEC V20 (8088-compatible) processor.

Students connect a logic analyzer to the live 8088 address/data/control bus, write 8088 assembly programs, and correlate waveform observations with datasheet timing specifications.

---

## What's in This Repository

| Path | Contents |
|------|----------|
| `firmware/` | Augmented `x86.cpp` / `x86.h` and new `buslog.cpp` / `buslog.h` — drop-in replacements for the pi86 source files |
| `lab_programs/` | Five 8088 assembly programs (`prog1`–`prog5`) and a `Makefile` |
| `LAB_ASSIGNMENT.md` | Full 2-week lab assignment with deliverables and grading rubric (130 points) |
| `GETTING_STARTED.md` | Step-by-step student walk-through from hardware inspection to first captured waveform |

## Firmware Augmentations

The `firmware/` files add the following on top of the upstream pi86 firmware:

- **Bus trace logger** — every bus cycle (address, data, type, wait states) is written to `bus_trace.csv` via a lock-free ring buffer and background flush thread.
- **Debug checkpoint port (0x80)** — `OUT 0x80, AL` logs a POST-code-style checkpoint to the CSV and prints it to the Pi's terminal, making it easy to correlate specific program events with bus captures.
- **Wait-state controller (0x81)** — `OUT 0x81, AL` inserts 0–7 extra CLK cycles per bus transaction in real time, allowing bandwidth measurements at different wait-state settings.
- **Bus statistics ports (0x82–0x8A)** — read-back counters for memory reads, memory writes, I/O reads, and I/O writes; reset with `OUT 0x82, 0`.
- **ROM write protection** — writes to the 0xF0000–0xFFFFF BIOS region are silently discarded by the firmware, matching the behavior of a real ROM chip.

## Lab Programs

| Program | What it exercises |
|---------|------------------|
| `prog1_memcopy.asm` | `REP MOVSB` / `REP CMPSB` — bulk memory read/write cycles |
| `prog2_io_toggle.asm` | I/O write cycles with a walking-bit pattern on the data bus |
| `prog3_waitstates.asm` | Memory bandwidth vs. wait-state count measurement |
| `prog4_memdecode.asm` | Address decoding, ROM write protection, address-line verification |
| `prog5_isr.asm` | Custom INT 8h handler with ISR chaining; full interrupt bus sequence |

## Hardware Requirements

- Raspberry Pi 3B+, 4 or 5 with pi86 daughterboard (NEC V20 / 8088-compatible processor)
- Logic analyzer with at least 7 channels and 10 MS/s sample rate (Saleae Logic or equivalent)
- Pre-loaded microSD card with Raspberry Pi OS, pi86 firmware built and running

## Quick Build

**Firmware** (on the Raspberry Pi, after copying `firmware/` files into `~/pi86/code/v20/`):
```bash
g++ -o pi86 pi86.cpp x86.cpp buslog.cpp cga.cpp vga.cpp drives.cpp timer.cpp \
    $(sdl2-config --cflags --libs) -lwiringPi -lpthread -std=c++11 -O2
```

**Lab programs** (requires NASM):
```bash
cd lab_programs
make        # assembles all .com files
make install  # copies them into floppy.img with mtools
```

## Getting Started

Students: see [GETTING_STARTED.md](GETTING_STARTED.md) for a guided walk-through.

## Based On

[homebrew8088/pi86](https://github.com/homebrew8088/pi86) by homebrew8088. This repository contains only the augmented firmware files and lab materials; the upstream pi86 source, BIOS ROM, and disk images are not included and must be obtained from the original repository.
