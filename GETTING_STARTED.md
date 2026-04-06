# Getting Started: pi86 Lab Walk-Through

This guide walks you through every step from unboxing the hardware to running your first assembly program and seeing bus cycles on the logic analyzer. Work through it **in order** — each section assumes the previous one succeeded.

Expect this walk-through to take about 45–60 minutes the first time.

---

## Step 0 — What You Should Have in Your Kit

Before touching anything, verify you have:

- [ ] Raspberry Pi with pi86 daughterboard already installed
- [ ] Pre-loaded microSD card (labeled "pi86 Lab")
- [ ] Logic analyzer + USB cable + at least 8 hook-tip probe leads
- [ ] USB keyboard
- [ ] HDMI cable + monitor
- [ ] USB-C (or micro-USB) power supply for the Pi
- [ ] Your laptop with logic-analyzer software installed

If anything is missing, stop and tell the instructor before proceeding.

>**Important:** If you need the original Pi3B/+ image, please download it here: https://drive.google.com/file/d/1z0neA5d8kHTbi1HleQAAHZKnSSlVTkxi/view?usp=sharing
You'll need to use the Raspberry Pi Imager, with Custom Image, to write it. Extract the 7z file first and then write to an SD card with at least 16GB of space.

---

## Step 1 — Physical Inspection

> **Warning:** The pi86 daughterboard carries a real 8088-compatible processor. It is static-sensitive. Keep it on the anti-static mat until you're ready to power on.

1. Lay the Pi flat on the mat with the GPIO header facing you.
2. Confirm the daughterboard is seated squarely. Every pin of the 40-pin header should be engaged — no pins should be visible between the two boards.
3. Locate the 8088 chip on the daughterboard. It is the 40-pin DIP in the ZIF socket. The notch on the chip should face the same direction as the silk-screen arrow.
4. Identify the test-point pads along the edge of the daughterboard. They are labeled with the signal names from the Equipment table in the lab assignment. Some boards use 0.1" header pins; others use exposed copper pads — either can accept hook-tip probes.

---

## Step 2 — First Power-On (No Logic Analyzer Yet)

Let's confirm the system boots before adding probes.

1. Insert the microSD card into the Pi's card slot (contacts facing the board).
2. Connect the USB keyboard and HDMI monitor.
3. Connect the power supply. **Do not connect it to the wall yet.**
4. Double-check that the 8088 chip is in its socket.
5. Plug in the power supply.
6. On boot, open a terminal.
7. Enter: cd pi86/code/v20
8. Run: ./run_pi86.sh



**Expected behavior within 10 seconds:**
- Pi status LEDs blink during Linux boot
- A CGA-style text screen appears on the monitor
- After ~5 seconds: `A:\>` DOS prompt appears

If you see a blank screen or no DOS prompt after 30 seconds, power off immediately and ask the instructor.

**Do not type anything else in the lab yet.** Just confirm the prompt is there, then power off to attach the probes.

---

## Step 3 — Attaching Logic Analyzer Probes

Power the Pi off before attaching probes.

### Channel-to-Signal Map

Attach hook-tip leads in this order. Work from channel 0 upward — it is easy to lose track if you start in the middle.

Reference the pi86 github project's x86.h reference table for required signals: https://github.com/homebrew8088/pi86/blob/main/code/v20/x86.h

The pinout used for Wiring Pi is seen here:

