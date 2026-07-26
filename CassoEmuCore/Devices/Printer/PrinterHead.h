#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"
#include "Devices/Printer/PrintRaster.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterHead
//
//  The pure mechanical model of the ImageWriter II carriage and paper feed. It
//  replays the interpreter's motion timeline -- a HeadBurst per printed pass
//  (its row and struck width) and a LineFeed / FormFeed per paper advance -- in
//  real print time: the carriage sweeps its printed width at the draft speed,
//  parks where it finished between passes, alternates direction (bidirectional
//  print), feeds at the line / form-feed rate, and paints each pass's ink into
//  the "wet" preview layer as it crosses.
//
//  No threading, no clock, no raster ownership: the PrinterWorker owns those and
//  calls Advance() with the elapsed print time and the built / presented rasters
//  under its lock. All the head math -- the right-to-left mirror geometry, the
//  monotonic print frontier, feed-to-sound pacing, and where the carriage parks
//  -- lives here in core so every case is unit-testable (Constitution VI: the
//  platform edge stays thin, the model is covered).
//
////////////////////////////////////////////////////////////////////////////////

class PrinterHead
{
public:
    // Documented ImageWriter II draft mechanics. Draft text is rated 2 ppm: a
    // 66-line page in ~30 s -- a carriage that sweeps its 8" line (1280 dots at
    // 160 dpi) in ~0.32 s, i.e. 25 in/s or 4000 dots/s. We print at the highest
    // quality for free but move the head at that documented draft speed whatever
    // the band height, so the carriage looks right whether it lays 8-dot text or
    // a 16-dot graphics band (which then simply takes two passes, as the real
    // printer does). A LINE feed of a few rows at 480 rows/s reads as instant.
    static constexpr double   kCarriageDotsPerSec = 4000.0;
    static constexpr double   kFeedRowsPerSec     = 480.0;

    // A FORM feed is paced so the paper stops EXACTLY when its page-feed sound
    // ends (a real ImageWriter's motor and paper stop together). The audio picks
    // a grain by feed fraction -- short / medium / long, thresholds 1/3 and 2/3
    // of a page; these durations MUST match PrinterAudioSource's
    // page_feed_{short,medium,long}.mp3 (measured 2026-07-25).
    static constexpr double   kPageFeedGrainSec[3] = { 0.680, 1.250, 3.210 };

    // Seconds a FORM feed of `rows` takes: the sound grain its fraction of a page
    // selects, so the feed rate (rows / duration) stops the paper with the sound.
    static double   FormFeedDurationSec (int rows);

    // Park the head at `parkRow` with an empty timeline -- a fresh sheet (row 0)
    // or the bottom of a restored strip, so printing resumes below existing paper
    // rather than replaying it. The frontier and mask start at the park row so
    // nothing below reads as printed.
    void      Reset (int parkRow);

    // Append a drain's motion events to the timeline. HeadBurst / LineFeed /
    // FormFeed carry head motion and are queued; other event types are ignored.
    void      Queue (const vector<PrinterEvent> & events);

    // Replay `timeSec` of print time along the timeline: sweep the current burst
    // across its printed width at the carriage speed -- painting its ink into
    // `presented` wherever `built` already carries this pass's colour -- slew a
    // feed at the feed rate, and pop the next event as each completes. `built` is
    // the fully-composited strip the drain builds ahead; `presented` is the wet-
    // ink preview layer the head lays one pass at a time. Both are supplied (and
    // must be guarded) by the caller.
    void      Advance (double timeSec, const PrintRaster & built, PrintRaster & presented);

    // Seconds of print time still queued: the remainder of the in-progress motion
    // plus every pending event, each at its own speed. The worker gates the drain
    // on this so the ring backs up and the guest throttles to the print rate.
    double    PendingSeconds () const;

    // ---- Published state (mirrors the PrinterWorker's atomics) -------------

    // True while nothing is moving and nothing is queued (head parked).
    bool      Idle        () const { return m_phase == Phase::Idle && m_pending.empty (); }

    // True while the carriage is mid-pass, the paper is feeding, or motion is
    // still queued -- the panel keeps requesting animation frames while it holds.
    bool      Moving      () const { return m_phase != Phase::Idle || !m_pending.empty (); }

    // The platen: the paper row under the head. It slews through a feed.
    int       PlatenRow   () const { return (int) m_headRow; }

    // The reveal mask's swept column while printing (0 between passes, so nothing
    // below the frontier shows during a feed).
    int       MaskCol     () const { return (m_phase == Phase::Sweeping) ? (int) m_headCol : 0; }

    // The reveal frontier's top row: the pass being laid while printing, held at
    // the last finished pass through a feed so freshly fed paper reads blank.
    int       RevealTop   () const { return (m_phase == Phase::Sweeping) ? (int) m_sweepMaskTop : (int) m_frontier; }

    // The carriage sweep direction of the live line (bidirectional print).
    bool      SweepLtr    () const { return (m_phase == Phase::Sweeping) ? m_sweepLtr : true; }

    // The physical carriage column in dots (0 = left margin) for the head glyph.
    // Unlike the reveal mask -- which closes to 0 during a feed -- this HOLDS at
    // the last sweep's end so the carriage parks where it finished instead of
    // snapping to the left edge.
    int       CarriageCol () const { return m_carriageCol; }

private:
    enum class Phase { Idle, Sweeping, Feeding };

    // Lay the current sweep's primary into `presented` across the carriage travel
    // [fromP, toP] (progress dots), struck only where `built` already carries this
    // pass's colour so overprints accrue into composites (red -> red|blue purple).
    void      PaintPresented (double fromP, double toP,
                              const PrintRaster & built, PrintRaster & presented);

    std::deque<PrinterEvent>   m_pending;                    // unplayed motion timeline
    Phase     m_phase          = Phase::Idle;                // what the head is doing right now
    double    m_headRow        = 0.0;                        // platen: paper row under the head
    double    m_headCol        = 0.0;                        // sweep edge in dots (L>R-equivalent)
    double    m_sweepWidth     = 0.0;                        // current burst's printed width
    double    m_feedTarget     = 0.0;                        // paper row the current feed ends at
    double    m_feedRate       = kFeedRowsPerSec;            // rows/s of the current feed (line vs form)
    double    m_frontier       = 0.0;                        // furthest printed row -- monotonic, blank below
    double    m_sweepMaskTop   = 0.0;                        // reveal-mask top for the current sweep
    double    m_sweepPaintedTo = 0.0;                        // sweep progress already painted into presented
    Byte      m_sweepColor     = 0;                          // primary bits the current sweep lays
    bool      m_sweepLtr       = true;                       // direction of the current sweep
    bool      m_nextLtr        = true;                       // direction the next sweep will take
    int       m_carriageCol    = 0;                          // physical carriage column (holds in feeds)
};
