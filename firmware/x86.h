#ifndef X86_H
#define X86_H

// ============================================================================
//  pi86 Augmented Firmware  --  x86.h
//  Based on the homebrew8088/pi86 project (GPL-3.0)
//
//  Additions for lab use:
//    - Bus cycle trace logger (CSV output)
//    - Configurable wait-state injection via I/O port 0x81
//    - Debug checkpoint port 0x80  (POST-code style, with GPIO trigger pulse)
//    - Bus activity statistics counters, readable at I/O 0x83-0x8A
//    - ROM write-protection for 0xF0000–0xFFFFF
// ============================================================================

// ---------------------------------------------------------------------------
// Processor type aliases
// ---------------------------------------------------------------------------
#define V20   88
#define V30   86

// ---------------------------------------------------------------------------
// GPIO pin assignments  (WiringPi numbering)
// ---------------------------------------------------------------------------
// Control / clock
#define PIN_CLK    29
#define PIN_RESET  27
#define PIN_ALE    26
#define PIN_IO_M   10
#define PIN_DTR    11
#define PIN_BHE     6

// Interrupt lines
#define PIN_INTR   28
#define PIN_INTA   31

// Multiplexed address/data bus AD0-AD7
#define AD0   25
#define AD1   24
#define AD2   23
#define AD3   22
#define AD4   21
#define AD5   30
#define AD6   14
#define AD7   13

// Upper address bus A8-A15
#define A8    12
#define A9     3
#define A10    2
#define A11    0
#define A12    7
#define A13    9
#define A14    8
#define A15   15

// Segment address A16-A19
#define A16   16
#define A17    1
#define A18    4
#define A19    5

// ---------------------------------------------------------------------------
// Extra GPIO outputs available on the Pi that are NOT used by the 8088 bus.
// These are used for logic-analyzer trigger / activity signals.
// ---------------------------------------------------------------------------
#define PIN_DEBUG_TRIGGER  17   // Pulsed when ASM writes to port 0x80
#define PIN_BUS_ACTIVITY   18   // Toggles on every bus transaction
// pins 19, 20 are available for future expansion

// ---------------------------------------------------------------------------
// Debug / lab I/O port map (all in 8-bit port range for easy OUT imm8)
// ---------------------------------------------------------------------------
#define PORT_DEBUG_CHECKPOINT  0x80  // Write: log POST code + pulse trigger GPIO
#define PORT_WAIT_STATE_CTRL   0x81  // Write: set # of wait states (0–7)
                                     // Read:  return current wait-state count
#define PORT_STATS_CTRL        0x82  // Write 0x00: reset all counters
#define PORT_STAT_MREAD_LO     0x83  // Read: mem-read  count, byte 0 (LSB)
#define PORT_STAT_MREAD_HI     0x84  // Read: mem-read  count, byte 1
#define PORT_STAT_MWRITE_LO    0x85  // Read: mem-write count, byte 0
#define PORT_STAT_MWRITE_HI    0x86  // Read: mem-write count, byte 1
#define PORT_STAT_IOREAD_LO    0x87  // Read: I/O-read  count, byte 0
#define PORT_STAT_IOREAD_HI    0x88  // Read: I/O-read  count, byte 1
#define PORT_STAT_IOWRITE_LO   0x89  // Read: I/O-write count, byte 0
#define PORT_STAT_IOWRITE_HI   0x8A  // Read: I/O-write count, byte 1

// ---------------------------------------------------------------------------
// ROM region  (writes here are silently discarded — acts like real ROM)
// ---------------------------------------------------------------------------
#define ROM_BASE  0xF0000u
#define ROM_TOP   0xFFFFFu

// ---------------------------------------------------------------------------
// Bus cycle trace log
// ---------------------------------------------------------------------------
#define BUS_LOG_CAPACITY  8192      // Circular-buffer depth (entries)
#define BUS_LOG_FILENAME  "bus_trace.csv"

typedef enum {
    CYC_MEM_READ  = 0,
    CYC_MEM_WRITE = 1,
    CYC_IO_READ   = 2,
    CYC_IO_WRITE  = 3,
    CYC_INTR_ACK  = 4
} BusCycleType;

typedef struct {
    unsigned long long  cycle_number;   // Global cycle counter at the time
    BusCycleType        type;
    unsigned int        address;        // 20-bit memory address or 16-bit I/O
    unsigned char       data;           // Byte transferred
    unsigned char       wait_states;    // Wait states inserted for this cycle
} BusLogEntry;

// ---------------------------------------------------------------------------
// Bus statistics (updated on every bus cycle)
// ---------------------------------------------------------------------------
typedef struct {
    unsigned int mem_reads;
    unsigned int mem_writes;
    unsigned int io_reads;
    unsigned int io_writes;
    unsigned int intr_acks;
} BusStats;

// ---------------------------------------------------------------------------
// Required system includes
// ---------------------------------------------------------------------------
#include <wiringPi.h>
#include <string>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <fstream>
#include <cstdio>

using namespace std;

// ---------------------------------------------------------------------------
// Globals visible across translation units
// ---------------------------------------------------------------------------
extern unsigned char RAM[0x100000];
extern unsigned char IO[0x10000];
extern volatile bool Stop_Flag;
extern volatile unsigned char  g_wait_states;     // 0–7 extra CLK pulses
extern volatile unsigned long long g_cycle_count;  // Incremented every bus cycle
extern BusStats g_stats;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Core system control
void Reset();
void Start(int Processor);
void Load_Bios(string Bios_file);

// Memory helpers
void  Write_Memory_Array(unsigned long long Address, char data[], int length);
void  Read_Memory_Array (unsigned long long Address, char *buf,  int length);
void  Write_Memory_Byte (unsigned long long Address, char byte);
char  Read_Memory_Byte  (unsigned long long Address);
void  Write_Memory_Word (unsigned long long Address, unsigned short word);

// I/O helpers
void  Write_IO_Array(unsigned long long Address, char data[], int length);
void  Read_IO_Array (unsigned long long Address, char *buf,  int length);
void  Write_IO_Byte (unsigned long long Address, char byte);
char  Read_IO_Byte  (unsigned long long Address);
void  Write_IO_Word (unsigned long long Address, unsigned short word);

// Interrupt request lines
void IRQ0();
void IRQ1();

// Bus trace log control
void BusLog_Init();
void BusLog_Shutdown();   // Flushes remaining entries and closes file
void BusLog_Reset();      // Clears the circular buffer (discards unsaved entries)

// Stats
void Stats_Reset();

#endif // X86_H
