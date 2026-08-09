#include "Pch.h"

#include "Devices/Printer/PrinterEngine.h"

#include "Devices/Printer/PrinterByteRing.h"




// The head moves at real draft speed off the WALL clock, so the carriage looks
// right no matter how fast the emulator runs (under backpressure the guest spins
// its ready-wait loop, burning emulated cycles far faster than real time -- a
// cycle-paced head would then complete whole passes between preview frames and
// read as no motion at all). The guest cycle delta only CAPS it: a paused CPU
// freezes the printer, a slowed one prints slower, but neither prints faster than
// the real machine's fixed carriage speed.
static constexpr double   s_kGuestCyclesPerSec = 1020484.0;   // Apple II NTSC clock (the rate cap)

// Cap one tick's advance so a scheduler stall (the worker starved for 100s of ms)
// snaps the head forward a little, not across the whole buffered page.
static constexpr double   s_kMaxTickSec = 0.1;

// The interpreter is run only far enough ahead to keep this many seconds of
// print time queued -- the real printer's small line buffer. Past it the drain
// stops, the ring backs up, and the guest blocks on the ready line. A couple of
// passes' worth: enough that the head never starves between drains, little
// enough that backpressure stays tight.
static constexpr double   s_kBufferSeconds = 0.75;

// Bytes per gated drain slice while filling the buffer.
static constexpr size_t   s_kDrainSliceBytes = 4096;





