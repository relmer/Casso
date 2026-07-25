#include "Pch.h"

#include "Print/PrinterWorker.h"

#include "Devices/Printer/PrinterByteRing.h"




static constexpr int   s_kIdleSleepMs = 4;   // loop period: paces the head in ~4 ms steps

// Faithful ImageWriter II mechanics. Draft text is rated 2 ppm: a 66-line page
// in ~30 s. That is a carriage that sweeps its 8" line (1280 dots at 160 dpi) in
// ~0.32 s -- 25 in/s, or 4000 dots/s. We print at the highest quality for free
// but move the head at that documented draft speed regardless of band height, so
// the carriage looks right whether it is laying 8-dot text or a 16-dot graphics
// band (which then simply takes two passes, ~1 ppm, as the real printer does).
static constexpr double   s_kCarriageDotsPerSec = 4000.0;   // 25 in/s draft carriage

// A LINE feed (a few rows) at ~480 rows/s reads as instant.
static constexpr double   s_kFeedRowsPerSec = 480.0;

// A FORM feed is paced so the paper stops EXACTLY when its page-feed sound ends
// (a real ImageWriter's motor and paper stop together). The audio picks a grain
// by feed fraction -- short / medium / long, thresholds 1/3 and 2/3 of a page;
// these durations MUST match PrinterAudioSource's page_feed_{short,medium,long}
// .mp3 (measured 2026-07-25: 0.680 / 1.250 / 3.210 s). Feed rate then = rows/dur.
static constexpr double   s_kPageFeedGrainSec[3] = { 0.680, 1.250, 3.210 };

static double FormFeedDurationSec (int rows)
{
    double   frac = (double) rows / (double) PrinterGrid::kPageRows;
    int      slot = (frac < 1.0 / 3.0) ? 0 : (frac < 2.0 / 3.0) ? 1 : 2;

    return s_kPageFeedGrainSec[slot];
}

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
//  rightmost inked dot, 0 for a blank span. The locking SpanInkExtent and the
//  drain loop (which already holds the raster lock) share this.
//
////////////////////////////////////////////////////////////////////////////////

static int RasterInkExtent (const PrintRaster & raster, int firstRow, int lastRow)
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
//  ~PrinterWorker
//
////////////////////////////////////////////////////////////////////////////////

PrinterWorker::~PrinterWorker()
{
    Stop();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Start
//
//  Builds a fresh job over the card's ring and spawns the drain thread. A
//  running worker is stopped first so Start is idempotent across machine
//  rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Start (PrinterByteRing & ring, PrintRaster seed)
{
    if (m_running)
    {
        Stop();
    }

    m_job = make_unique<PrinterJob> (ring);

    // Restore a persisted pending strip before the thread starts, so new
    // strikes continue on the restored paper at its saved feed position.
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
    m_pending.clear();
    m_phase        = Idle;
    m_headRow      = (double) m_job->Raster().RowsUsed();
    m_headCol      = 0.0;
    m_sweepWidth   = 0.0;
    m_feedTarget   = 0.0;
    m_feedRate     = s_kFeedRowsPerSec;
    m_frontier     = (double) m_job->Raster().RowsUsed();
    m_sweepMaskTop = (double) m_job->Raster().RowsUsed();
    m_sweepPaintedTo = 0.0;
    m_sweepColor   = 0;
    m_sweepLtr     = true;
    m_nextLtr      = true;
    m_carriageCol.store   (0, std::memory_order_relaxed);
    m_hostFormFeeds.store (0, std::memory_order_relaxed);

    m_stopRequested = false;
    m_running       = true;
    m_thread        = std::thread ([this] { Run(); });
}




////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
//  Signals and joins the drain thread. The job (and its raster) remain valid
//  afterwards so the UI thread can render/persist the completed strip.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Stop()
{
    if (!m_running)
    {
        return;
    }

    m_stopRequested = true;

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_running = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  FlushNow
//
////////////////////////////////////////////////////////////////////////////////

size_t PrinterWorker::FlushNow (vector<PrinterEvent> & events)
{
    if (m_job == nullptr)
    {
        return 0;
    }

    return m_job->Drain (events);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotStrip
//
//  Copies the strip under the raster lock so the live preview reads a consistent
//  image while the drain thread keeps running -- no Stop()/Start() and no new
//  interpreter, so the guest's in-flight state (line feed, colour, head column)
//  is never reset out from under it.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterWorker::SnapshotStrip (PrintRaster & out)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return false;
    }

    out = m_job->Raster();   // copy under lock
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotStripSpan
//
//  Viewport-bounded variant of SnapshotStrip: copies only the requested rows
//  under the raster lock, so the live preview's per-refresh cost stays flat
//  no matter how long the banner grows.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterWorker::SnapshotStripSpan (int firstRow, int lastRow, PrintRaster & out)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return false;
    }

    m_job->Raster().CopyRowSpan (firstRow, lastRow, out);
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotPresentedSpan
//
//  Same as SnapshotStripSpan but over the presented ("wet ink") layer the head
//  paints -- what the live preview renders.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterWorker::SnapshotPresentedSpan (int firstRow, int lastRow, PrintRaster & out)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return false;
    }

    m_presented.CopyRowSpan (firstRow, lastRow, out);
    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SpanInkExtent
