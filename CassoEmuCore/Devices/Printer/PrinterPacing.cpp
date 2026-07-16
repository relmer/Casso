#include "Pch.h"

#include "PrinterPacing.h"




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
//  Sets how many rows the guest has produced (the reveal chases this). The
//  column argument is retained for API / future head-column tracking, but the
//  sweep runs the full carriage width every band (revealing ink IS the sweep),
//  so it is not used to bound the sweep here. A target that shrinks below the
//  reveal (a tear-off) clamps the reveal back onto the fresh sheet.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::SetTargetPosition (int targetRows, int targetColDots)
{
    bool  wasCaughtUp = m_revealed >= (double) m_target;

    (void) targetColDots;

    m_target = (targetRows > 0) ? targetRows : 0;

    if (m_revealed > (double) m_target)
    {
        m_revealed    = (double) m_target;                      // target shrank (tear-off): clamp
        m_revealedCol = (double) PrinterGrid::kDotsPerRow;
    }
    else if (wasCaughtUp && m_revealed < (double) m_target)
    {
        m_revealedCol = 0.0;   // new content after catching up: begin a fresh live band's sweep
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

int PrinterPacing::Advance (double nowSeconds)
{
    double  dt          = 0.0;
    double  dotsPerRow  = (double) PrinterGrid::kDotsPerRow;

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

    // Already at the newest content: hold the live band complete, no sweep.
    if (m_revealed >= (double) m_target)
    {
        m_revealed    = (double) m_target;
        m_revealedCol = dotsPerRow;
        return RevealedRows();
    }

    // Fast-forward, or a backlog too large to sweep through: snap to the target.
    if (m_fastForward || ((double) m_target - m_revealed) > m_cfg.coalesceRows)
    {
        m_revealed    = (double) m_target;
        m_revealedCol = dotsPerRow;
        m_fastForward = false;
        return RevealedRows();
    }

    // Sweep the carriage across the live band at the real ImageWriter rate. Each
    // full-width pass reveals ONE pin band and steps the head down a band
    // (reversing direction, bidirectional print) -- so revealing rows and the
    // head sweep are the same motion and neither can outrun the other.
    m_revealedCol += m_cfg.dotsPerSecond * dt;

    while (m_revealedCol >= dotsPerRow && m_revealed < (double) m_target)
    {
        m_revealedCol     -= dotsPerRow;
        m_revealed         = (std::min) ((double) m_target, m_revealed + (double) m_cfg.rowsPerSweep);
        m_sweepLeftToRight = !m_sweepLeftToRight;
    }

    if (m_revealed >= (double) m_target)
    {
        m_revealed    = (double) m_target;   // reached newest content mid-pass
        m_revealedCol = dotsPerRow;          // show the live band complete
    }
    else if (m_revealedCol > dotsPerRow)
    {
        m_revealedCol = dotsPerRow;
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
    int  col = 0;

    // Caught up: the live band shows complete (no partial sweep to draw).
    if (RevealedRows() >= m_target)
    {
        return PrinterGrid::kDotsPerRow;
    }

    // Sweeping the live band -- expose it left-to-right up to the head column.
    col = (int) m_revealedCol;

    if (col < 0)                        { col = 0; }
    if (col > PrinterGrid::kDotsPerRow) { col = PrinterGrid::kDotsPerRow; }

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
