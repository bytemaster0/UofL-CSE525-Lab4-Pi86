// ============================================================================
//  pi86 Augmented Firmware  --  x86.cpp
//  Based on the homebrew8088/pi86 project by homebrew8088 (GPL-3.0)
//
//  Lab augmentations (marked with  [LAB]):
//    1. Bus-cycle logger        — every transaction recorded to bus_trace.csv
//    2. Configurable wait states — extra CLK pulses, controlled by I/O 0xE1
//    3. Debug checkpoint port   — I/O write to 0x80 prints to console + CSV
//    4. ROM write protection    — writes to 0xF0000–0xFFFFF are silently dropped
//    5. Bus statistics counters — readable at I/O 0xE3-0xEA from ASM code
// ============================================================================

#include "x86.h"
#include "buslog.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
unsigned char RAM[0x100000];
unsigned char IO[0x10000];

bool IRQ0_Flag = false;
bool IRQ1_Flag = false;

volatile bool              Stop_Flag    = false;
volatile unsigned char     g_wait_states = 0;         // [LAB] default: 0 wait states
volatile unsigned long long g_cycle_count = 0;        // [LAB] global bus cycle counter
BusStats                   g_stats = {0, 0, 0, 0, 0}; // [LAB] bus statistics

// ---------------------------------------------------------------------------
// [LAB] Stats reset
// ---------------------------------------------------------------------------
void Stats_Reset()
{
    memset(&g_stats, 0, sizeof(g_stats));
    printf("[stats] Counters reset.\n");
}

// ---------------------------------------------------------------------------
// Interrupt helpers
// ---------------------------------------------------------------------------
void IRQ0()
{
    IRQ0_Flag = true;
    digitalWrite(PIN_INTR, HIGH);
}

void IRQ1()
{
    IRQ1_Flag = true;
    digitalWrite(PIN_INTR, HIGH);
}

static char Read_Interrupts()
{
    char intr = IRQ0_Flag;
    intr = intr + (IRQ1_Flag << 1);
    return intr;
}

// ---------------------------------------------------------------------------
// System bus I/O
// ---------------------------------------------------------------------------
static int Read_Address()
{
    int Address;
    Address  = digitalRead(AD0);
    Address += (digitalRead(AD1)  << 1);
    Address += (digitalRead(AD2)  << 2);
    Address += (digitalRead(AD3)  << 3);
    Address += (digitalRead(AD4)  << 4);
    Address += (digitalRead(AD5)  << 5);
    Address += (digitalRead(AD6)  << 6);
    Address += (digitalRead(AD7)  << 7);
    Address += (digitalRead(A8)   << 8);
    Address += (digitalRead(A9)   << 9);
    Address += (digitalRead(A10)  << 10);
    Address += (digitalRead(A11)  << 11);
    Address += (digitalRead(A12)  << 12);
    Address += (digitalRead(A13)  << 13);
    Address += (digitalRead(A14)  << 14);
    Address += (digitalRead(A15)  << 15);
    Address += (digitalRead(A16)  << 16);
    Address += (digitalRead(A17)  << 17);
    Address += (digitalRead(A18)  << 18);
    Address += (digitalRead(A19)  << 19);
    return Address;
}

static char Read_Control_Bus()
{
    char Control_Bus;
    Control_Bus  = digitalRead(PIN_DTR);
    Control_Bus += (digitalRead(PIN_IO_M) << 1);
    Control_Bus += (digitalRead(PIN_INTA) << 2);
    return Control_Bus;
}

static char Read_Memory_Bank()
{
    char Memory_Bank;
    Memory_Bank  = digitalRead(AD0);
    Memory_Bank += (digitalRead(PIN_BHE) << 1);
    return Memory_Bank;
}

// ---------------------------------------------------------------------------
// Data bus direction control
// ---------------------------------------------------------------------------
static void Data_Bus_Direction_8088_IN()
{
    pinMode(AD0, INPUT); pinMode(AD1, INPUT); pinMode(AD2, INPUT); pinMode(AD3, INPUT);
    pinMode(AD4, INPUT); pinMode(AD5, INPUT); pinMode(AD6, INPUT); pinMode(AD7, INPUT);
}

