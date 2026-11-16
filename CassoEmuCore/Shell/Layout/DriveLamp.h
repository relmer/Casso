#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLamp
//
//  When a hard disk's lamp is lit: for `windowCycles` after the CPU cycle of
//  its last command, so a run of reads reads as steady activity. A drive
//  that has had no command has a last cycle of 0 and is dark.
//
////////////////////////////////////////////////////////////////////////////////

class DriveLamp
{
public:
    static bool  IsLit (uint64_t lastActivityCycle, uint64_t nowCycle, uint64_t windowCycles)
    {
        return lastActivityCycle != 0 && nowCycle >= lastActivityCycle && nowCycle - lastActivityCycle < windowCycles;
    }
};
