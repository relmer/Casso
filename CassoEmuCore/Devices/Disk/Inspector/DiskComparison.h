#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerdict / DifferenceKind
//
//  What comparing a quarter track of A with B found (FR-118), the strongest
//  that applies, and each kind of difference the Differences tab lists
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
//  difference has its place on A and on B, and for nibbles the span of
//  cells whose bits differ, with bitsChanged -1 where the bits were not
//  compared and 0 where only the framing differs; a file pair has the outcome
//  of comparing two files of one path, with A's map cells that have bytes
//  differing from B's.
//
////////////////////////////////////////////////////////////////////////////////

struct ComparisonOptions
{
    bool  isIgnoringSync    = false;
    bool  isIgnoringVolumes = false;
    bool  isIgnoringDates   = false;
};


//  What a comparison's text calls each side: its file name, with the
//  nearest folder where the two paths differ when the file names match
//  (DiskComparer::GetSideNames), and with its source when the paths match
//  too (ComparisonSession).
struct SideNames
{
    std::wstring  a = L"A";
    std::wstring  b = L"B";

    const std::wstring &  Get (int side) const { return (side == 0) ? a : b; }
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
    int           damagedSide     = -1;    // the side whose record is damaged, for "Not compared"
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
    int             firstBitA     = -1;
    int             bitCountA     = 0;
    int             firstBitB     = -1;
    int             bitCountB     = 0;
    int             bitsChanged   = -1;
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

    //  One side is a sector image, which doesn't contain a track layout, so only the
    //  sectors' data is compared.
    bool  isSectorsOnly = false;

    SideNames  names;

    //  The differences the options leave in the Differences tab, in order.
    vector<Difference>  GetListed (const ComparisonOptions & options) const;

    //  Each compared track counted once: identical, differ, only in A, only
    //  in B; Nothing recorded and Standard layout are left out (FR-121).
    void  Count (int & outIdentical, int & outDiffer, int & outOnlyInA, int & outOnlyInB, int & outNotCompared) const;
};
