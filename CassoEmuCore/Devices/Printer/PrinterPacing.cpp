#include "Pch.h"

#include "PrinterPacing.h"




// Floor on a pass's sweep width (logic seeking): even a single-dot line moves
// the head a visible nudge rather than completing a zero-length "pass".
static constexpr int   s_kMinSweepDots = 64;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::PrinterPacing
//
////////////////////////////////////////////////////////////////////////////////

PrinterPacing::PrinterPacing (const Config & cfg)
    : m_cfg (cfg)
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::Reset (double nowSeconds, int revealedRows)
{
    m_lastTime    = nowSeconds;
    m_revealed    = (revealedRows > 0) ? (double) revealedRows : 0.0;
    m_revealedCol = 0.0;   // a fresh live band starts its sweep at the left margin
    m_started     = true;
    m_fastForward = false;
    m_sweepLeftToRight = true;

    if (m_target < (int) m_revealed)
    {
        m_target = (int) m_revealed;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::SetTargetRows
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::SetTargetRows (int targetRows)
{
    SetTargetPosition (targetRows, PrinterGrid::kDotsPerRow);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::SetTargetPosition
//
//  Sets how many rows the guest has produced (the carriage chases this) and
//  the live band's sweep width -- the real machine's LOGIC SEEKING: the head
//  travels only the printed span of each pass (plus overtravel the caller
//  bakes in), so a short catalog line sweeps ~2 inches, not the whole platen.
//  A mid-pass column already beyond a narrower new width clamps to it, so the
//  pass completes instead of overshooting.
//
//  Crucially this does NOT touch the head column otherwise. When the carriage
//  has caught the guest it parks at the margin its last pass ended on; fresh
//  content simply lets Advance resume the sweep from that same margin.
//  Resetting the column here (the old behavior) teleported the head back to an
//  edge on every sub-chunk the guest fed a band in -- the forward/back/forward
//  stutter. A target that shrinks below the reveal (a tear-off) rewinds onto
//  the fresh sheet.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::SetTargetPosition (int targetRows, int targetColDots)
{
    m_target         = (targetRows > 0) ? targetRows : 0;
    m_sweepWidthDots = std::clamp (targetColDots, s_kMinSweepDots, PrinterGrid::kDotsPerRow);

    if (m_revealedCol > (double) m_sweepWidthDots)
    {
        m_revealedCol = (double) m_sweepWidthDots;   // narrower band: finish the pass at its edge
    }

    if (m_revealed > (double) m_target)
    {
        m_revealed    = (double) m_target;   // target shrank (tear-off): clamp
        m_revealedCol = 0.0;                 // rest the carriage at the home margin
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::RequestFastForward
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::RequestFastForward()
{
    m_fastForward = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::Advance
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::Advance (double nowSeconds, bool liveBandHasInk)
{
    double  dt     = 0.0;
    double  sweepW = (double) m_sweepWidthDots;   // this band's pass span (logic seeking)

    if (!m_started)
    {
        m_lastTime = nowSeconds;
        m_started  = true;
    }

    dt = nowSeconds - m_lastTime;

    if (dt < 0.0)
    {
        dt = 0.0;   // clock went backwards -> no progress this step
    }

    m_lastTime = nowSeconds;

    if (dt > m_cfg.resumeThresholdSeconds)
    {
        dt = m_cfg.resumeNudgeSeconds;   // resuming from a parked loop: nudge to re-arm the sweep, don't leap the gap
    }

    // Already at the newest content: the carriage rests where its last pass left
    // it (a margin). Do NOT move the column -- parking in place is exactly what
    // lets the next band resume the sweep from that margin instead of jumping.
    if (m_revealed >= (double) m_target)
    {
        m_revealed = (double) m_target;
        return RevealedRows();
    }

    // Fast-forward, or a backlog too large to sweep through: jump-cut to the
    // target and rest the carriage at the home margin.
    if (m_fastForward || ((double) m_target - m_revealed) > m_cfg.coalesceRows)
    {
        m_revealed    = (double) m_target;
        m_revealedCol = 0.0;
        m_fastForward = false;
        return RevealedRows();
    }

    // A BLANK live band is a paper feed, not a print: slew the rows through at
    // the paper-feed rate with the head parked where it is -- no sweep, no
    // direction flip. (A form feed used to sweep the carriage across every
    // empty band of the page; the real machine just feeds.) When the feed
    // reaches an inked band the caller reports ink and the sweep resumes from
    // the parked column.
    if (!liveBandHasInk)
    {
        m_revealed = (std::min) ((double) m_target, m_revealed + m_cfg.blankRowsPerSecond * dt);
        return RevealedRows();
    }

    // Sweep the carriage across the live band at the real ImageWriter rate.
    // Each completed pass -- the band's SWEEP WIDTH, not the whole platen
    // (logic seeking) -- reveals ONE pin band and steps the head down a band
    // (reversing direction, bidirectional print), so revealing rows and the
    // head sweep are the same motion and neither can outrun the other.
    m_revealedCol += m_cfg.dotsPerSecond * dt;

    while (m_revealedCol >= sweepW && m_revealed < (double) m_target)
    {
        m_revealedCol     -= sweepW;
        m_revealed         = (std::min) ((double) m_target, m_revealed + (double) m_cfg.rowsPerSweep);
        m_sweepLeftToRight = !m_sweepLeftToRight;
    }

    // Reached the newest content mid-pass: the carriage parks at the column it
    // got to. Produced ink up to here is shown; nothing lies beyond it yet, so
    // there is no snap -- when more arrives the sweep continues from this column.
    if (m_revealed >= (double) m_target)
    {
        m_revealed = (double) m_target;
    }
    if (m_revealedCol > sweepW)
    {
        m_revealedCol = sweepW;
    }

    return RevealedRows();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::RevealedRows
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::RevealedRows() const
{
    int  rows = (int) m_revealed;

    if (rows < 0)        { rows = 0; }
    if (rows > m_target) { rows = m_target; }

    return rows;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::RevealedColDots
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::RevealedColDots() const
{
    // The single source of truth for the head's column: the swept position within
    // the live band while printing (0..SweepWidthDots), or the margin the
    // carriage is parked on when caught up. Never snaps to full width -- a
    // parked head sits at its margin so the presenter can rest the glyph there
    // and resume the next pass smoothly.
    int  col = (int) m_revealedCol;

    if (col < 0)               { col = 0; }
    if (col > m_sweepWidthDots) { col = m_sweepWidthDots; }

    return col;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::TargetRows
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::TargetRows() const
{
    return m_target;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::IsCaughtUp
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPacing::IsCaughtUp() const
{
    return RevealedRows() >= m_target;
}
