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
    m_started     = true;
    m_fastForward = false;

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
    m_target = (targetRows > 0) ? targetRows : 0;

    if (m_revealed > (double) m_target)
    {
        m_revealed = (double) m_target;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::RequestFastForward
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPacing::RequestFastForward ()
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
    double  dt = 0.0;

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
        m_fastForward = false;
    }
    else if (((double) m_target - m_revealed) > m_cfg.coalesceRows)
    {
        m_revealed = (double) m_target;   // jump-cut past a large backlog
    }
    else
    {
        m_revealed += m_cfg.rowsPerSecond * dt;

        if (m_revealed > (double) m_target)
        {
            m_revealed = (double) m_target;
        }
    }

    return RevealedRows ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::RevealedRows
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::RevealedRows () const
{
    int  rows = (int) m_revealed;

    if (rows < 0)        { rows = 0; }
    if (rows > m_target) { rows = m_target; }

    return rows;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::TargetRows
//
////////////////////////////////////////////////////////////////////////////////

int PrinterPacing::TargetRows () const
{
    return m_target;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPacing::IsCaughtUp
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPacing::IsCaughtUp () const
{
    return RevealedRows () >= m_target;
}
