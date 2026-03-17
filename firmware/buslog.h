#ifndef BUSLOG_H
#define BUSLOG_H

// ============================================================================
//  pi86 Bus Cycle Logger  --  buslog.h
//
//  Provides a lock-free circular ring-buffer that records every bus
//  transaction observed by the Pi.  A background flush thread writes
//  completed entries to  bus_trace.csv  in CSV format.
//
//  CSV columns:
//    cycle, type, address, data, wait_states
//
//  The file can be opened directly in a spreadsheet or imported into
//  logic-analyzer software for side-by-side comparison with captured
//  waveforms.
// ============================================================================

#include "x86.h"

// ---------------------------------------------------------------------------
// Initialize the logger.  Must be called once before Start().
//
// Logging is OFF by default.  Set the environment variable PI86_LOG=1
// before launching pi86 to enable it:
//
//   PI86_LOG=1 sudo -E ./pi86
//   PI86_LOG=1 ./run_pi86.sh
//
// When enabled: opens/truncates bus_trace.csv, writes the CSV header,
// and spawns the background flush thread.
// When disabled: prints a one-line notice and returns immediately (no file
// is created, no thread is started, BusLog_Push is a no-op).
// ---------------------------------------------------------------------------
void BusLog_Init();

// ---------------------------------------------------------------------------
// Push a single bus cycle record into the ring buffer.
// Called from the hot bus loop in x86.cpp — must be very fast.
// Non-blocking: if the buffer is full the oldest entry is silently overwritten.
// ---------------------------------------------------------------------------
void BusLog_Push(BusCycleType type,
                 unsigned int address,
                 unsigned char data,
                 unsigned char wait_states);

// ---------------------------------------------------------------------------
// Flush all buffered entries to disk immediately (called from flush thread).
// Safe to call manually from main() before program exit.
// ---------------------------------------------------------------------------
void BusLog_Flush();

// ---------------------------------------------------------------------------
// Clear the ring buffer without writing to disk.
// Call when you want a "clean" capture for a new experiment.
// ---------------------------------------------------------------------------
void BusLog_Reset();

// ---------------------------------------------------------------------------
// Signal the flush thread to stop and wait for it to finish.
// Performs a final flush before returning.
// ---------------------------------------------------------------------------
void BusLog_Shutdown();

#endif // BUSLOG_H
