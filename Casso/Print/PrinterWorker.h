#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"

class PrinterByteRing;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterWorker
//
//  The exe-side platform edge for the printer drain: a background thread that
//  paces PrinterJob::Drain so the card's ring never overflows and the strip
//  builds up as the guest prints. All real work lives in the core PrinterJob;
//  this owns only the thread and its lifecycle (irreducible platform code per
//  Constitution VI).
//
//  Threading: the drain thread is the sole writer of the raster while running.
//  The UI thread must call Stop() (which joins) before it touches the raster
//  for eject/persistence; FlushNow() then drains any final bytes single-
//  threaded.
//
//  A temporary raw-byte capture tee (to %TEMP%\casso-printer-capture.bin) is
//  wired here to grab a real Print Shop stream for the T011 checkpoint; remove
//  it once the interpreter's provisional command bytes are pinned.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterWorker
{
public:
    ~PrinterWorker ();

    // `seed` restores a persisted pending strip into the job before the drain
    // thread starts (no race); default-empty seed starts a fresh sheet.
    void          Start    (PrinterByteRing & ring, PrintRaster seed = PrintRaster ());
    void          Stop     ();
    bool          Running  () const { return m_running; }

    // Valid from Start() until the next Start()/dtor. Touch the raster only
    // after Stop().
    PrinterJob *  Job      () { return m_job.get (); }

    // Synchronous drain of any bytes still in the ring; call only after Stop().
    size_t        FlushNow (vector<PrinterEvent> & events);

    // Copy the current strip raster for the live preview WITHOUT stopping the
    // drain thread. The drain loop mutates the raster only while holding
    // m_rasterMutex, so this yields a consistent copy while the same interpreter
    // keeps running -- reading the preview can never disturb the job's state.
    // Returns false when no job is active. Safe to call from the UI thread.
    bool          SnapshotStrip (PrintRaster & out);

    // Same contract, but copies only rows [firstRow, lastRow] (rebased to 0 in
    // `out`). The live preview snapshots its ~1-page viewport through this so
    // per-refresh cost is bounded by the viewport, not the strip (FR-033).
    bool          SnapshotStripSpan (int firstRow, int lastRow, PrintRaster & out);

    // Thread-safe status signals for the chrome indicator / panel: a monotonic
    // counter that advances as guest bytes are drained (sample the delta to see
    // "receiving"), and whether the strip currently holds printed content. Safe
    // to read from the UI thread while the drain thread runs.
    uint64_t      ActivityCount () const { return m_activity.load    (std::memory_order_relaxed); }
    bool          HasContent    () const { return m_hasContent.load (std::memory_order_relaxed); }

    // Current strip height in native rows, published cheaply (no raster copy) so
    // the live preview can scale its refresh rate to the strip -- re-rendering
    // the whole strip is O(rows), so a tall banner must refresh less often to
    // avoid O(rows^2) work. Safe to read from the UI thread.
    int           RowsUsed      () const { return m_rowsUsed.load   (std::memory_order_relaxed); }

    // The print head's newest position (paper row + dot column), published
    // atomically as one word after each drain so the pair is always coherent.
    // Drives the panel's left-to-right ink reveal (FR-034); reading it never
    // touches the raster. Safe to call from the UI thread.
    void          HeadPosition  (int & row, int & colDots) const
    {
        uint64_t   packed = m_headPos.load (std::memory_order_relaxed);

        row     = (int) (packed >> 32);
        colDots = (int) (packed & 0xFFFFFFFFu);
    }

private:
    void          Run             ();
    void          OpenCaptureFile ();

    unique_ptr<PrinterJob>   m_job;
    std::thread              m_thread;
    std::mutex               m_rasterMutex;   // guards raster mutation vs. UI-thread SnapshotStrip
    std::atomic<bool>        m_stopRequested { false };
    std::atomic<uint64_t>    m_activity      { 0 };
    std::atomic<bool>        m_hasContent    { false };
    std::atomic<int>         m_rowsUsed      { 0 };   // strip height for preview pacing
    std::atomic<uint64_t>    m_headPos       { 0 };   // (row << 32) | colDots (FR-034)
    bool                     m_running       = false;
    std::ofstream            m_capture;
};
