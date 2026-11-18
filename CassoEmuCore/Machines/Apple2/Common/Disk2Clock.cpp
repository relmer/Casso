#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Clock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Clock::SetSource
//
//  The last sync starts at the source's cycle, so the first access runs
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Clock::SetSource (const uint64_t * source) noexcept
{
    m_source   = source;
    m_lastSync = (source != nullptr) ? *source : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Clock::TakeElapsed
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2Clock::TakeElapsed (uint64_t & outElapsed) noexcept
{
    uint64_t  now = 0;



    outElapsed = 0;

    if (m_source == nullptr)
    {
        return false;
    }

    now = *m_source;

    if (now > m_lastSync)
    {
        outElapsed = now - m_lastSync;
    }

    m_lastSync = now;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Clock::GetPending
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Disk2Clock::GetPending() const noexcept
{
    bool  isAhead = m_source != nullptr && *m_source > m_lastSync;



    return isAhead ? *m_source - m_lastSync : 0;
}
