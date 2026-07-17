#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing
//
//  Pure clock-driven pacing for the printer panel's paper animation (R-012):
//  the panel reveals freshly printed rows over time so the fanfold paper
//  appears to feed as the guest prints, rather than snapping straight to the
//  finished page. Row production and rendering live elsewhere; this owns only
//  the "how much should be visible at time T" decision, so it stays
//  deterministic and is unit-tested with an injected clock -- no real time and
//  no threads.
//
//  Behavior (single carriage clock -- the row reveal and the head sweep are the
//  SAME motion, not two independent rates that can outrun each other):
//   - The head sweeps the live band left-to-right (then right-to-left, ...) at
//     dotsPerSecond; RevealedColDots is that sweep position. Rows already swept
//     show complete below it.
//   - Completing a full-width sweep reveals exactly ONE pin band (rowsPerSweep
//     rows) and steps the head down a band -- so revealing ink IS the sweep, at
//     the real ImageWriter's ~one-band-per-pass rate, no matter how fast the
//     guest dumped the bytes. This is what keeps the motion smooth: the reveal
//     never blows through a band full-width faster than the head could draw it.
//   - RequestFastForward reveals everything on the next Advance (skip the show).
//   - A coalescing jump-cut avoids animating a huge backlog: when the target
//     runs more than coalesceRows ahead of what is revealed (a burst of
//     buffered output, or the panel was hidden during a long job), the reveal
//     snaps to the target instead of sweeping through thousands of rows.
//   - SetTargetPosition's column argument is retained for API/head-tracking use
//     but the sweep runs the full carriage width every band (the panel drives it
//     with the full width); it never pulls the reveal backwards.
//
//  All times are seconds on a caller-supplied monotonic clock; a clock that
//  goes backwards simply makes no progress for that step.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterPacing
{
public:
    struct Config
    {
        // The carriage sweep speed is the ONLY rate: rows reveal as a consequence
        // of sweeps completing (one pin band per full-width pass), so the two can
        // never run at incoherent rates. dotsPerSecond / kDotsPerRow * rowsPerSweep
        // is the effective row-feed rate (~50 rows/s at the defaults -- real
        // ImageWriter graphics speed).
        double  dotsPerSecond = 4000.0;                     // head sweep across the band (FR-034)
        int     rowsPerSweep  = PrinterGrid::kPinBandRows;  // rows revealed per full-width sweep
        double  coalesceRows  = 2000.0;                     // backlog beyond this -> jump-cut

        // The panel's render loop drops to a coarse idle tick between the guest's
        // data bursts, so the first Advance after a gap carries a large dt.
        // Acting on it whole would leap a full carriage pass (or several) in one
        // frame -- a visible jump. But a fixed ceiling on every dt is worse: it
        // also clamps merely-slow frames, dragging the sweep below carriage speed
        // whenever the frame rate dips (the "sometimes very slow" head). So a dt
        // beyond resumeThresholdSeconds is treated as a RESUME from a parked loop
        // -- the head advances only resumeNudgeSeconds to re-arm the sweep (which
        // re-hots the loop), and the backlog then animates over the frames that
        // follow. Normal and slow frames pass through untouched, so the sweep
        // always runs at full carriage speed regardless of frame rate.
        double  resumeThresholdSeconds = 0.20;   // dt above this == the loop was parked, not merely slow
        double  resumeNudgeSeconds     = 0.02;   // advance only this much on the resume frame
    };

    explicit PrinterPacing (const Config & cfg = Config ());

    // Reset to a known state: clears elapsed history and reveals `revealedRows`
    // (at full line width -- no sweep in progress).
    void  Reset (double nowSeconds, int revealedRows = 0);

    // Total rows produced so far. A target below the revealed count clamps the
    // reveal back down (rows are normally appended, so this is monotonic).
    // Equivalent to SetTargetPosition (targetRows, full width).
    void  SetTargetRows (int targetRows);

    // Head position target (FR-034): `targetRows` complete rows above the live
    // line, plus the head's dot column within it.
    void  SetTargetPosition (int targetRows, int targetColDots);

    // Reveal everything on the next Advance (user skips the animation).
    void  RequestFastForward ();

    // Advance the reveal to `nowSeconds`; returns the rows now visible.
    int   Advance (double nowSeconds);

    int   RevealedRows () const;

    // The head's dot column -- the single source of truth for where the carriage
    // is. While printing it is the swept position within the live line (rows
    // below RevealedRows() are complete; the line AT RevealedRows() shows up to
    // this column). When caught up it is the margin the carriage parked on -- it
    // never snaps to full width, so the head rests and resumes without jumping.
    int   RevealedColDots () const;

    int   TargetRows   () const;
    bool  IsCaughtUp   () const;   // no rows left to reveal (carriage parked at a margin)

    // The real ImageWriter prints bidirectionally: each line's carriage pass
    // runs opposite to the last. This flag flips every time a fresh line's sweep
    // begins (see Advance), so the presenter can drive the head + ink reveal
    // right-to-left on alternate lines. Starts left-to-right.
    bool  SweepLeftToRight () const { return m_sweepLeftToRight; }

private:
    Config  m_cfg;
    double  m_lastTime    = 0.0;
    double  m_revealed    = 0.0;   // top of the live (sweeping) band, in rows
    double  m_revealedCol = 0.0;   // head column within it; 0 == parked at the home margin
    int     m_target      = 0;
    bool    m_started     = false;
    bool    m_fastForward = false;
    bool    m_sweepLeftToRight = true;   // reverses each new band (bidirectional)
};
