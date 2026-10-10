#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Devices/Disk/Inspector/FileMap/SectorSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparer
//
//  Compares what the drive reads on two analyzed disks (FR-118 to FR-120):
//  each whole track once, quarter tracks of the standard layouts set aside,
//  every other quarter track against the same one on the other disk. A
//  track's verdict is the strongest that holds; its differences are found
//  after aligning B to A on the first address field both hold, or on the
//  rotation most 8-nibble runs agree on, with a Myers diff bounded at
//  kMaxEdits so a sync run of another length marks only that run. Files of
//  the volumes the map reads pair by path. Pure; neither disk changes.
//
////////////////////////////////////////////////////////////////////////////////

class DiskComparer
{
public:
    static constexpr int     kMaxEdits        = 64;
    static constexpr double  kTimingTolerance = 0.01;

    static DiskComparison  Compare (const DiskAnalysis & a, const DiskAnalysis & b);

    //  The rotation, in nibbles, that best lines B's nibbles up with A's.
    static int  Align (const TrackAnalysis & a, const TrackAnalysis & b);

    //  The edits that turn one nibble run into the other, as hunks of A and
    //  B; false when they need more than kMaxEdits.
    struct Hunk
    {
        int  firstA = 0;
        int  countA = 0;
        int  firstB = 0;
        int  countB = 0;
    };

    static bool  Diff (std::span<const Byte> a, std::span<const Byte> b, vector<Hunk> & outHunks);

    //  The nibble hunks that differ on one track once aligned, for the views
    //  of the selected track whatever its verdict.
    static void  ListNibbles (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs);

    //  B's sector paired with A's, by sector number and encoding with repeats
    //  in passing order (FR-120), or -1.
    static int  FindPairedSector (const TrackAnalysis & a, int sectorIndex, const TrackAnalysis & b);

private:
    static bool            IsStandardLayout (const DiskAnalysis & disk, int quarterTrack);
    static void            CompareTrack     (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, TrackComparison & inOut, vector<Difference> & inOutDiffs);
    static bool            IsSameCells      (const TrackAnalysis & a, const TrackAnalysis & b, int & outRotation, bool & outIsTimingSame);
    static bool            IsSameNibbles    (const TrackAnalysis & a, const TrackAnalysis & b);
    static int             CompareSectors   (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs);
    static uint32_t        GetSectorCell    (const TrackAnalysis & track, const AnalyzedSector & sector);
    static void            ListTiming       (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, int rotation, vector<Difference> & inOutDiffs);
    static void            CompareFiles     (const DiskAnalysis & a, const DiskAnalysis & b, DiskComparison & inOut);
    static bool            ReadContents     (const FileMap & map, const SectorSource & source, const MappedFile & file, vector<Byte> & outBytes);
    static bool            ReadCell         (const FileMap & map, const SectorSource & source, int cell, vector<Byte> & inOut);
    static vector<Byte>    GetNibbles       (const TrackAnalysis & track, int rotation);
    static bool            HasSectors       (const TrackAnalysis & track) { return !track.sectors.empty(); }
};
