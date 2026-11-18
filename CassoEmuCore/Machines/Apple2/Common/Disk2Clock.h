#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Clock
//
//  The CPU's cycle count for a Disk II controller. Each access to the
//  controller first runs the drive on by the cycles since the last access,
//  as AppleWin computes the CPU's cycles at the top of its handler. The
//  source is the bus cycle of the access in flight. With no source, as in a
//  test that drives the controller without a CPU, the controller's Tick runs
//  the drive instead.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Clock
{
public:
    void  SetSource (const uint64_t * source) noexcept;
    bool  HasSource () const noexcept { return m_source != nullptr; }

    //  The source's cycle, or 0 with none.
    uint64_t  GetNow () const noexcept { return (m_source != nullptr) ? *m_source : 0; }

    //  The cycles since the last call, which become the last sync. A power
    //  cycle zeroes the CPU's count without moving the last sync, so a count
    //  that moved back takes the sync with it and returns 0. False with no
    //  source.
    bool  TakeElapsed (uint64_t & outElapsed) noexcept;

    //  The cycles a TakeElapsed now would return, with nothing taken.
    uint64_t  GetPending () const noexcept;

    uint64_t  GetLastSync () const noexcept     { return m_lastSync; }
    void      SetLastSync (uint64_t cycle) noexcept { m_lastSync = cycle; }

private:
    const uint64_t  * m_source   = nullptr;
    uint64_t          m_lastSync = 0;
};
