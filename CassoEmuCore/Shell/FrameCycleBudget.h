#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FrameCycleBudget
//
//  The cycle target for each pass of the CPU thread's frame loop: whatever
//  takes the CPU's cycle count to the next frame boundary.
//
////////////////////////////////////////////////////////////////////////////////

class FrameCycleBudget
{
public:

    //  The frame loop's slice, which sets how finely audio resolves the
    //  speaker. Prime-ish, so a slice boundary does not land on the same
    //  instruction every frame.
    static constexpr uint32_t  kSliceCycles = 1023;

    static uint32_t  GetTarget      (uint32_t nominalCycles, uint64_t totalCycles);
    static uint32_t  GetPauseTarget (uint32_t nominalCycles, uint64_t totalCycles, double fraction);
};
