#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"
#include "Devices/Printer/PrinterHead.h"

class PrinterByteRing;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterEngine
//
//  The printer's drain + pacing + preview pipeline, with no thread of its own.
//  It owns the PrinterJob (interpreter + raster over the card's ring), the pure
//  PrinterHead mechanical model, and the "wet ink" presented layer, and it
//  publishes the head's state for the UI thread through lock-free atomics.
//
//  One Tick(nowMs) is a single print step: play any host Form Feed, run the
//  interpreter just far enough ahead to keep the line buffer full (backpressure),
//  advance the head by the elapsed real time (capped by the guest cycle delta),
//  and publish the platen / reveal frontier / carriage. The wall clock is passed
//  IN, so a unit test drives the whole pipeline with a synthetic time and a pre-
//  loaded ring -- no thread, no real clock. The exe's PrinterWorker owns only the
//  thread that reads the clock and calls Tick (Constitution VI: the platform edge
//  is the thread; the pipeline is testable core).
//
//  Threading: Tick and the Snapshot* / SpanInkExtent readers all take the raster
//  mutex, so the owning thread may Tick while another thread snapshots for the
//  preview. The caller must stop that thread before Job()/FlushNow() touch the
//  raster for eject or persistence.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterEngine
{
public:
    // Builds a fresh job over `ring`; `seed` restores a persisted pending strip
    // (its feed position and all) before any stepping, default-empty starts a
    // fresh sheet. Resets the head, seeds the presented layer + published state.
    void          Start (PrinterByteRing & ring, PrintRaster seed = PrintRaster ());

    // Wire the guest's monotonic cycle accumulator so pacing caps the print rate
    // to emulated time (see the class comment); null paces off the wall clock
    // alone (tests / no CPU). Read best-effort across threads -- it only caps.
    void          SetCycleClock (const uint64_t * cycles) { m_guestCycles = cycles; }

    // One print step at wall-clock time `nowMs` (steady-clock milliseconds). The
    // first Tick seeds the pacing clocks; a Tick with no elapsed time still
    // republishes state. Takes the raster mutex.
    void          Tick (int64_t nowMs);

    // The job, valid from Start() onward. Touch the raster only while no thread
    // is Ticking.
    PrinterJob *  Job () { return m_job.get (); }

    // Synchronous drain of any bytes still in the ring; call only while no thread
    // is Ticking.
    size_t        FlushNow (vector<PrinterEvent> & events);

    // Consistent copies for the live preview, taken under the raster mutex while
    // a thread keeps Ticking. Strip = the fully-composited built raster; Presented
    // = the wet-ink layer the head has physically laid so far. The span variants
    // copy only rows [firstRow, lastRow] (rebased to 0 in `out`) so the per-
    // refresh cost is bounded by the viewport, not the strip. Return false when
    // no job is active.
    bool          SnapshotStrip        (PrintRaster & out);
    bool          SnapshotStripSpan    (int firstRow, int lastRow, PrintRaster & out);
    bool          SnapshotPresentedSpan (int firstRow, int lastRow, PrintRaster & out);

    // One past the rightmost inked dot over rows [firstRow, lastRow] of the built
    // strip (0 == blank), under the raster mutex. Drives the preview's audio ink
    // gate for the live pin band.
    int           SpanInkExtent (int firstRow, int lastRow);

    // Host form feed (the preview's Form Feed button): records the request; the
    // next Tick performs the feed on the Ticking thread so the interpreter, raster
    // and timeline stay single-writer. Safe to call from another thread.
    void          FormFeed ();

    // Published state (lock-free reads for the UI thread)

    uint64_t      ActivityCount () const { return m_activity.load    (std::memory_order_relaxed); }
    bool          HasContent    () const { return m_hasContent.load (std::memory_order_relaxed); }
    int           RowsUsed      () const { return m_rowsUsed.load   (std::memory_order_relaxed); }

    void          HeadPosition  (int & row, int & colDots) const
    {
        uint64_t   packed = m_headPos.load (std::memory_order_relaxed);

        row     = (int) (packed >> 32);
        colDots = (int) (packed & 0xFFFFFFFFu);
    }

    bool          HeadSweepLtr  () const { return m_headLtr.load      (std::memory_order_relaxed) != 0; }
    int           RevealBandTop () const { return m_revealTop.load    (std::memory_order_relaxed); }
    bool          HeadMoving    () const { return m_headMoving.load   (std::memory_order_relaxed) != 0; }
    int           CarriageCol   () const { return m_carriageCol.load  (std::memory_order_relaxed); }

private:
    // Non-locking rightmost-ink probe over [firstRow, lastRow]; shared by
    // SpanInkExtent (which holds the lock) and any in-Tick caller.
    static int    RasterInkExtent (const PrintRaster & raster, int firstRow, int lastRow);

    unique_ptr<PrinterJob>   m_job;
    PrinterHead              m_head;          // pure mechanical carriage + feed model (tested)
    PrintRaster              m_presented;     // "wet ink" layer the head paints (preview only)
    std::mutex               m_rasterMutex;   // guards raster mutation vs. snapshot readers

    std::atomic<uint64_t>    m_activity      { 0 };
    std::atomic<bool>        m_hasContent    { false };
    std::atomic<int>         m_rowsUsed      { 0 };   // raster's real built height (preview clamp / tear)
    std::atomic<uint64_t>    m_headPos       { 0 };   // platen: (row << 32) | maskColDots
    std::atomic<int>         m_headLtr       { 1 };   // published carriage sweep direction (1 = L>R)
    std::atomic<int>         m_revealTop     { 0 };   // reveal-mask top row (print frontier)
    std::atomic<int>         m_carriageCol   { 0 };   // physical carriage column for the glyph (holds in feeds)
    std::atomic<int>         m_headMoving    { 0 };   // carriage sweeping or paper feeding
    std::atomic<int>         m_hostFormFeeds { 0 };   // host Form Feed requests, played by Tick

    // Wall-clock + cycle pacing (see SetCycleClock / the class comment).
    const uint64_t *         m_guestCycles   = nullptr;   // CPU cycle accumulator (caps the print rate)
    uint64_t                 m_lastCycles    = 0;         // cycle count at the last head advance
    int64_t                  m_lastTickMs    = 0;         // wall-clock ms at the last head advance
    bool                     m_pacingSeeded  = false;     // seed the clocks on the first tick
};