![description](https://github.com/bytemaster0/UofL-CSE525-Lab4-Pi86/blob/main/images/pipins.png)

Pay attention to AD0-7, CLK, IO/M, DT/R pins. Each logic level analyzer pin will require a ground pin! Find a suitable ground pin for the Wiring Pi implementation vs the physical Pi pin.

```
Ch 0  →  CLK   (CLK - check reference pin on 8088/NEC V20 pinout diagram)
Ch 1  →  ALE   (ALE - check reference pi on 8088/NEC V20 pinout diagram)
Ch 2  →  AD0   (lowest bit of the multiplexed bus)
Ch 3  →  AD7   (highest bit of the multiplexed bus)
Ch 4  →  A15   (upper address — always address, never data)
Ch 5  →  IO/M  (LOW = memory,  HIGH = I/O)
Ch 6  →  DT/R  (LOW = read,    HIGH = write)
```

For reference, the image below shows AD0-7 and ALE pins being connected to the logic level analyzer. You can direclty press the probe pins onto the vertical header. For extra ground, you can use the backplane of the pins, there should be just enough of the through-hole pin avaialble.

![description](https://github.com/bytemaster0/UofL-CSE525-Lab4-Pi86/blob/main/images/logicpins.jpg)


> **Note on GPIO trigger:** The pi86 daughterboard uses every one of the Pi's 28 available GPIO header pins for 8088 bus signals, so there is no free pin for a separate logic-analyzer trigger output. Use **ALE rising edge (Ch 1)** as your primary trigger throughout the lab. To locate a specific I/O write to port 0x80, look for the combination IO/M=HIGH and DT/R=HIGH in the captured data, then check the address bits.

In other lab sections, you will use the following pinout:
```
Ch 0  →  AD0   (Address and Data Pin 0)
Ch 1  →  AD1   (Address and Data Pin 1)
Ch 2  →  AD2   (Address and Data Pin 2)
Ch 3  →  AD3   (Address and Data Pin 3)
Ch 4  →  AD4   (Address and Data Pin 4)
Ch 5  →  AD5   (Address and Data Pin 5)
Ch 6  →  AD6   (Address and Data Pin 6)
Ch 7  →  AD7   (Address and Data Pin 7)
Ch 8  →  ALE   (Address Latch Enable pin, used as a memory address valid/access indicator)
```

>The above pinout will be called **Pinout Configuration 2** as needed.

**For Week 2 interrupt experiments, add:**
```
Ch 7  →  INTR  (physical header pin 38, match to INTR on 8088/NEC V20 pinout)
```

**Ground:** Attach the logic analyzer's ground lead to the GND test point (usually the pad nearest a board edge, marked GND). One ground connection is sufficient.

> **Probe tip:** Hook-tip leads can slip off small pads. After attaching each probe, give the wire a gentle tug — if the hook falls off, re-seat it and try twisting the hook to lock it onto the pad.

### Color Coding (Optional but Helpful)

Most logic analyzer kits include colored probe wires. Suggested convention, depends on what's available in the lab kit:

| Color | Signal type |
|-------|-------------|
| Black | GND |
| Yellow | CLK |
| Orange | ALE |
| White | Data/address bus |
| Red | Control signals (IO/M, DT/R) |
| Green | INTR (Week 2 only) |

---

## Step 4 — Logic Analyzer Software Setup

### 4a. Connect the Logic Analyzer

1. Connect the logic analyzer to your laptop via USB.
2. Open logic level analyzer software.
3. The analyzer should appear as a connected device in the top-left panel. If it does not, check the USB cable and reinstall the driver.

### 4b. Configure Channels

In the channel setup panel:

| Channel | Rename to | Voltage threshold |
|---------|-----------|------------------|
| 0 | CLK | 1.65 V (3.3 V logic) |
| 1 | ALE | 1.65 V |
| 2 | AD0 | 1.65 V |
| 3 | AD7 | 1.65 V |
| 4 | A15 | 1.65 V |
| 5 | IO/M | 1.65 V |
| 6 | DT/R | 1.65 V |

Rename your channels as needed and preset threshold voltages.

### 4c. Set Sample Rate and Trigger

- **Sample rate:** 10 MS/s (megasamples per second). At ~0.3 MHz bus clock, this gives about 33 samples per CLK period — more than enough to see clean edges.
- **Capture duration:** Start with **100 ms**. You can increase this later.
- **Trigger:** Rising edge on **ALE** (channel 1).
  - In Logic 2: click the trigger icon (lightning bolt) next to channel 1, select "Rising Edge".

Do **not** press Start yet.

---

## Step 5 — First Capture: Boot Sequence

1. Power on the Pi (with probes attached).
2. Immediately press **Start** on the logic analyzer. You want to capture the bus activity from the moment the 8088 comes out of reset.

   > If you miss the boot, that is fine — the 8088 keeps executing code at the DOS prompt. Press Start at any time to capture ongoing activity.

3. Wait 2–3 seconds, then press **Stop**.

### What You Should See

Zoom in until individual CLK pulses are visible. You should see a repeating pattern like this (simplified):

```
CLK    ‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|
ALE    __|‾‾‾|_____________________________|‾‾‾|__
AD0-7  ===ADDR===|=======DATA========|======ADDR==
IO/M   _________________________________________    (memory cycle: stays LOW)
DT/R   _____________________________|‾‾‾‾‾‾‾‾‾‾    (write: goes HIGH)
```

If everything looks like flat lines (no transitions), check:
- Is the power supply actually on? (Pi LEDs should be lit.)
- Are all ground connections secure?
- Are probe hooks on the correct test points?

If CLK is visible but ALE never pulses, the 8088 may not be running — the daughterboard may need to be reseated.

**If everything worked as expected**, you should see images like the following. Note: These were generated with probes in **Configuration 2.** They show a memory test in progress during POST. You can see both address and data being sent on AD0-AD7. You can see when ALE indicates an address and when data is live.
![description](https://github.com/bytemaster0/UofL-CSE525-Lab4-Pi86/blob/main/images/logic1.png)
![description](https://github.com/bytemaster0/UofL-CSE525-Lab4-Pi86/blob/main/images/logic2.png)

### Identifying a Memory Read Cycle

Zoom to a single ALE pulse and the two cycles that follow it:

1. **During ALE HIGH:** Look at AD0–AD7 and A15. This is the address. Note that A15 (Ch 4) tells you whether the address is in the lower 32 KB (A15=0) or upper (A15=1).
2. **After ALE goes LOW:**
   - IO/M (Ch 5) LOW = memory cycle
   - DT/R (Ch 6) LOW = read (CPU is receiving data)
   - AD0–AD7 carry data from the BIOS ROM (this is the opcode being fetched)

**Mark this on your capture** (Logic 2: right-click → Add Marker) and label it "MEM READ - BIOS fetch". You will use this in Deliverable 1.1.

---

## Step 6 — Run Your First Lab Program

With the DOS prompt visible on the monitor:

### 6a. Verify the programs are on the floppy

```
A:\> dir
```

You should see `PROG1.COM`, `PROG2.COM`, etc. If not, ask the instructor — the floppy image may need to be reflashed.

### 6b. Run Program 2 (the simplest to observe)

Program 2 generates a repeating, predictable I/O write pattern that is the easiest to find on the logic analyzer.

1. Keep the trigger on **Rising edge on ALE (Ch 1)**.
2. Set capture duration to **10 ms**.
3. Press **Start** on the logic analyzer.
4. Type `PROG2` at the DOS prompt and press Enter.

The analyzer will trigger on the first ALE pulse after you press Enter.

### 6c. What You Should See After Triggering

The capture will show:
- A brief burst of memory reads (the OS loading the program)
- Then a very regular sequence of I/O write cycles

To find the I/O write cycles quickly, look for segments where IO/M (Ch 5) is **HIGH** — that distinguishes I/O cycles from the surrounding memory reads.

Zoom into the I/O write cycles. They look different from memory cycles:

| Signal | Memory cycle | I/O cycle |
|--------|-------------|-----------|
| IO/M | LOW | **HIGH** |
| DT/R | varies | HIGH (write) |
| AD0–AD7 | data byte | data byte (walking bit: 0x01, 0x02, 0x04...) |
| A8–A15 | upper address | **all LOW** (port 0x80 fits in 8 bits) |

> **Checkpoint:** Can you see the walking-bit pattern on AD0–AD7? Zoom in until you can read the individual bit values. For the first I/O write, AD0 should be HIGH and AD1–AD7 LOW (value = 0x01). For the second write, AD1 is HIGH and the rest LOW (value = 0x02), and so on.

---

## Step 7 — Reading the Bus Trace CSV

Bus cycle logging is **off by default** because it generates ~1–2 MB/s of disk writes and is not needed for every exercise. Enable it by setting `PI86_LOG=1` when launching pi86:

```bash
# On the Pi — from the terminal running the firmware:
PI86_LOG=1 ./run_pi86.sh
```

When logging is active, the firmware prints `[buslog] Logging bus cycles to bus_trace.csv` at startup. When disabled, it prints `[buslog] Bus logging disabled.`

### Access via SSH

On your laptop, open a terminal:

```bash
ssh pi@<ip-address>
# default password: raspberry  (or as set by your instructor)

cd ~/pi86/code/v20
tail -20 bus_trace.csv
```

You should see output like:

```
cycle,type,address,data,wait_states
1,MEM_RD,0xFFFF0,0xEA,0
2,MEM_RD,0xFFFF1,0x00,0
3,MEM_RD,0xFFFF2,0x01,0
...
```

### Make Sense of the First Few Rows

The very first cycles are always BIOS fetches from address `0xFFFF0`. This is the 8088 reset vector. The bytes at that address are:

```
0xEA  0x00 0x01  0x00 0xF8
```

`0xEA` is the opcode for `JMP FAR`. The CPU is jumping to `0xF800:0x0100` — the start of the BIOS code in the pi86 firmware. You can verify this by looking at `Load_Bios()` in [firmware/x86.cpp](firmware/x86.cpp) at the lines that write `FFFF0`.

### Export for Spreadsheet Analysis

```bash
# On the Pi — copy to a USB drive or use scp from your laptop
scp pi@<ip-address>:~/pi86/code/v20/bus_trace.csv ./my_capture.csv
```

Open in Excel or LibreOffice Calc. Filter the `type` column to show only `MEM_WR` cycles, for example, to count writes during a specific program run.

---

## Step 8 — Assemble and Run Your Own Program

This section shows you the complete workflow you will use for every lab program you write.

### 8a. Write the Program

On your laptop, create a file `test.asm`:

```nasm
    BITS 16
    ORG  0x100

start:
    ; Write a known value to the debug checkpoint port
    mov  al, 0x42       ; ASCII 'B' -- easy to find in the CSV
    out  0x80, al       ; I/O write -> logs to bus_trace.csv + prints to Pi terminal

    ; Write a different value to confirm two distinct cycles
    mov  al, 0x43       ; ASCII 'C'
    out  0x80, al

    ; Exit to DOS
    mov  ah, 0x4C
    xor  al, al
    int  0x21
```

### 8b. Assemble It

```bash
nasm -f bin -o test.com test.asm
```

If NASM reports an error, check the indentation (NASM is whitespace-sensitive for labels) and that `BITS 16` and `ORG 0x100` are both present.

To see what binary was produced:

```bash
ndisasm -b 16 -o 0x100 test.com
```

Expected output:
```
00000100  B442              mov al,0x42
00000102  E680              out 0x80,al
00000104  B443              mov al,0x43
00000106  E680              out 0x80,al
00000108  B44C              mov ah,0x4c
0000010A  30C0              xor al,al
0000010C  CD21              int 0x21
```

### 8c. Copy to the pi86 Floppy

```bash
# Using mtools (installed via: sudo apt install mtools)
mcopy -i floppy.img test.com ::TEST.COM

# Or use SCP to put it on the Pi, then copy it into the image from there
scp test.com pi@<ip>:~/pi86/code/v20/
```

If using SCP, on the Pi:
```bash
sudo mount -o loop floppy.img /mnt
sudo cp test.com /mnt/TEST.COM
sudo umount /mnt
```

### 8d. Run It and Capture

1. Keep the trigger on rising edge of ALE (Ch 1).
2. Press Start on the analyzer.
3. At the DOS prompt: `TEST`
4. The analyzer triggers on the first ALE pulse after execution begins.

In the capture, scroll past the initial memory reads (the OS loader) and look for two I/O write cycles close together where IO/M (Ch 5) is HIGH. The data on AD0–AD7 should be `0x42` (= 0100 0010 binary: AD1 and AD6 HIGH) for the first write and `0x43` for the second.

Also check `bus_trace.csv`:
```
NNN,IO_WR,0x80,0x42,0
NNN,IO_WR,0x80,0x43,0
```

If you see both of these, your toolchain is working end-to-end.

---

## Step 9 — Try the Wait State Controller

This is a preview of the Week 2 experiment. It takes 2 minutes and makes wait states immediately tangible.

Type the following at the DOS prompt (we'll use DEBUG, which is included on the pi86 DOS image):

```
DEBUG
```

At the `-` prompt, enter these commands one at a time (do not type the comment text in parentheses):

```
o E1 04
o 80 AA
o E1 00
o 80 BB
q
```

The first `o E1 04` sets 4 wait states. The `o 80 AA` generates a checkpointed I/O write with 4 wait states. The `o E1 00` clears them, and `o 80 BB` generates a 0-wait-state write for comparison.

On the logic analyzer (trigger on ALE, capture a 50 ms window before typing the DEBUG commands), zoom into the I/O write for `0xAA` vs. the one for `0xBB`. You should see four extra CLK pulses between the address phase and the data phase in the `0xAA` cycle, and none in the `0xBB` cycle.

This is exactly what your Week 2 timing measurements will quantify.

Type `q` to exit DEBUG.

---

## Step 10 — Checklist Before Your Lab Session

Use this before each lab session to make sure you are ready:

- [ ] Pi boots to DOS prompt (Step 2)
- [ ] All 8 probe channels connected and labeled (Step 3)
- [ ] Logic analyzer software sees the device (Step 4a)
- [ ] Channels renamed and threshold set to 1.65 V (Step 4b)
- [ ] Sample rate set to 10 MS/s (Step 4c)
- [ ] `dir` at DOS prompt shows all 5 lab programs (Step 6a)
- [ ] Running PROG2 and looking for IO/M=HIGH cycles shows the I/O write pattern (Step 6b–6c)
- [ ] If using CSV logging: pi86 was launched with `PI86_LOG=1` and `bus_trace.csv` is updating (Step 7)
- [ ] NASM installed on your laptop and `test.com` assembles without errors (Step 8b)

If any item fails, resolve it before moving on to the lab exercises.

---

## Common Problems and Fixes

### Logic analyzer shows flat lines on all channels

- Most common cause: ground not connected. Verify the ground lead is clipped to the GND test point.
- Second cause: the Pi is powered on but the pi86 firmware crashed. SSH in and check `sudo journalctl -xe`. Restart the firmware: `sudo ./pi86`.

### ALE never pulses

- The 8088 chip may not be seated. Power off, remove and firmly re-insert the chip in the ZIF socket. Align the notch with the board marking.
- The CLK signal may not be reaching the chip. Verify `CLK` (Ch 0) is toggling. If CLK is flat, the Pi firmware is not running — SSH in and start it manually.

### Checkpoint writes don't appear in `bus_trace.csv` or on the Pi terminal

- Confirm the program is writing to port 0x80. On the logic analyzer, trigger on ALE and look for an I/O write cycle (IO/M=HIGH + DT/R=HIGH) at address 0x80.
- If the I/O write cycle appears on the bus but `[POST] Checkpoint...` is not printed on the Pi's terminal, the augmented firmware may not be installed. Ask the instructor.

### Program crashes or hangs at DOS prompt

- Reassemble with NASM and check `ndisasm` output — a common mistake is forgetting `ORG 0x100`.
- Verify the program returns to DOS with `int 21h / AH=4Ch` at the end.
- Check that segment registers are set correctly. In a COM file, CS=DS=ES=SS all point to the same segment. If your program uses ES for a different segment (as in prog4), make sure you push/pop DS correctly around ISR calls.

### `bus_trace.csv` is empty, missing, or not updating

- Logging is off by default. Make sure you launched pi86 with `PI86_LOG=1 ./run_pi86.sh`. The startup line `[buslog] Logging bus cycles to bus_trace.csv` confirms it is active.
- The file is flushed every 100 ms. If it still appears empty after a second, check permissions: `chmod 777 ~/pi86/code/v20/` and relaunch with `PI86_LOG=1`.

### NASM error: "parser: instruction expected"

- NASM requires instructions to be indented (at least one space or tab before the mnemonic). Labels must start in column 0 with a colon.
- Example of correct formatting:
  ```nasm
  my_label:           ; label starts at column 0
      mov ax, 0x10    ; instruction indented
  ```

### mtools "no such file" when running `make install`

- Verify the floppy image path in the Makefile (`FLOPPY` variable). The default path assumes you are working inside `lab_programs/` and the image is at `../code/v20/floppy.img`. Adjust if your directory layout differs.

---

## Reference: Key Keyboard Shortcuts

### DOS Commands (at the `A:\>` prompt)

| Command | Effect |
|---------|--------|
| `dir` | List files on current drive |
| `PROG1` | Run prog1.com |
| `DEBUG` | Start the DEBUG.COM debugger |
| `TYPE filename` | Print a text file to screen |
| `CLS` | Clear the screen |
| Ctrl+C | Interrupt a running program |

### DEBUG Debugger (at the `-` prompt)

| Command | Effect |
|---------|--------|
| `r` | Show all registers |
| `d 0100` | Dump memory at offset 0x100 |
| `u 0100` | Unassemble from offset 0x100 |
| `o 80 AA` | Write 0xAA to I/O port 0x80 |
| `i E3` | Read I/O port 0xE3 (mem-read count low byte) |
| `q` | Quit |