static void Data_Bus_Direction_8088_OUT()
{
    pinMode(AD0, OUTPUT); pinMode(AD1, OUTPUT); pinMode(AD2, OUTPUT); pinMode(AD3, OUTPUT);
    pinMode(AD4, OUTPUT); pinMode(AD5, OUTPUT); pinMode(AD6, OUTPUT); pinMode(AD7, OUTPUT);
}

static void Data_Bus_Direction_8086_IN()
{
    Data_Bus_Direction_8088_IN();
    pinMode(A8,  INPUT); pinMode(A9,  INPUT); pinMode(A10, INPUT); pinMode(A11, INPUT);
    pinMode(A12, INPUT); pinMode(A13, INPUT); pinMode(A14, INPUT); pinMode(A15, INPUT);
}

static void Data_Bus_Direction_8086_OUT()
{
    Data_Bus_Direction_8088_OUT();
    pinMode(A8,  OUTPUT); pinMode(A9,  OUTPUT); pinMode(A10, OUTPUT); pinMode(A11, OUTPUT);
    pinMode(A12, OUTPUT); pinMode(A13, OUTPUT); pinMode(A14, OUTPUT); pinMode(A15, OUTPUT);
}

static void Write_To_Data_Port_0_7(char Byte)
{
    digitalWrite(AD0, Byte & 1);
    digitalWrite(AD1, (Byte >> 1) & 1);
    digitalWrite(AD2, (Byte >> 2) & 1);
    digitalWrite(AD3, (Byte >> 3) & 1);
    digitalWrite(AD4, (Byte >> 4) & 1);
    digitalWrite(AD5, (Byte >> 5) & 1);
    digitalWrite(AD6, (Byte >> 6) & 1);
    digitalWrite(AD7, (Byte >> 7) & 1);
}

static void Write_To_Data_Port_8_15(char Byte)
{
    digitalWrite(A8,  Byte & 1);
    digitalWrite(A9,  (Byte >> 1) & 1);
    digitalWrite(A10, (Byte >> 2) & 1);
    digitalWrite(A11, (Byte >> 3) & 1);
    digitalWrite(A12, (Byte >> 4) & 1);
    digitalWrite(A13, (Byte >> 5) & 1);
    digitalWrite(A14, (Byte >> 6) & 1);
    digitalWrite(A15, (Byte >> 7) & 1);
}

static char Read_From_Data_Port_0_7()
{
    char ret = 0;
    ret  = digitalRead(AD0);
    ret += (digitalRead(AD1) << 1);
    ret += (digitalRead(AD2) << 2);
    ret += (digitalRead(AD3) << 3);
    ret += (digitalRead(AD4) << 4);
    ret += (digitalRead(AD5) << 5);
    ret += (digitalRead(AD6) << 6);
    ret += (digitalRead(AD7) << 7);
    return ret;
}

static char Read_From_Data_Port_8_15()
{
    char ret = 0;
    ret  = digitalRead(A8);
    ret += (digitalRead(A9)  << 1);
    ret += (digitalRead(A10) << 2);
    ret += (digitalRead(A11) << 3);
    ret += (digitalRead(A12) << 4);
    ret += (digitalRead(A13) << 5);
    ret += (digitalRead(A14) << 6);
    ret += (digitalRead(A15) << 7);
    return ret;
}

// ---------------------------------------------------------------------------
// Clock pulse generation
// ---------------------------------------------------------------------------
static void CLK()
{
    // HIGH phase  (repeated writes add duty-cycle width for stable signal)
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CLK, HIGH); digitalWrite(PIN_CLK, HIGH);
    // LOW phase
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CLK, LOW);  digitalWrite(PIN_CLK, LOW);
}

// [LAB] Insert 'n' additional wait-state CLK pulses.
static inline void Insert_Wait_States(unsigned char n)
{
    for (unsigned char i = 0; i < n; ++i)
        CLK();
}

