#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Ui/DiskInspector/InspectorTables.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSourceKind / ComparisonSource
//
//  Where one side of a comparison comes from (FR-117): a drive's disk as it
//  is now, that disk as it was inserted or last reloaded, that disk's file
//  as saved now, or an image file.
//
////////////////////////////////////////////////////////////////////////////////

enum class ComparisonSourceKind
{
    DriveNow,
    AsInserted,
    ItsFile,
    ImageFile,
};


struct ComparisonSource
{
    ComparisonSourceKind  kind  = ComparisonSourceKind::DriveNow;
    int                   drive = 0;
    std::wstring          path;

    bool  operator== (const ComparisonSource &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText
//
//  The words of a comparison, kept out of the views so they can be tested:
//  each track's verdict for the Tracks tab's Comparison column (FR-118), the
//  result chip (FR-121), and the Differences tab's rows, in the disk's order
//  until a column sorts them, under a toggle per group of differences that
//  filters them as the Findings tab's categories do.
//
////////////////////////////////////////////////////////////////////////////////

class ComparisonText
{
public:
    enum Group
    {
        kGroupTracks,
        kGroupNibbles,
        kGroupTiming,
        kGroupSectors,
        kGroupVolumes,
        kGroupFiles,
        kGroupCount,
    };

    static std::wstring  FormatVerdict (const TrackComparison & track);
    static std::wstring  FormatResult  (const DiskComparison & comparison);
    static std::wstring  FormatKind    (DifferenceKind kind);
    static std::wstring  FormatDetail  (const Difference & difference);
    static std::wstring  FormatSource  (const ComparisonSource & source);
    static std::wstring  FormatGroup   (int group);
    static int           GetGroup      (DifferenceKind kind);
    static uint32_t      GetAllGroups  ();

    static vector<std::wstring>  GetColumns ();
    static vector<TableRow>      BuildRows  (const vector<Difference> & listed, uint32_t groupMask, int sortColumn, bool isDescending);

    static std::array<int, kGroupCount>  CountGroups (const vector<Difference> & listed);

    //  The Tracks tab with a Comparison column after its first: each quarter
    //  track's verdict, or "Comparing" while a comparison is under way.
    static void  AddVerdictColumn (vector<std::wstring> & inOutColumns, vector<TableRow> & inOutRows, const DiskComparison & comparison, bool isComparing);

    //  Whether A's nibble lies in a hunk that differs, with the nibble of B
    //  aligned with it, or -1 where B has none.
    static bool  FindInHunk (const vector<Difference> & hunks, int nibbleA, int & outNibbleB);

private:
    static std::wstring  FormatNibbles (int first, int count);
};
