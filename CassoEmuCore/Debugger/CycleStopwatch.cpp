#include "Pch.h"

#include "Debugger/CycleStopwatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatch::Arm
//
//  New addresses start a new set of laps.
//
////////////////////////////////////////////////////////////////////////////////

void CycleStopwatch::Arm (Word start, Word stop)
{
    Reset();

    m_start   = start;
    m_stop    = stop;
    m_isArmed = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatch::Disarm
//
//  The laps stay, so they can still be shown.
//
////////////////////////////////////////////////////////////////////////////////

void CycleStopwatch::Disarm()
{
    m_isArmed  = false;
    m_isTiming = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatch::Reset
//
////////////////////////////////////////////////////////////////////////////////

void CycleStopwatch::Reset()
{
    m_startCycle = 0;
    m_laps       = 0;
    m_last       = 0;
    m_shortest   = 0;
    m_longest    = 0;
    m_total      = 0;
    m_isTiming   = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CycleStopwatch::OnInstruction
//
//  The stop address is tested first, so a stopwatch whose two addresses are
//  the same ends one lap and starts the next on the same instruction. A
//  count that went backward under a timing lap -- a power cycle restarts
//  it -- leaves that lap unrecorded.
//
////////////////////////////////////////////////////////////////////////////////

void CycleStopwatch::OnInstruction (Word pc, uint64_t cycles)
{
    uint64_t  lap = 0;



    if (!m_isArmed)
    {
        return;
    }

    if (m_isTiming && pc == m_stop)
    {
        m_isTiming = false;

        if (cycles >= m_startCycle)
        {
            lap        = cycles - m_startCycle;
            m_shortest = (m_laps == 0) ? lap : std::min (m_shortest, lap);
            m_longest  = std::max (m_longest, lap);
            m_last     = lap;
            m_total   += lap;
            ++m_laps;
        }
    }

    if (!m_isTiming && pc == m_start)
    {
        m_isTiming   = true;
        m_startCycle = cycles;
    }
}