//
//  Rightmost-ink probe over rows [firstRow, lastRow] under the raster lock:
//  returns one past the rightmost inked dot (0 == blank span). Scans each row
//  from the right and only past the best answer so far; bounded by a pin band
//  in practice (~16 rows), so the cost is trivial.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterWorker::SpanInkExtent (int firstRow, int lastRow)
{
    std::lock_guard<std::mutex>   lock (m_rasterMutex);

    if (m_job == nullptr)
    {
        return 0;
    }

    return RasterInkExtent (m_job->Raster(), firstRow, lastRow);
}




////////////////////////////////////////////////////////////////////////////////
//
//  FormFeed
//
//  Host-initiated page advance (the preview's Form Feed button). The UI thread
//  only records the request; Run performs the actual feed on the drain thread so
//  the interpreter, raster and event timeline all stay single-writer. The head
//  then slews through the new blank page at feed speed, scrolling it into view
//  with the paper-feed sound instead of snapping to a blank sheet.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::FormFeed()
{
    m_hostFormFeeds.fetch_add (1, std::memory_order_relaxed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Run
//
//  The print loop. Each ~4 ms tick it (1) plays any host Form Feed request; (2)
//  runs the interpreter far enough ahead to keep the line buffer full -- but no
//  further, so the ring backs up and the guest throttles itself; (3) replays the
//  emitted carriage timeline for the elapsed guest cycles at the real carriage /
//  feed speed; (4) publishes the platen + reveal frontier + strip height for the
//  preview, which follows the head 1:1. With no clock wired (tests) it drains
//  freely and snaps the head to the raster.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::Run()
{
    vector<PrinterEvent>   events;

    while (!m_stopRequested)
    {
        size_t   drained    = 0;
        int      platenRow  = 0;
        int      maskCol    = 0;
        int      carriage   = 0;
        int      revealTop  = 0;
        bool     sweepLtr   = true;
        bool     moving     = false;
        int      rasterRows = 0;
        bool     content    = false;

        {
            std::lock_guard<std::mutex>   lock (m_rasterMutex);

            // Play any host Form Feed requests on this (the single writer) thread,
            // enqueuing their feed motion for the head to slew through.
            int   hostFeeds = m_hostFormFeeds.exchange (0, std::memory_order_relaxed);

            for (int f = 0; f < hostFeeds; f++)
            {
                events.clear();
                m_job->FormFeed (events);
                QueueMotion (events);
            }

            // 1. Keep the line buffer full: run the interpreter ahead until a
            //    buffer's worth of print is queued, then stop (backpressure).
            while (PendingMotionSeconds() < s_kBufferSeconds)
            {
                size_t   got = 0;

                events.clear();
                got = m_job->Drain (events, s_kDrainSliceBytes);

                if (got == 0)
                {
                    break;   // ring empty -- nothing more to queue yet
                }

                QueueMotion (events);
                drained += got;
            }

            // 2. Advance the head by real print time. Wall clock drives the draft
            //    carriage speed; the guest cycle delta only caps it (paused CPU ->
            //    frozen printer, slowed CPU -> slower, never faster than real).
            {
                int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                      std::chrono::steady_clock::now().time_since_epoch()).count();

                if (!m_pacingSeeded)
                {
                    m_lastTickMs   = nowMs;
                    m_lastCycles   = (m_guestCycles != nullptr) ? *m_guestCycles : 0;
                    m_pacingSeeded = true;
                }

                double   timeSec = (double) (nowMs - m_lastTickMs) / 1000.0;

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

                AdvanceEventHead (timeSec);
            }

            // The platen is where the head sits (it slews through a feed); the
            // reveal frontier is the pass being laid, held one band back through a
            // feed so freshly fed paper reads blank. Between passes the mask closes
            // (col 0) so nothing below the frontier shows.
            platenRow  = (int) m_headRow;
            maskCol    = (m_phase == Sweeping) ? (int) m_headCol : 0;
            revealTop  = (m_phase == Sweeping) ? (int) m_sweepMaskTop : (int) m_frontier;
            sweepLtr   = (m_phase == Sweeping) ? m_sweepLtr : true;
            moving     = (m_phase != Idle) || !m_pending.empty();
            rasterRows = m_job->Raster().RowsUsed();
            content    = m_job->HasContent();

            // Physical carriage column for the glyph: track the sweep edge while
            // sweeping, hold the last position between passes (a real carriage
            // parks where it finished; it does not fly back to the left margin).
            if (m_phase == Sweeping)
            {
                // Mirror a right-to-left pass around THIS line's printed width, not
                // the full platen -- a short line (text) sweeps back over its own
                // ink, not off to the right margin (logic seeking).
                carriage = m_sweepLtr ? (int) m_headCol
                                      : ((int) m_sweepWidth - (int) m_headCol);
            }
            else
            {
                carriage = m_carriageCol.load (std::memory_order_relaxed);
            }
        }

        events.clear();

        // Publish activity (guest bytes consumed) + content + platen + reveal
        // frontier + strip height. RowsUsed stays the raster's real built height
        // (the panel clamps the viewport and detects a tear against it).
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

        std::this_thread::sleep_for (std::chrono::milliseconds (s_kIdleSleepMs));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  QueueMotion
//
//  Appends the head-motion events (a pass, or a paper feed) from a drain to the
//  timeline; other event types carry no carriage motion and are dropped.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::QueueMotion (const vector<PrinterEvent> & events)
{
    for (const PrinterEvent & ev : events)
    {
        if (ev.type == PrinterEventType::HeadBurst ||
            ev.type == PrinterEventType::LineFeed  ||
            ev.type == PrinterEventType::FormFeed)
        {
            m_pending.push_back (ev);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PendingMotionSeconds
//
//  Print time still queued: the remainder of the in-progress motion plus every
//  event waiting in the timeline, each at its own speed. The drain gate holds
//  the buffer to this many seconds (backpressure).
//
////////////////////////////////////////////////////////////////////////////////

double PrinterWorker::PendingMotionSeconds() const
{
    double   seconds = 0.0;

    if (m_phase == Sweeping)
    {
        seconds += (m_sweepWidth - m_headCol) / s_kCarriageDotsPerSec;
    }
    else if (m_phase == Feeding)
    {
        seconds += (m_feedTarget - m_headRow) / (m_feedRate > 0.0 ? m_feedRate : s_kFeedRowsPerSec);
    }

    for (const PrinterEvent & ev : m_pending)
    {
        if (ev.type == PrinterEventType::HeadBurst)
        {
            seconds += (double) (ev.toDot + 1) / s_kCarriageDotsPerSec;
        }
        else if (ev.type == PrinterEventType::FormFeed)
        {
            seconds += (ev.rows > 0) ? FormFeedDurationSec (ev.rows) : 0.0;
        }
        else if (ev.type == PrinterEventType::LineFeed)
        {
            seconds += (double) ev.rows / s_kFeedRowsPerSec;
        }
    }

    return seconds;
}




////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceEventHead
//
//  Replays `timeSec` of print time along the event timeline. Sweeping crosses
//  the current burst's printed width at the carriage speed; the pass completes
//  and the next event is popped. Feeding slews the platen toward the feed target
//  at the feed speed. Idle pops the next event: a HeadBurst starts a sweep,
//  snapping the platen to the pass row and flipping direction (bidirectional
//  print); a LineFeed / FormFeed starts a slew. Called with m_rasterMutex held.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::AdvanceEventHead (double timeSec)
{
    int   guard = 0;

    while (timeSec > 0.0 && guard++ < 8192)
    {
        if (m_phase == Sweeping)
        {
            double   remaining = m_sweepWidth - m_headCol;
            double   sweepTime = remaining / s_kCarriageDotsPerSec;

            if (sweepTime > timeSec)
            {
                m_headCol += s_kCarriageDotsPerSec * timeSec;
                timeSec    = 0.0;
            }
            else
            {
                timeSec  -= sweepTime;
                m_headCol = m_sweepWidth;
                m_phase   = Idle;   // pass complete
            }

            PaintPresented (m_sweepPaintedTo, m_headCol);   // lay this pass's ink up to the head
            m_sweepPaintedTo = m_headCol;
        }
        else if (m_phase == Feeding)
        {
            double   remaining = m_feedTarget - m_headRow;
            double   feedTime  = remaining / m_feedRate;

            if (remaining <= 0.0)
            {
                m_headRow = m_feedTarget;
                m_phase   = Idle;
            }
            else if (feedTime > timeSec)
            {
                m_headRow += m_feedRate * timeSec;
                timeSec    = 0.0;
            }
            else
            {
                timeSec  -= feedTime;
                m_headRow = m_feedTarget;
                m_phase   = Idle;   // feed complete
            }
        }
        else   // Idle: start the next queued motion
        {
            PrinterEvent   ev;

            if (m_pending.empty())
            {
                break;   // nothing queued -- head parked
            }

            ev = m_pending.front();
            m_pending.pop_front();

            if (ev.type == PrinterEventType::HeadBurst)
            {
                double   width = (double) (ev.toDot + 1);

                if (width < 1.0)                                  { width = 1.0; }
                if (width > (double) PrinterGrid::kDotsPerRow)    { width = (double) PrinterGrid::kDotsPerRow; }

                // Mask only the new ink: passes overlap the previous band by a few
                // rows (feed < band height) for a gapless join, and those already-
                // printed rows must not blank when this pass starts, so the mask
                // top never rises above the monotonic print frontier.
                m_headRow        = (double) ev.row;                                // platen snaps to the pass row
                m_sweepMaskTop   = (std::max) ((double) ev.row, m_frontier);
                m_frontier       = (std::max) (m_frontier, (double) ev.row + PrinterGrid::kPinBandRows);
                m_sweepWidth     = width;
                m_headCol        = 0.0;
                m_sweepColor     = (Byte) ev.color;                                // primary this pass lays
                m_sweepPaintedTo = 0.0;
                m_sweepLtr       = m_nextLtr;
                m_nextLtr        = !m_nextLtr;                                      // alternate each pass (bidirectional)
                m_phase          = Sweeping;
            }
            else if (ev.type == PrinterEventType::LineFeed ||
                     ev.type == PrinterEventType::FormFeed)
            {
                m_feedTarget = m_headRow + (double) ev.rows;

                if (ev.type == PrinterEventType::FormFeed && ev.rows > 0)
                {
                    // Match the paper to its sound: finish exactly at the grain end.
                    m_feedRate = (double) ev.rows / FormFeedDurationSec (ev.rows);
                }
                else
                {
                    m_feedRate = s_kFeedRowsPerSec;
                }

                m_phase = Feeding;
            }
            // other event types carry no head motion -- already consumed
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintPresented
//
//  Lays the current sweep's primary into the presented ("wet ink") layer across
//  the carriage travel [fromP, toP] (progress dots). The band is struck only
//  where the built strip already carries this pass's colour, so an overprint
//  accrues into a composite (red pass -> red; blue return -> red|blue = purple)
//  and the same render-time mixing that produces the finished image plays out
//  live. Called under m_rasterMutex from AdvanceEventHead.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterWorker::PaintPresented (double fromP, double toP)
{
    if (m_sweepColor == 0 || m_job == nullptr || toP <= fromP)
    {
        return;
    }

    const PrintRaster &  built = m_job->Raster();
    int                  row0  = (int) m_headRow;
    int                  row1  = row0 + PrinterGrid::kPinBandRows - 1;
    int                  c0    = 0;
    int                  c1    = 0;

    // Map swept progress to physical carriage columns: a right-to-left pass lays
    // from the right edge inward, so its columns mirror around the sweep width.
    if (m_sweepLtr)
    {
        c0 = (int) fromP;
        c1 = (int) toP;
    }
    else
    {
        c0 = (int) (m_sweepWidth - toP);
        c1 = (int) (m_sweepWidth - fromP);
    }

    c0 = (std::max) (0, c0);
    c1 = (std::min) (PrinterGrid::kDotsPerRow - 1, c1);

    for (int r = row0; r <= row1; r++)
    {
        for (int c = c0; c <= c1; c++)
        {
            Byte   lay = (Byte) (built.CellAt (c, r) & m_sweepColor);

            if (lay != 0)
            {
                m_presented.Strike (c, r, (InkPrimary) lay);
            }
        }
    }
}
