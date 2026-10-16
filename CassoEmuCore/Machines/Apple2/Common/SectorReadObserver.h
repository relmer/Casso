#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ISectorReadObserver
//
//  Called with each sector a volume reads from its DOS-logical buffer, as
//  the sector's index there: track * 16 + sector. A ProDOS block is two such
//  sectors. LinkedDisk uses the calls to find the sectors an operation reads
//  that are not yet in memory.
//
////////////////////////////////////////////////////////////////////////////////

class ISectorReadObserver
{
public:
    virtual ~ISectorReadObserver () = default;

    virtual void  OnSectorRead (size_t sector) = 0;
};
