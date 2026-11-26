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
//  track's verdict is the strongest that applies; its differences are found
//  after aligning B to A on the first address field both have, or on the
//  rotation that most 8-nibble runs match at, with an edit-distance alignment
//  that allows kMaxEdits nibbles of drift, so a sync run of another length
//  marks only that run. Files of
//  the volumes the map reads pair by path. Pure; neither disk changes.
//
////////////////////////////////////////////////////////////////////////////////

class DiskComparer
{
public:
    static constexpr int     kMaxEdits        = 64;
    static constexpr double  kTimingTolerance = 0.01;

    //  The comparison's text calls the sides by `names`, or by
    //  GetSideNames without them.
    static DiskComparison  Compare (const DiskAnalysis & a, const DiskAnalysis & b, const SideNames * names = nullptr);

    //  Each side's file name, with the nearest folder where the two paths
    //  differ when the file names match.
    static SideNames  GetSideNames (const DiskAnalysis & a, const DiskAnalysis & b);
    static SideNames  GetSideNames (const std::filesystem::path & pathA, const std::filesystem::path & pathB);

    //  The listed quarter tracks, and the files, compared again over an
    //  earlier result (FR-122).
    static void  Recompare (const DiskAnalysis & a, const DiskAnalysis & b, const std::set<int> & quarterTracks, DiskComparison & inOutComparison,
                            const SideNames * names = nullptr);

    //  The rotation, in nibbles, that best lines B's nibbles up with A's.
    static int  Align (const TrackAnalysis & a, const TrackAnalysis & b);

    //  The fewest edits that turn one nibble run into the other, as hunks of
    //  A and B. A changed nibble is one edit, so it pairs with the nibble at
    //  its place on the other side, and B may drift up to kMaxEdits nibbles
    //  from A.
    struct Hunk
    {
        int  firstA = 0;
        int  countA = 0;
        int  firstB = 0;
        int  countB = 0;
    };

    static bool  Diff (std::span<const Byte> a, std::span<const Byte> b, vector<Hunk> & outHunks);

    //  The cells of A's run and B's run aligned bit for bit, with kBitMargin
    //  cells of each track either side: the first and last cells that
    //  differ on each side and how many bits change. Runs longer than
    //  kMaxBitRun are not compared, and their bitsChanged is -1.
    static constexpr int  kBitMargin = 16;
    static constexpr int  kMaxBitRun = 4096;

    static void  FindBits (std::span<const Byte> cellsA, uint32_t startA, uint32_t lengthA, std::span<const Byte> cellsB, uint32_t startB, uint32_t lengthB,
                           Difference & inOut);

    //  The nibble hunks that differ on one track once aligned, for the views
    //  of the selected track whatever its verdict.
    static void  ListNibbles (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs);

    //  B's sector paired with A's, by sector number and encoding with repeats
    //  in passing order (FR-120), or -1.
    static int  FindPairedSector (const TrackAnalysis & a, int sectorIndex, const TrackAnalysis & b);

private:
    static bool      IsSectorImage       (const DiskAnalysis & disk);
    static bool      IsStandardLayout    (const DiskAnalysis & disk, int quarterTrack);
    static void      CompareQuarterTrack (const DiskAnalysis & a, const DiskAnalysis & b, int qt, bool isSectorsOnly, TrackComparison & t, vector<Difference> & inOutDiffs);
    static void      CompareTrack        (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, TrackComparison & inOut, vector<Difference> & inOutDiffs);
    static void      CompareSectorData   (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, TrackComparison & inOut, vector<Difference> & inOutDiffs);
    static bool      IsSameCells         (const TrackAnalysis & a, const TrackAnalysis & b, int & outRotation, bool & outIsTimingSame);
    static bool      IsSameNibbles       (const TrackAnalysis & a, const TrackAnalysis & b);
    static int       CompareSectors      (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, vector<Difference> & inOutDiffs);
    static uint32_t  GetSectorCell       (const TrackAnalysis & track, const AnalyzedSector & sector);
    static void      CountSectorBits     (const TrackAnalysis & a, const vector<Difference> & nibbleDiffs, Difference & inOut);
    static void      ListTiming          (const TrackAnalysis & a, const TrackAnalysis & b, int quarterTrack, int rotation, vector<Difference> & inOutDiffs);
    static void      CompareFiles        (const DiskAnalysis & a, const DiskAnalysis & b, DiskComparison & inOut);
    static bool            ReadContents     (const FileMap & map, const SectorSource & source, const MappedFile & file, vector<Byte> & outBytes,
                                             vector<std::pair<size_t, int>> * outStarts = nullptr);
    static vector<int>   GetDifferingCells (const vector<std::pair<size_t, int>> & starts, const vector<Byte> & a, const vector<Byte> & b);
    static bool          ReadCell          (const FileMap & map, const SectorSource & source, int cell, vector<Byte> & inOut);
    static vector<Byte>  GetNibbles        (const TrackAnalysis & track, int rotation);
    static uint32_t      GetCost           (const vector<uint16_t> & cost, int width, int lo, int i, int j);
    static uint32_t      GetNibbleCell     (const TrackAnalysis & track, int nibble);
    static bool          HasSectors        (const TrackAnalysis & track) { return !track.sectors.empty(); }
};
