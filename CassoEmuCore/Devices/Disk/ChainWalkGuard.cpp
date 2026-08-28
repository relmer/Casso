#include "Pch.h"

#include "ChainWalkGuard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChainWalkGuard::ChainWalkGuard
//
////////////////////////////////////////////////////////////////////////////////

ChainWalkGuard::ChainWalkGuard (uint32_t capacity)
{
    m_capacity = capacity;
    m_seen.assign (capacity, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChainWalkGuard::TryVisit
//
//  A unit outside the volume is refused for the same reason a repeat is: the
//  chain has left the realm of things that could be true, and following it
//  further only compounds the corruption.
//
//  The visited flag is set only for a step that is actually taken, so the
//  visited count stays a count of real progress rather than of attempts.
//
////////////////////////////////////////////////////////////////////////////////

bool ChainWalkGuard::TryVisit (uint32_t unit)
{
    bool  inRange = unit < m_capacity;
    bool  repeat  = false;
    bool  tooLong = m_visited >= m_capacity;



    if (!inRange || tooLong)
    {
        m_hitBound       = true;
        m_exceededLength = true;
        return false;
    }

    repeat = m_seen[unit];

    if (repeat)
    {
        m_hitBound = true;
        m_sawCycle = true;
        return false;
    }

    m_seen[unit] = true;
    m_visited++;

    return true;
}
