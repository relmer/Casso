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
//  Behavior:
//   - Rows reveal at a steady rate (rowsPerSecond), approximating the
//     ImageWriter's ~250 cps replay feel.
//   - Within the live line, ink reveals left-to-right at dotsPerSecond as the
//     head sweeps (FR-034): SetTargetPosition supplies the head's row + dot
//     column; RevealedColDots is the sweep position. While the reveal is
//     still catching up through older rows, those lines show at full width;
//     the sweep restarts from column 0 when the reveal reaches the live row.
//     Same-row column targets are max-held so a second overprint pass (color
//     printing re-sweeps the same line) never un-reveals ink already shown.
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
        double  dotsPerSecond = 4000.0;   // head sweep within the live line (FR-034)
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

    // Dot column revealed within the live line: rows below RevealedRows() are
    // complete; the line AT RevealedRows() shows up to this column. Full width
    // while the reveal is still catching up through older rows.
    int   RevealedColDots () const;

    int   TargetRows   () const;
    bool  IsCaughtUp   () const;   // no rows left to reveal (sweep may continue)

private:
    Config  m_cfg;
    double  m_lastTime    = 0.0;
    double  m_revealed    = 0.0;   // fractional so sub-row rates accumulate
    double  m_revealedCol = (double) PrinterGrid::kDotsPerRow;
    int     m_target      = 0;
    int     m_targetCol   = PrinterGrid::kDotsPerRow;
    int     m_sweepRow    = -1;    // row the current sweep belongs to
    bool    m_started     = false;
    bool    m_fastForward = false;
};
