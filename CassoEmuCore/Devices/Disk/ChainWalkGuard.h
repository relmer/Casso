#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChainWalkGuard
//
//  Makes a walk over an on-disk chain terminate on any input.
//
//  This exists because the walks it guards run BY DESIGN on volumes chosen for
//  being damaged. A corrupted next-pointer can name a unit already visited, or
//  name itself, and an unguarded walk then loops forever -- a hang, not a bad
//  answer, and the worst possible failure for a tool whose job is recovering
//  broken disks.
//
//  Two bounds, because either alone can be defeated. A visited set stops a
//  cycle; a ceiling derived from the volume's own capacity stops a chain that
//  never repeats but never ends either. Hitting either one is reported, never
//  silently treated as a clean finish -- a walk that stopped early because it
//  gave up is not the same as a walk that reached the end.
//
////////////////////////////////////////////////////////////////////////////////

class ChainWalkGuard
{
public:
    //  `capacity` is the number of addressable units on the volume: no honest
    //  chain can be longer, so exceeding it is proof of corruption.
    explicit ChainWalkGuard (uint32_t capacity);

    //  Records a step. False means STOP -- either the unit was already seen or
    //  the walk has run past what the volume could hold. The reason is
    //  available afterwards.
    bool  TryVisit (uint32_t unit);

    //  True when the walk stopped because it hit a bound rather than because it
    //  reached the end of the chain. Callers report this as an unfollowable
    //  chain instead of pretending the walk completed.
    bool  HitBound () const { return m_hitBound; }

    bool  SawCycle       () const { return m_sawCycle; }
    bool  ExceededLength () const { return m_exceededLength; }

    uint32_t  GetVisitedCount () const { return m_visited; }

private:
    vector<bool>  m_seen;
    uint32_t      m_capacity       = 0;
    uint32_t      m_visited        = 0;
    bool          m_hitBound       = false;
    bool          m_sawCycle       = false;
    bool          m_exceededLength = false;
};
