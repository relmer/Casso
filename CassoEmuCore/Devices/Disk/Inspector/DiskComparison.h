#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerdict / DifferenceKind
//
//  What comparing a quarter track of A with B found (FR-118), the strongest
//  that holds, and each kind of difference the Differences tab lists
//  (FR-119, FR-120).
//
////////////////////////////////////////////////////////////////////////////////

enum class TrackVerdict
{
    NothingRecorded,
    StandardLayout,
    Identical,
    SameCells,
    SameNibbles,
    SameSectorData,
    SectorsDiffer,
    NibblesDiffer,
    OnlyInA,
    OnlyInB,
    NotCompared,
};


enum class DifferenceKind
{
    TrackOnlyInA,
    TrackOnlyInB,
    Nibbles,
    Timing,
    SectorBytes,
    SectorChecksum,
    SectorOnlyInA,
    SectorOnlyInB,
    Volume,
    FileContents,
    FileLength,
    FileAttributes,
    FileDates,
    FileOnlyInA,
    FileOnlyInB,
    FileNotCompared,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonOptions / TrackComparison / Difference / FilePair / DiskComparison
//
//  The three options change only which differences are listed and the file
//  comparison, never a verdict (FR-120). A track's comparison gives its
//  verdict with the rotation and length change that qualify it; a
//  difference gives its place on A and on B; a file pair gives the outcome
//  of comparing two files of one path, with A's map cells that hold bytes
//  differing from B's.
//
////////////////////////////////////////////////////////////////////////////////

struct ComparisonOptions
{
    bool  isIgnoringSync    = false;
    bool  isIgnoringVolumes = false;
    bool  isIgnoringDates   = false;
};


struct TrackComparison
{
    int           quarterTrack    = 0;
    TrackVerdict  verdict         = TrackVerdict::NothingRecorded;
    int           slotA           = -1;
    int           slotB           = -1;
    int           rotationCells   = 0;
    int           rotationNibbles = 0;
    uint32_t      cellCount       = 0;
    int           lengthChange    = 0;
    int           sectorsDiffer   = 0;
    bool          isTimingDiffer  = false;
    bool          isFluxOnOneSide = false;
    std::wstring  reason;
};


struct Difference
{
    DifferenceKind  kind          = DifferenceKind::Nibbles;
    int             quarterTrack  = -1;
    int             sector        = -1;
    int             firstNibbleA  = -1;
    int             nibbleCountA  = 0;
    int             firstNibbleB  = -1;
    int             nibbleCountB  = 0;
    uint32_t        cell          = 0;
    int             count         = 0;
    int             firstOffset   = -1;
    bool            isSyncOnly    = false;
    std::wstring    path;
    std::wstring    detail;
};


struct FilePair
{
    std::wstring    path;
    int             fileA     = -1;
    int             fileB     = -1;
    DifferenceKind  outcome   = DifferenceKind::FileContents;
    bool            isSame    = false;
    std::wstring    reason;
    vector<int>     differingCells;
};


struct DiskComparison
{
    vector<TrackComparison>  tracks;
    vector<Difference>       differences;
    vector<FilePair>         files;
    std::wstring             fileNote;

    //  The differences the options leave in the Differences tab, in order.
    vector<Difference>  GetListed (const ComparisonOptions & options) const;

    //  Each compared track counted once: identical, differ, only in A, only
    //  in B; Nothing recorded and Standard layout are left out (FR-121).
    void  Count (int & outIdentical, int & outDiffer, int & outOnlyInA, int & outOnlyInB, int & outNotCompared) const;
};
