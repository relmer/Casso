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
    m_revealedCol = (double) PrinterGrid::kDotsPerRow;
    m_targetCol   = PrinterGrid::kDotsPerRow;
    m_sweepRow    = -1;
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
//  Row channel behaves exactly like the original SetTargetRows. The column
//  channel is per-row: a NEW target row installs a fresh column target (the
//  sweep restarts when the reveal arrives there -- see Advance); further
//  updates on the SAME row max-hold the column, so an overprint pass sweeping
//  the line again never pulls the reveal backwards.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::SetTargetPosition (int targetRows, int targetColDots)
{
    int   col = std::clamp (targetColDots, 0, PrinterGrid::kDotsPerRow);

    m_target = (targetRows > 0) ? targetRows : 0;

    if (m_revealed > (double) m_target)
    {
        m_revealed = (double) m_target;
    }

    if (m_target != m_sweepRow)
    {
        m_sweepRow  = m_target;
        m_targetCol = col;
    }
    else if (col > m_targetCol)
    {
        m_targetCol = col;
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
    double  dt        = 0.0;
    bool    wasBehind = RevealedRows() < m_target;

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

    if (m_fastForward)
    {
        m_revealed    = (double) m_target;
        m_revealedCol = (double) m_targetCol;
        m_fastForward = false;
    }
    else if (((double) m_target - m_revealed) > m_cfg.coalesceRows)
    {
        m_revealed    = (double) m_target;      // jump-cut past a large backlog
        m_revealedCol = (double) m_targetCol;   // no sweep after a jump-cut
    }
    else
    {
        m_revealed += m_cfg.rowsPerSecond * dt;

        if (m_revealed > (double) m_target)
        {
            m_revealed = (double) m_target;
        }

        if (RevealedRows() >= m_target)
        {
            if (wasBehind)
            {
                // The reveal just reached the live line: the head starts a fresh
                // sweep (the dt spent catching up is not double-spent on the
                // sweep; it continues next step). Each new line reverses the
                // carriage direction -- the ImageWriter prints bidirectionally.
                m_revealedCol      = 0.0;
                m_sweepLeftToRight = !m_sweepLeftToRight;
            }
            else
            {
                m_revealedCol += m_cfg.dotsPerSecond * dt;

                if (m_revealedCol > (double) m_targetCol)
                {
                    m_revealedCol = (double) m_targetCol;
                }
            }
        }
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

    if (RevealedRows() < m_target)
    {
        // Still catching up through older rows: those lines show complete.
        return PrinterGrid::kDotsPerRow;
    }

    col = (int) m_revealedCol;

    if (col < 0)           { col = 0; }
    if (col > m_targetCol) { col = m_targetCol; }

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