// ---------------------------------------------------------------------------
// [LAB] Debug trigger pulse  (no-op on Pi 3B/4B — pins 17/18 not on 40-pin header)
// ---------------------------------------------------------------------------
static inline void Pulse_Debug_Trigger()
{
    // ~microsecond pulse — wide enough for any logic analyzer at >1 MS/s
    digitalWrite(PIN_DEBUG_TRIGGER, HIGH);
    digitalWrite(PIN_DEBUG_TRIGGER, HIGH);
    digitalWrite(PIN_DEBUG_TRIGGER, HIGH);
    digitalWrite(PIN_DEBUG_TRIGGER, HIGH);
    digitalWrite(PIN_DEBUG_TRIGGER, LOW);
}

// ---------------------------------------------------------------------------
// [LAB] Handle special debug I/O writes from ASM programs.
// Returns true if the write was fully handled (should NOT be stored in IO[]).
// Returns false if the write should also be stored normally.
// ---------------------------------------------------------------------------
static bool Handle_Debug_IO_Write(unsigned int Address, unsigned char Data)
{
    switch (Address) {
        case PORT_DEBUG_CHECKPOINT:
            // ASM did:  OUT 0x80, al
            // Log the checkpoint and print to console (GPIO pulse is a no-op).
            printf("[POST] Checkpoint 0x%02X  (cycle %llu)\n",
                   Data, (unsigned long long)g_cycle_count);
            Pulse_Debug_Trigger();
            return false;   // also store in IO[] so BIOS code can read it back

        case PORT_WAIT_STATE_CTRL:
            // ASM did:  OUT 0xE1, al  (bits 2:0 = wait state count, 0–7)
            g_wait_states = Data & 0x07;
            IO[PORT_WAIT_STATE_CTRL] = g_wait_states;  // keep shadow in sync for IN 0xE1 readback
            printf("[wait] Wait states set to %u\n", (unsigned)g_wait_states);
            return true;

        case PORT_STATS_CTRL:
            if (Data == 0x00)
                Stats_Reset();
            return true;

        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// [LAB] Prepare the stats I/O shadow registers so the 8088 can read them.
// Called just before servicing an I/O read in the stats port range.
// ---------------------------------------------------------------------------
static void Refresh_Stats_IO()
{
    IO[PORT_STAT_MREAD_LO]  = (unsigned char)(g_stats.mem_reads  & 0xFF);
    IO[PORT_STAT_MREAD_HI]  = (unsigned char)(g_stats.mem_reads  >> 8);
    IO[PORT_STAT_MWRITE_LO] = (unsigned char)(g_stats.mem_writes & 0xFF);
    IO[PORT_STAT_MWRITE_HI] = (unsigned char)(g_stats.mem_writes >> 8);
    IO[PORT_STAT_IOREAD_LO] = (unsigned char)(g_stats.io_reads   & 0xFF);
    IO[PORT_STAT_IOREAD_HI] = (unsigned char)(g_stats.io_reads   >> 8);
    IO[PORT_STAT_IOWRITE_LO]= (unsigned char)(g_stats.io_writes  & 0xFF);
    IO[PORT_STAT_IOWRITE_HI]= (unsigned char)(g_stats.io_writes  >> 8);
    IO[PORT_WAIT_STATE_CTRL]= g_wait_states;
}

// ---------------------------------------------------------------------------
// [LAB] Toggle the bus-activity indicator GPIO
// ---------------------------------------------------------------------------
static bool s_bus_activity_state = false;
static inline void Toggle_Bus_Activity()
{
    s_bus_activity_state = !s_bus_activity_state;
    digitalWrite(PIN_BUS_ACTIVITY, s_bus_activity_state ? HIGH : LOW);
}

// ============================================================================
// Main System Bus Loop  --  8088 (V20) variant
// ============================================================================
static void Start_System_Bus_8088()
{
    int  Address;
    char ctrl;
    unsigned char ws = 0;   // local copy of wait states for this cycle

    while (Stop_Flag != true) {
        CLK();

        if (digitalRead(PIN_ALE) == 1) {
            Address = Read_Address();
            CLK();
            ctrl = Read_Control_Bus();
            ws   = g_wait_states;          // [LAB] snapshot wait-state setting
            g_cycle_count++;               // [LAB]
            Toggle_Bus_Activity();         // [LAB]

            switch (ctrl) {

                // ----------------------------------------------------------
                // Memory Read
                // ----------------------------------------------------------
                case 0x04: {
                    unsigned char byte_out = RAM[Address];
                    Data_Bus_Direction_8088_OUT();
                    Write_To_Data_Port_0_7(byte_out);
                    Insert_Wait_States(ws);    // [LAB]
                    CLK(); CLK();
                    Data_Bus_Direction_8088_IN();
                    // [LAB] log + stats
                    g_stats.mem_reads++;
                    BusLog_Push(CYC_MEM_READ, (unsigned)Address, byte_out, ws);
                    break;
                }

                // ----------------------------------------------------------
                // Memory Write
                // [LAB] ROM protection: silently discard writes to ROM region
                // ----------------------------------------------------------
                case 0x05: {
                    unsigned char byte_in = Read_From_Data_Port_0_7();
                    if ((unsigned)Address < ROM_BASE || (unsigned)Address > ROM_TOP) {
                        RAM[Address] = byte_in;
                    }
                    // If address IS in ROM space the write is discarded but
                    // the bus cycle still completes normally (CLKs still run).
                    Insert_Wait_States(ws);    // [LAB]
                    CLK(); CLK();
                    // [LAB] log + stats
                    g_stats.mem_writes++;
                    BusLog_Push(CYC_MEM_WRITE, (unsigned)Address, byte_in, ws);
                    break;
                }

                // ----------------------------------------------------------
                // I/O Read
                // ----------------------------------------------------------
                case 0x06: {
                    if (Address >= PORT_STAT_MREAD_LO && Address <= PORT_STAT_IOWRITE_HI)
                        Refresh_Stats_IO();   // [LAB] update shadow regs before read
                    unsigned char byte_out = IO[Address];
                    Data_Bus_Direction_8088_OUT();
                    Write_To_Data_Port_0_7(byte_out);
                    Insert_Wait_States(ws);    // [LAB]
                    CLK(); CLK();
                    Data_Bus_Direction_8088_IN();
                    // [LAB] log + stats
                    g_stats.io_reads++;
                    BusLog_Push(CYC_IO_READ, (unsigned)Address, byte_out, ws);
                    break;
                }

                // ----------------------------------------------------------
                // I/O Write
                // ----------------------------------------------------------
                case 0x07: {
                    unsigned char byte_in = Read_From_Data_Port_0_7();
                    bool handled = Handle_Debug_IO_Write((unsigned)Address, byte_in); // [LAB]
                    if (!handled)
                        IO[Address] = byte_in;
                    Insert_Wait_States(ws);    // [LAB]
                    CLK(); CLK();
                    // [LAB] log + stats
                    g_stats.io_writes++;
                    BusLog_Push(CYC_IO_WRITE, (unsigned)Address, byte_in, ws);
                    break;
                }

                // ----------------------------------------------------------
                // Interrupt Acknowledge
                // ----------------------------------------------------------
                case 0x02:
                    // Wait for the second INTA bus cycle (4 CLKs on 8088)
                    CLK(); CLK(); CLK(); CLK();
                    switch (Read_Interrupts()) {
                        case 0x01:   // IRQ0 — system timer
                            Data_Bus_Direction_8088_OUT();
                            Write_To_Data_Port_0_7(0x08);
                            CLK(); CLK();
                            Data_Bus_Direction_8088_IN();
                            IRQ0_Flag = false;
                            digitalWrite(PIN_INTR, LOW);
                            break;
                        case 0x02:   // IRQ1 — keyboard
                            Data_Bus_Direction_8088_OUT();
                            Write_To_Data_Port_0_7(0x09);
                            CLK(); CLK();
                            Data_Bus_Direction_8088_IN();
                            IRQ1_Flag = false;
                            digitalWrite(PIN_INTR, LOW);
                            break;
                        case 0x03:   // Both — handle timer first
                            Data_Bus_Direction_8088_OUT();
                            Write_To_Data_Port_0_7(0x08);
                            CLK(); CLK();
                            Data_Bus_Direction_8088_IN();
                            IRQ0_Flag = false;
                            break;
                        default:
                            printf("[intr] Unhandled interrupt flags 0x%02X\n",
                                   (unsigned)Read_Interrupts());
                            break;
                    }
                    g_stats.intr_acks++;
                    BusLog_Push(CYC_INTR_ACK, 0, 0, 0);
                    break;

                default:
                    printf("[bus] Unexpected control bus value 0x%02X\n", (unsigned)ctrl);
                    break;
            }
        }
    }
}

// ============================================================================
// Main System Bus Loop  --  8086 (V30) variant
// ============================================================================
static void Start_System_Bus_8086()
{
    int  Address;
    char Memory_IO_Bank;
    unsigned char ws = 0;

    while (Stop_Flag != true) {
        CLK();

        if (digitalRead(PIN_ALE) == 1) {
            Address        = Read_Address();
            Memory_IO_Bank = Read_Memory_Bank();
            CLK();
            ws = g_wait_states;   // [LAB]
            g_cycle_count++;      // [LAB]
            Toggle_Bus_Activity();// [LAB]

            switch (Read_Control_Bus() + (Memory_IO_Bank << 4)) {

                // ------ Memory Write (16-bit, both bytes) ------------------
                case 0x07: {
                    unsigned char lo = Read_From_Data_Port_0_7();
                    unsigned char hi = Read_From_Data_Port_8_15();
                    if ((unsigned)Address < ROM_BASE) {
                        RAM[Address]   = lo;
                        RAM[Address+1] = hi;
                    }
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.mem_writes++;
                    BusLog_Push(CYC_MEM_WRITE, (unsigned)Address, lo, ws);
                    break;
                }
                // ------ Memory Write (high byte only) ----------------------
                case 0x17: {
                    unsigned char hi = Read_From_Data_Port_8_15();
                    if ((unsigned)Address < ROM_BASE)
                        RAM[Address] = hi;
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.mem_writes++;
                    BusLog_Push(CYC_MEM_WRITE, (unsigned)Address, hi, ws);
                    break;
                }
                // ------ Memory Write (low byte only) -----------------------
                case 0x27: {
                    unsigned char lo = Read_From_Data_Port_0_7();
                    if ((unsigned)Address < ROM_BASE)
                        RAM[Address] = lo;
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.mem_writes++;
                    BusLog_Push(CYC_MEM_WRITE, (unsigned)Address, lo, ws);
                    break;
                }
                // ------ Memory Read (16-bit, both bytes) -------------------
                case 0x06:
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_0_7(RAM[Address]);
                    Write_To_Data_Port_8_15(RAM[Address+1]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.mem_reads++;
                    BusLog_Push(CYC_MEM_READ, (unsigned)Address, RAM[Address], ws);
                    break;
                // ------ Memory Read (high byte only) -----------------------
                case 0x16:
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_8_15(RAM[Address]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.mem_reads++;
                    BusLog_Push(CYC_MEM_READ, (unsigned)Address, RAM[Address], ws);
                    break;
                // ------ Memory Read (low byte only) ------------------------
                case 0x26:
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_0_7(RAM[Address]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.mem_reads++;
                    BusLog_Push(CYC_MEM_READ, (unsigned)Address, RAM[Address], ws);
                    break;

                // ------ I/O Write (16-bit) ---------------------------------
                case 0x05: {
                    unsigned char lo = Read_From_Data_Port_0_7();
                    unsigned char hi = Read_From_Data_Port_8_15();
                    if (!Handle_Debug_IO_Write((unsigned)Address, lo))
                        IO[Address] = lo;
                    IO[Address+1] = hi;   // high byte never maps to a debug port
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.io_writes++;
                    BusLog_Push(CYC_IO_WRITE, (unsigned)Address, lo, ws);
                    break;
                }
                case 0x15: {
                    unsigned char hi = Read_From_Data_Port_8_15();
                    IO[Address] = hi;
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.io_writes++;
                    BusLog_Push(CYC_IO_WRITE, (unsigned)Address, hi, ws);
                    break;
                }
                case 0x25: {
                    unsigned char lo = Read_From_Data_Port_0_7();
                    if (!Handle_Debug_IO_Write((unsigned)Address, lo))
                        IO[Address] = lo;
                    Insert_Wait_States(ws); CLK(); CLK();
                    g_stats.io_writes++;
                    BusLog_Push(CYC_IO_WRITE, (unsigned)Address, lo, ws);
                    break;
                }
                // ------ I/O Read (16-bit) ----------------------------------
                case 0x04:
                    if (Address >= PORT_STAT_MREAD_LO && Address <= PORT_STAT_IOWRITE_HI)
                        Refresh_Stats_IO();
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_0_7(IO[Address]);
                    Write_To_Data_Port_8_15(IO[Address+1]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.io_reads++;
                    BusLog_Push(CYC_IO_READ, (unsigned)Address, IO[Address], ws);
                    break;
                case 0x14:
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_8_15(IO[Address]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.io_reads++;
                    BusLog_Push(CYC_IO_READ, (unsigned)Address, IO[Address], ws);
                    break;
                case 0x24:
                    if (Address >= PORT_STAT_MREAD_LO && Address <= PORT_STAT_IOWRITE_HI)
                        Refresh_Stats_IO();
                    Data_Bus_Direction_8086_OUT();
                    Write_To_Data_Port_0_7(IO[Address]);
                    Insert_Wait_States(ws); CLK(); CLK();
                    Data_Bus_Direction_8086_IN();
                    g_stats.io_reads++;
                    BusLog_Push(CYC_IO_READ, (unsigned)Address, IO[Address], ws);
                    break;

                // ------ Interrupt Acknowledge (7 CLKs on 8086) -------------
                case 0x00:
                    CLK(); CLK(); CLK(); CLK(); CLK(); CLK(); CLK();
                    switch (Read_Interrupts()) {
                        case 0x01:
                            Data_Bus_Direction_8086_OUT();
                            Write_To_Data_Port_0_7(0x08);
                            CLK(); CLK();
                            Data_Bus_Direction_8086_IN();
                            IRQ0_Flag = false;
                            digitalWrite(PIN_INTR, LOW);
                            break;
                        case 0x02:
                            Data_Bus_Direction_8086_OUT();
                            Write_To_Data_Port_0_7(0x09);
                            CLK(); CLK();
                            Data_Bus_Direction_8086_IN();
                            IRQ1_Flag = false;
                            digitalWrite(PIN_INTR, LOW);
                            break;
                        case 0x03:
                            Data_Bus_Direction_8086_OUT();
                            Write_To_Data_Port_0_7(0x08);
                            CLK(); CLK();
                            Data_Bus_Direction_8086_IN();
                            IRQ0_Flag = false;
                            break;
                        default:
                            printf("[intr] Unhandled flags 0x%02X\n",
                                   (unsigned)Read_Interrupts());
                            break;
                    }
                    g_stats.intr_acks++;
                    BusLog_Push(CYC_INTR_ACK, 0, 0, 0);
                    break;

                default:
                    printf("[bus] Unexpected control 0x%02X\n",
                           (unsigned)(Read_Control_Bus() + (Memory_IO_Bank << 4)));
                    break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Dispatcher — picks 8088 or 8086 loop
// ---------------------------------------------------------------------------
static void Start_System_Bus(int Processor)
{
    if (Processor == 88)
        Start_System_Bus_8088();
    else
        Start_System_Bus_8086();
}

// ---------------------------------------------------------------------------
// GPIO setup
// ---------------------------------------------------------------------------
void Setup()
{
    Stop_Flag = false;
    wiringPiSetup();

    // Control / clock pins
    pinMode(PIN_CLK,   OUTPUT);
    pinMode(PIN_RESET, OUTPUT);
    pinMode(PIN_ALE,   INPUT);
    pinMode(PIN_IO_M,  INPUT);
    pinMode(PIN_DTR,   INPUT);
    pinMode(PIN_BHE,   INPUT);

    // Interrupt pins
    pinMode(PIN_INTR, OUTPUT);
    pinMode(PIN_INTA,  INPUT);
    digitalWrite(PIN_INTR, LOW);

    // [LAB] extra output pins for logic-analyzer signals
    pinMode(PIN_DEBUG_TRIGGER, OUTPUT);
    pinMode(PIN_BUS_ACTIVITY,  OUTPUT);
    digitalWrite(PIN_DEBUG_TRIGGER, LOW);
    digitalWrite(PIN_BUS_ACTIVITY,  LOW);

    // Multiplexed address/data bus — start as inputs
    for (int p : {AD0,AD1,AD2,AD3,AD4,AD5,AD6,AD7,
                  A8,A9,A10,A11,A12,A13,A14,A15,
                  A16,A17,A18,A19})
        pinMode(p, INPUT);
}

// ---------------------------------------------------------------------------
// Reset sequence
// ---------------------------------------------------------------------------
void Reset()
{
    digitalWrite(PIN_RESET, HIGH);
    CLK(); CLK(); CLK(); CLK();
    CLK(); CLK(); CLK(); CLK();
    digitalWrite(PIN_RESET, LOW);
}

// ---------------------------------------------------------------------------
// Start — sets up GPIO, resets processor, starts bus thread; logger is opt-in (PI86_LOG=1)
// ---------------------------------------------------------------------------
void Start(int Processor)
{
    Setup();
    BusLog_Init();     // [LAB] start CSV logger if PI86_LOG=1
    Reset();
    thread System_Bus(Start_System_Bus, Processor);
    System_Bus.detach();
}

// ---------------------------------------------------------------------------
// Load_Bios
// ---------------------------------------------------------------------------
void Load_Bios(string Bios_file)
{
    std::ifstream MemoryFile;
    MemoryFile.open(Bios_file);
    MemoryFile.seekg(0, ios::end);
    int FileSize = MemoryFile.tellg();
    MemoryFile.seekg(0, MemoryFile.beg);
    char Rom[FileSize];
    MemoryFile.read(Rom, FileSize);
    MemoryFile.close();

    // Jump vector at the reset entry point: JMP FAR 0xF000:0x0100
    char FFFF0[] = {0xEA, 0x00, 0x01, 0x00, 0xF8,
                    'E','M',' ','0','4','/','1','0','/','2','0'};
    Write_Memory_Array(0xFFFF0, FFFF0, sizeof(FFFF0));
    Write_Memory_Array(0xF8000, Rom,   sizeof(Rom));

    // BIOS I/O init values (unchanged from upstream)
    Write_IO_Byte(0xF0FF, 0xFF);
    Write_IO_Byte(0xF000, 0xFF);
    Write_IO_Byte(0xF0F0, 0x03);   // Video mode
    Write_IO_Byte(0x3DA,  0xFF);

    // [LAB] Initialize debug port shadow values
    Write_IO_Byte(PORT_DEBUG_CHECKPOINT, 0x00);
    Write_IO_Byte(PORT_WAIT_STATE_CTRL,  0x00);
    Refresh_Stats_IO();

    printf("[bios] Loaded %d bytes from %s\n", FileSize, Bios_file.c_str());
    printf("[bios] Bus trace file: %s (enable with PI86_LOG=1)\n", BUS_LOG_FILENAME);
}

// ============================================================================
// Memory / I/O helper implementations
// ============================================================================

void Write_Memory_Array(unsigned long long Address, char data[], int Length)
{
    for (int i = 0; i < Length; i++)
        RAM[Address++] = data[i];
}

void Read_Memory_Array(unsigned long long Address, char *buf, int Length)
{
    for (int i = 0; i < Length; i++)
        buf[i] = RAM[Address++];
}

void Write_Memory_Byte(unsigned long long Address, char byte)
{
    RAM[Address] = byte;
}

char Read_Memory_Byte(unsigned long long Address)
{
    return RAM[Address];
}

void Write_Memory_Word(unsigned long long Address, unsigned short word)
{
    RAM[Address]     = (char)(word & 0xFF);
    RAM[Address + 1] = (char)(word >> 8);
}

void Write_IO_Array(unsigned long long Address, char data[], int Length)
{
    for (int i = 0; i < Length; i++)
        IO[Address++] = data[i];
}

void Read_IO_Array(unsigned long long Address, char *buf, int Length)
{
    for (int i = 0; i < Length; i++)
        buf[i] = IO[Address++];
}

void Write_IO_Byte(unsigned long long Address, char byte)
{
    IO[Address] = byte;
}

char Read_IO_Byte(unsigned long long Address)
{
    return IO[Address];
}

void Write_IO_Word(unsigned long long Address, unsigned short word)
{
    IO[Address]     = (char)(word & 0xFF);
    IO[Address + 1] = (char)(word >> 8);
}