////////////////////////////////////////////////////////////////////////////////
//
//  RasterInkExtent
//
//  Non-locking rightmost-ink probe over rows [firstRow, lastRow]: one past the
//  rightmost inked dot, 0 for a blank span. The locking SpanInkExtent shares it.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterEngine::RasterInkExtent (const PrintRaster & raster, int firstRow, int lastRow)
{
    int  extent = 0;
    int  last   = (std::min) (lastRow, raster.RowsUsed() - 1);



    for (int row = (std::max) (0, firstRow); row <= last; row++)
    {
        for (int col = PrinterGrid::kDotsPerRow - 1; col >= extent; col--)
        {
            if (raster.CellAt (col, row) != 0)
            {
                extent = col + 1;
                break;
            }
        }

        if (extent == PrinterGrid::kDotsPerRow)
        {
            break;   // can't grow further
        }
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Builds a fresh job over the ring, restoring a persisted strip if given, and
//  resets the head + preview state. No thread -- the caller drives Tick.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterEngine::Start (PrinterByteRing & ring, PrintRaster seed)
{
    m_job = make_unique<PrinterJob> (ring);

    // Restore a persisted pending strip before any stepping, so new strikes
    // continue on the restored paper at its saved feed position.
    if (seed.RowsUsed() > 0)
    {
        m_job->Raster() = std::move (seed);
    }

    // The presented layer starts as the restored strip fully laid (that paper is
    // already printed); the head then paints only the new rows it sweeps.
    m_presented = m_job->Raster();

    // Reflect any restored strip so the indicator shows Pending immediately.
    m_hasContent.store (m_job->HasContent(), std::memory_order_relaxed);
    m_rowsUsed.store   (m_job->Raster().RowsUsed(), std::memory_order_relaxed);
    m_headPos.store    (((uint64_t) (uint32_t) m_job->HeadRow() << 32)
                        | (uint32_t) m_job->HeadColumnDots(), std::memory_order_relaxed);

    // Re-seed the event head so the new strip starts throttled from cycle zero.
    // Park it at the bottom of any restored strip so the head resumes below the
    // restored paper rather than replaying it.
    m_pacingSeeded = false;
    m_head.Reset (m_job->Raster().RowsUsed());
    m_carriageCol.store   (0, std::memory_order_relaxed);
    m_hostFormFeeds.store (0, std::memory_order_relaxed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FlushNow
//
////////////////////////////////////////////////////////////////////////////////

size_t PrinterEngine::FlushNow (vector<PrinterEvent> & events)
{
    // No job means nothing was ever queued, so zero drained is the truth.
    return (m_job != nullptr) ? m_job->Drain (events) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrySnapshotStrip
//
//  Copies the strip under the raster lock so the live preview reads a consistent
//  image while the drain thread keeps Ticking -- no Stop()/Start() and no new
//  interpreter, so the guest's in-flight state (line feed, color, head column)
//  is never reset out from under it.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterEngine::TrySnapshotStrip (PrintRaster & out)
{
    bool  ok = false;



    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    // No job means no strip to copy, and `out` is left as the caller had it.
    if (m_job != nullptr)
    {
        out = m_job->Raster();   // copy under lock
        ok  = true;
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrySnapshotStripSpan
//
//  Viewport-bounded variant of TrySnapshotStrip: copies only the requested rows
//  under the raster lock, so the live preview's per-refresh cost stays flat no
//  matter how long the banner grows.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterEngine::TrySnapshotStripSpan (int firstRow, int lastRow, PrintRaster & out)
{
    bool  ok = false;



    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job != nullptr)
    {
        m_job->Raster().CopyRowSpan (firstRow, lastRow, out);
        ok = true;
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrySnapshotPresentedSpan
//
//  Same as TrySnapshotStripSpan but over the presented ("wet ink") layer the head
//  paints -- what the live preview renders.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterEngine::TrySnapshotPresentedSpan (int firstRow, int lastRow, PrintRaster & out)
{
    bool  ok = false;



    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    // Gated on m_job, not on m_presented: with no job the presented layer is
    // stale paper from a finished print and the caller wants a blank sheet.
    if (m_job != nullptr)
    {
        m_presented.CopyRowSpan (firstRow, lastRow, out);
        ok = true;
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SpanInkExtent
//
//  Rightmost-ink probe over rows [firstRow, lastRow] under the raster lock:
//  returns one past the rightmost inked dot (0 == blank span).
//
////////////////////////////////////////////////////////////////////////////////

int PrinterEngine::SpanInkExtent (int firstRow, int lastRow)
{
    int  extent = 0;



    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    // 0 is both "no job" and "blank span" -- callers treat them the same.
    if (m_job != nullptr)
    {
        extent = RasterInkExtent (m_job->Raster(), firstRow, lastRow);
    }

    return extent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormFeed
//
//  Records a host page-advance request. Tick performs the actual feed on the
//  Ticking thread so the interpreter, raster and event timeline stay single-
//  writer; the head then slews through the new blank page at feed speed.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterEngine::FormFeed()
{
    m_hostFormFeeds.fetch_add (1, std::memory_order_relaxed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  One print step at wall-clock `nowMs`. It (1) plays any host Form Feed request;
//  (2) runs the interpreter far enough ahead to keep the line buffer full -- but
//  no further, so the ring backs up and the guest throttles itself; (3) replays
//  the emitted carriage timeline for the elapsed guest cycles at the real carriage
//  / feed speed; (4) publishes the platen + reveal frontier + strip height for the
//  preview, which follows the head 1:1. With no clock wired (tests) it drains
//  freely and snaps the head to the raster.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterEngine::Tick (int64_t nowMs)
{
    vector<PrinterEvent>   events;
    size_t                 drained    = 0;
    int                    platenRow  = 0;
    int                    maskCol    = 0;
    int                    carriage   = 0;
    int                    revealTop  = 0;
    bool                   sweepLtr   = true;
    bool                   moving     = false;
    int                    rasterRows = 0;
    bool                   content    = false;



    if (m_job == nullptr)
    {
        return;
    }

    {
        int  hostFeeds = 0;

        std::lock_guard<std::mutex>   lock (m_rasterMutex);

        // Play any host Form Feed requests on this (the single writer) thread,
        // enqueuing their feed motion for the head to slew through.
        hostFeeds = m_hostFormFeeds.exchange (0, std::memory_order_relaxed);

        for (int f = 0; f < hostFeeds; f++)
        {
            events.clear();
            m_job->FormFeed (events);
            m_head.Queue (events);
        }

        // 1. Keep the line buffer full: run the interpreter ahead until a
        //    buffer's worth of print is queued, then stop (backpressure).
        while (m_head.PendingSeconds() < s_kBufferSeconds)
        {
            size_t   got = 0;

            events.clear();
            got = m_job->Drain (events, s_kDrainSliceBytes);

            if (got == 0)
            {
                break;   // ring empty -- nothing more to queue yet
            }

            m_head.Queue (events);
            drained += got;
        }

        // 2. Advance the head by real print time. Wall clock drives the draft
        //    carriage speed; the guest cycle delta only caps it (paused CPU ->
        //    frozen printer, slowed CPU -> slower, never faster than real).
        {
            double  timeSec = 0.0;

            if (!m_pacingSeeded)
            {
                m_lastTickMs   = nowMs;
                m_lastCycles   = (m_guestCycles != nullptr) ? *m_guestCycles : 0;
                m_pacingSeeded = true;
            }

            timeSec = (double) (nowMs - m_lastTickMs) / 1000.0;

            m_lastTickMs = nowMs;

            if (m_guestCycles != nullptr)
            {
                uint64_t   cyclesNow = *m_guestCycles;   // best-effort cross-thread read; only caps
                double     cycleSec  = (double) (cyclesNow - m_lastCycles) / s_kGuestCyclesPerSec;

                m_lastCycles = cyclesNow;

                if (cycleSec < timeSec) { timeSec = cycleSec; }   // guest paused / below 1x
            }

            if (timeSec > s_kMaxTickSec) { timeSec = s_kMaxTickSec; }
            if (timeSec < 0.0)           { timeSec = 0.0; }

            m_head.Advance (timeSec, m_job->Raster(), m_presented);
        }

        // Snapshot the head's published state under the lock. The platen is where
        // the head sits (it slews through a feed); the reveal frontier is the pass
        // being laid, held one band back through a feed so freshly fed paper reads
        // blank; the carriage glyph parks where the last pass ended rather than
        // snapping to the left margin.
        platenRow  = m_head.PlatenRow();
        maskCol    = m_head.MaskCol();
        revealTop  = m_head.RevealTop();
        sweepLtr   = m_head.SweepLtr();
        moving     = m_head.Moving();
        carriage   = m_head.CarriageCol();
        rasterRows = m_job->Raster().RowsUsed();
        content    = m_job->HasContent();
    }

    // Publish activity (guest bytes consumed) + content + platen + reveal frontier
    // + strip height. RowsUsed stays the raster's real built height (the panel
    // clamps the viewport and detects a tear against it).
    if (drained > 0)
    {
        m_activity.fetch_add (drained, std::memory_order_relaxed);
    }

    m_hasContent.store (content,    std::memory_order_relaxed);
    m_rowsUsed.store   (rasterRows, std::memory_order_relaxed);
    m_headPos.store    (((uint64_t) (uint32_t) platenRow << 32) | (uint32_t) maskCol,
                        std::memory_order_relaxed);
    m_headLtr.store      (sweepLtr ? 1 : 0, std::memory_order_relaxed);
    m_revealTop.store    (revealTop, std::memory_order_relaxed);
    m_carriageCol.store  (carriage, std::memory_order_relaxed);
    m_headMoving.store   (moving ? 1 : 0, std::memory_order_relaxed);
}
