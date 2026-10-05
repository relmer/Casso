#include "Pch.h"

#include "Debugger/Reverse/HistoryStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryStatus::GetWallTimeAt
//
//  The base time moved on by the emulated time from baseCycle to cycle, in
//  a FILETIME's 100 ns ticks; a cycle before the base counts back.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryStatus::GetWallTimeAt (
    uint64_t  baseWallTime,
    uint64_t  baseCycle,
    uint64_t  cycle)
{
    constexpr double  kTicksPerSecond = 10000000.0;
    double            seconds         = ((double) cycle - (double) baseCycle) / kCyclesPerSecond;



    if (baseWallTime == 0)
    {
        return 0;
    }

    return baseWallTime + (uint64_t) std::llround (seconds * kTicksPerSecond);
}





