#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterJob.h"
#include "Devices/Printer/PrinterHead.h"

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

    // Wire the guest's monotonic cycle accumulator so the drain runs at real
    // ImageWriter speed in EMULATED time: the ring backs up, the card de-asserts
    // ready, and the guest throttles itself (backpressure) -- exactly like the
    // real parallel interface, so the strip is produced no faster than it
    // "prints" and can never race ahead of the preview. Running the emulator
    // faster (max perf) prints proportionally faster. Set once at machine build
    // (the pointer is stable across worker restarts); null drains unthrottled
    // (tests / no CPU). Read best-effort across threads -- it only paces.
    void          SetCycleClock (const uint64_t * cycles) { m_guestCycles = cycles; }

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

    // Viewport-bounded snapshot of the PRESENTED ("wet ink") layer -- the strip
    // as the mechanical head has physically laid it so far, one primary per pass,
    // rather than the fully-composited strip the drain builds ahead. This is what
    // the live preview renders, so an overprint (Print Shop lays red then re-
    // strikes blue) shows the colour mixing to purple as the passes sweep, and
    // freshly-fed paper reads blank until the head reaches it. Same locking /
    // rebasing contract as SnapshotStripSpan; delivery still uses the real strip.
    bool          SnapshotPresentedSpan (int firstRow, int lastRow, PrintRaster & out);

    // The ink extent of rows [firstRow, lastRow]: one past the rightmost inked
    // dot, or 0 when the span is blank. Read under the raster lock. The
    // panel's pacing asks this about the live pin band for the real machine's
    // behaviors: blank (0) slews through with the head parked, and an inked
    // band's carriage pass sweeps only its printed span (logic seeking), not
    // the whole platen.
    int           SpanInkExtent (int firstRow, int lastRow);

    // Host form feed (the preview's Form Feed button): advances the paper to
    // the top of the next page exactly as if the guest had sent $0C, under
    // the raster lock. The shell gates this on printer idleness so it never
    // interleaves with an in-flight print. Safe to call from the UI thread.
    void          FormFeed ();

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

    // The carriage sweep direction of the live line (bidirectional print): the
    // ImageWriter II lays one band left-to-right, feeds, then the next band
    // right-to-left. The reveal mirrors accordingly. Safe from the UI thread.
    bool          HeadSweepLtr  () const { return m_headLtr.load (std::memory_order_relaxed) != 0; }

    // The top strip row of the reveal mask: ink at or below it shows only within
    // the swept column span, so it is the print FRONTIER. During a sweep it is
    // the pass being laid; during a paper feed it holds at the last finished pass
    // while the platen (HeadPosition row) slews on, so freshly fed paper reads as
    // blank even though the raster already holds the next line (drained ahead to
    // keep the buffer full). Safe from the UI thread.
    int           RevealBandTop () const { return m_revealTop.load (std::memory_order_relaxed); }

    // True while the carriage is mid-pass or the paper is feeding (including a
    // host Form Feed, which does not bump the guest-activity signal). The panel
    // keeps requesting animation frames while this holds. Safe from the UI thread.
    bool          HeadMoving    () const { return m_headMoving.load (std::memory_order_relaxed) != 0; }

    // The physical carriage column in dots (0 = left margin), for the head glyph.
    // Unlike the reveal mask -- which closes to 0 during a feed to blank the paper
    // scrolling in below the frontier -- this HOLDS at the last sweep's end so the
    // carriage parks between passes instead of snapping to the left edge. Safe
    // from the UI thread.
    int           CarriageCol   () const { return m_carriageCol.load (std::memory_order_relaxed); }

private:
    void          Run ();

    unique_ptr<PrinterJob>   m_job;
    PrinterHead              m_head;          // pure mechanical carriage + feed model (core, tested)
    PrintRaster              m_presented;     // "wet ink" layer the head paints (preview only)
    std::thread              m_thread;
    std::mutex               m_rasterMutex;   // guards raster mutation vs. UI-thread SnapshotStrip
    std::atomic<bool>        m_stopRequested { false };
    std::atomic<uint64_t>    m_activity      { 0 };
    std::atomic<bool>        m_hasContent    { false };
    std::atomic<int>         m_rowsUsed      { 0 };   // raster's real built height (preview clamp / tear)
    std::atomic<uint64_t>    m_headPos       { 0 };   // platen: (row << 32) | maskColDots (FR-034)
    std::atomic<int>         m_headLtr       { 1 };   // published carriage sweep direction (1 = L>R)
    std::atomic<int>         m_revealTop     { 0 };   // reveal-mask top row (print frontier)
    std::atomic<int>         m_carriageCol   { 0 };   // physical carriage column for the glyph (holds in feeds)
    std::atomic<int>         m_headMoving    { 0 };   // carriage sweeping or paper feeding
    std::atomic<int>         m_hostFormFeeds { 0 };   // UI-thread Form Feed requests, played by Run
    bool                     m_running       = false;

    // Cycle-paced, event-driven mechanical head (see SetCycleClock). The
    // interpreter emits the real carriage timeline -- a HeadBurst per printed
    // pass (its row + struck extent) and a LineFeed / FormFeed per paper advance.
    // The head (m_head) replays that timeline off the guest clock at the draft
    // carriage / feed speed and the ring is drained only just ahead of it, so the
    // ring backs up, the card de-asserts ready, and the guest throttles itself to
    // the print rate. Running the emulator faster (max perf) replays faster.
    const uint64_t *         m_guestCycles   = nullptr;   // CPU cycle accumulator (caps the print rate)
    uint64_t                 m_lastCycles    = 0;         // cycle count at the last head advance
    int64_t                  m_lastTickMs    = 0;         // wall-clock ms at the last head advance
    bool                     m_pacingSeeded  = false;     // seed the clocks on the first tick
};
