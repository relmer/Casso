#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"
#include "Machines/Apple2/Common/DiskAccessLog.h"





//  What the guest did to a sector: read its data field, wrote over it, or
//  took its address field while searching for a sector (FR-161).
enum class AccessKind
{
    Reads,
    Writes,
    Scans,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SectorAccess
//
//  Which sectors of a track the guest's reads and writes covered (FR-161).
//  A read counts for a sector when it covers at least half of the sector's
//  data field, and a scan when it covers at least half of its address
//  field; a write counts for a sector when it covers any part of the
//  sector's address or data field. A stretch that crosses the index wraps
//  at the track's length.
//
////////////////////////////////////////////////////////////////////////////////

class SectorAccess
{
public:
    //  How many times each sector of the track, in the track's sector order,
    //  was read, written or scanned.
    static vector<uint32_t>  CountReads  (const TrackAnalysis & track, const vector<CellSpan> & spans);
    static vector<uint32_t>  CountWrites (const TrackAnalysis & track, const vector<CellSpan> & spans);
    static vector<uint32_t>  CountScans  (const TrackAnalysis & track, const vector<CellSpan> & spans);
    static vector<uint32_t>  Count       (AccessKind kind, const TrackAnalysis & track, const vector<CellSpan> & spans);

    //  The total over every span, for a quarter track with no sectors.
    static uint32_t  CountAll (const vector<CellSpan> & spans);

private:
    static vector<uint32_t>  CountHalfCovered (const TrackAnalysis & track, const vector<CellSpan> & spans, bool isAddress);
    static uint32_t  GetOverlap (uint32_t length, uint32_t spanStart, uint32_t spanEnd, uint32_t fieldStart, uint32_t fieldEnd);
};
