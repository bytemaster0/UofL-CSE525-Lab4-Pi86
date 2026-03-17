// ============================================================================
//  pi86 Bus Cycle Logger  --  buslog.cpp
//  GPL-3.0  (same license as upstream pi86 project)
// ============================================================================

#include "buslog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <thread>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Ring buffer
// ---------------------------------------------------------------------------
// Two indices: write_idx (producer, hot bus thread) and
//              read_idx  (consumer, background flush thread).
// Both are atomic; no mutex needed for the common case.
// ---------------------------------------------------------------------------
static BusLogEntry  s_ring[BUS_LOG_CAPACITY];
static atomic<unsigned int>  s_write_idx{0};
static atomic<unsigned int>  s_read_idx{0};
static atomic<bool>          s_running{false};

// File handle shared between Init / Flush / Shutdown
static FILE *s_fp = nullptr;

// Set to true only when Init succeeds and logging was requested via PI86_LOG=1.
// Checked in BusLog_Push to avoid ring-buffer churn when logging is disabled.
static bool s_enabled = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const char *cycle_type_str(BusCycleType t)
{
    switch (t) {
        case CYC_MEM_READ:  return "MEM_RD";
        case CYC_MEM_WRITE: return "MEM_WR";
        case CYC_IO_READ:   return "IO_RD";
        case CYC_IO_WRITE:  return "IO_WR";
        case CYC_INTR_ACK:  return "INTR_ACK";
        default:            return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// BusLog_Push  --  called from the hot bus loop
// This function is designed to be as fast as possible.
// ---------------------------------------------------------------------------
void BusLog_Push(BusCycleType type,
                 unsigned int  address,
                 unsigned char data,
                 unsigned char wait_states)
{
    if (!s_enabled) return;
    unsigned int idx = s_write_idx.fetch_add(1, memory_order_relaxed)
                       % BUS_LOG_CAPACITY;
    BusLogEntry &e = s_ring[idx];
    e.cycle_number = g_cycle_count;
    e.type         = type;
    e.address      = address;
    e.data         = data;
    e.wait_states  = wait_states;
    // If the ring is full the consumer will simply see stale-then-fresh data;
    // the oldest entry is silently overwritten (acceptable for a lab tool).
}

// ---------------------------------------------------------------------------
// BusLog_Flush  --  called by the background thread
// ---------------------------------------------------------------------------
void BusLog_Flush()
{
    if (!s_fp) return;

    unsigned int w = s_write_idx.load(memory_order_acquire);
    unsigned int r = s_read_idx.load(memory_order_relaxed);

    // How many new entries are waiting?
    unsigned int count = (w - r);   // wraps correctly with unsigned arithmetic
    if (count > BUS_LOG_CAPACITY)
        count = BUS_LOG_CAPACITY;   // clamped if producer ran ahead

    for (unsigned int i = 0; i < count; ++i) {
        unsigned int idx = (r + i) % BUS_LOG_CAPACITY;
        const BusLogEntry &e = s_ring[idx];
        fprintf(s_fp, "%llu,%s,0x%05X,0x%02X,%u\n",
                (unsigned long long)e.cycle_number,
                cycle_type_str(e.type),
                e.address,
                (unsigned)e.data,
                (unsigned)e.wait_states);
    }
    fflush(s_fp);
    s_read_idx.store(w, memory_order_release);
}

// ---------------------------------------------------------------------------
// Background flush thread  --  flushes every 100 ms
// ---------------------------------------------------------------------------
static void flush_thread_fn()
{
    while (s_running.load(memory_order_relaxed)) {
        usleep(100000);   // 100 ms
        BusLog_Flush();
    }
    // Final drain
    BusLog_Flush();
}

static thread s_flush_thread;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void BusLog_Init()
{
    if (!getenv("PI86_LOG")) {
        printf("[buslog] Bus logging disabled. "
               "Run with PI86_LOG=1 to enable (see GETTING_STARTED.md).\n");
        return;
    }

    s_fp = fopen(BUS_LOG_FILENAME, "w");
    if (!s_fp) {
        fprintf(stderr, "[buslog] WARNING: could not open %s for writing.\n",
                BUS_LOG_FILENAME);
        return;
    }
    // Write CSV header
    fprintf(s_fp, "cycle,type,address,data,wait_states\n");
    fflush(s_fp);

    s_write_idx.store(0, memory_order_relaxed);
    s_read_idx.store(0,  memory_order_relaxed);
    s_running.store(true, memory_order_relaxed);
    s_enabled = true;

    s_flush_thread = thread(flush_thread_fn);
    s_flush_thread.detach();

    atexit(BusLog_Shutdown);

    printf("[buslog] Logging bus cycles to %s\n", BUS_LOG_FILENAME);
}

void BusLog_Reset()
{
    // Snap the read pointer to the write pointer, discarding buffered entries.
    unsigned int w = s_write_idx.load(memory_order_acquire);
    s_read_idx.store(w, memory_order_release);
    printf("[buslog] Ring buffer cleared.\n");
}

void BusLog_Shutdown()
{
    if (!s_enabled) return;   // not initialized or already shut down
    s_enabled = false;         // stop BusLog_Push from adding new entries
    s_running.store(false, memory_order_relaxed);
    usleep(200000);   // Give flush thread time to exit (it sleeps 100 ms)
    BusLog_Flush();
    if (s_fp) {
        fclose(s_fp);
        s_fp = nullptr;
    }
    printf("[buslog] Shutdown complete. Trace saved to %s\n", BUS_LOG_FILENAME);
}
