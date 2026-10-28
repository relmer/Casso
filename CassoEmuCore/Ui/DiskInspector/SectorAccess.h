#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"
#include "Machines/Apple2/Common/DiskAccessLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess
//
//  Which sectors of a track the guest's reads and writes covered (FR-161).
//  A read counts for a sector when it covers at least half of the sector's
//  data field; a write counts for a sector when it covers any part of the
//  sector's address or data field. A stretch that crosses the index wraps
//  at the track's length.
//
////////////////////////////////////////////////////////////////////////////////

class SectorAccess
{
public:
    //  How many times each sector of the track, in the track's sector order,
    //  was read or written.
    static vector<uint32_t>  CountReads  (const TrackAnalysis & track, const vector<CellSpan> & spans);
    static vector<uint32_t>  CountWrites (const TrackAnalysis & track, const vector<CellSpan> & spans);

    //  The total over every span, for a quarter track with no sectors.
    static uint32_t  CountAll (const vector<CellSpan> & spans);

private:
    static uint32_t  GetOverlap (uint32_t length, uint32_t spanStart, uint32_t spanEnd, uint32_t fieldStart, uint32_t fieldEnd);
};
