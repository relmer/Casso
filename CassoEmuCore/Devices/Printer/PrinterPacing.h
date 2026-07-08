#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing
//
//  Pure clock-driven pacing for the printer panel's paper animation (R-012):
//  the panel reveals freshly printed rows over time so the fanfold paper
//  appears to feed as the guest prints, rather than snapping straight to the
//  finished page. Row production and rendering live elsewhere; this owns only
//  the "how many rows should be visible at time T" decision, so it stays
//  deterministic and is unit-tested with an injected clock -- no real time and
//  no threads.
//
//  Behavior:
//   - Rows reveal at a steady rate (rowsPerSecond), approximating the
//     ImageWriter's ~250 cps replay feel.
//   - RequestFastForward reveals everything on the next Advance (skip the show).
//   - A coalescing jump-cut avoids animating a huge backlog: when the target
//     runs more than coalesceRows ahead of what is revealed (a burst of
//     buffered output, or the panel was hidden during a long job), the reveal
//     snaps to the target instead of crawling through thousands of rows.
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
        double  rowsPerSecond = 432.0;    // steady reveal rate (tunable feel)
        double  coalesceRows  = 2000.0;   // backlog beyond this -> jump-cut
    };

    explicit PrinterPacing (const Config & cfg = Config ());

    // Reset to a known state: clears elapsed history and reveals `revealedRows`.
    void  Reset (double nowSeconds, int revealedRows = 0);

    // Total rows produced so far. A target below the revealed count clamps the
    // reveal back down (rows are normally appended, so this is monotonic).
    void  SetTargetRows (int targetRows);

    // Reveal everything on the next Advance (user skips the animation).
    void  RequestFastForward ();

    // Advance the reveal to `nowSeconds`; returns the rows now visible.
    int   Advance (double nowSeconds);

    int   RevealedRows () const;
    int   TargetRows   () const;
    bool  IsCaughtUp   () const;   // nothing left to reveal

private:
    Config  m_cfg;
    double  m_lastTime    = 0.0;
    double  m_revealed    = 0.0;   // fractional so sub-row rates accumulate
    int     m_target      = 0;
    bool    m_started     = false;
    bool    m_fastForward = false;
};
