#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TableRow
//
//  One row of an inspector table: its cells' text, and what it refers to,
//  so selecting the row can go to it.
//
////////////////////////////////////////////////////////////////////////////////

struct TableRow
{
    vector<std::wstring>  cells;
    int                   quarterTrack = -1;
    int                   sectorIndex  = -1;
    int                   field        = -1;
    int                   finding      = -1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTables
//
//  The rows of the Findings tab (FR-048, FR-049), the Tracks tab (FR-047)
//  and the Fields tab (FR-042), kept out of the views so they can be tested.
//  Findings come in quarter-track and cell order; any column sorts them, and
//  a category filter hides the rest.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorTables
{
public:
    static constexpr int  kCategoryCount = static_cast<int> (FindingCategory::Protection) + 1;

    static vector<std::wstring>  GetFindingColumns ();
    static vector<std::wstring>  GetTrackColumns   ();
    static vector<std::wstring>  GetFieldColumns   ();
    static vector<std::wstring>  GetImageColumns   ();

    static vector<TableRow>  BuildFindings (const DiskAnalysis & analysis, uint32_t categoryMask, int sortColumn, bool isDescending);
    static vector<TableRow>  BuildTracks   (const DiskAnalysis & analysis);
    static vector<TableRow>  BuildFields   (const DiskAnalysis & analysis, int quarterTrack);
    static vector<TableRow>  BuildImage    (const ImageDetails & image);

    static std::array<int, kCategoryCount>  CountCategories (const DiskAnalysis & analysis);
    static uint32_t                         GetAllCategories ();
    static bool                             IsAlignmentNoteShown (const DiskAnalysis & analysis);

private:
    static std::wstring  FormatQuarterTrackCell (int quarterTrack);
    static std::wstring  FormatContent          (const QuarterTrackEntry & entry);
    static std::wstring  FormatSharesWith       (const QuarterTrackEntry & entry);
    static std::wstring  FormatChecksum         (Byte stored, Byte computed, bool isGood);
    static std::wstring  FormatUnchecked        (Byte stored, Byte computed);
    static std::wstring  FormatMarks            (std::span<const Byte> found);
    static bool          IsLess                 (const std::wstring & a, const std::wstring & b);
    static bool          IsNumberChar           (wchar_t ch);
    static bool          TryParseNumber         (const std::wstring & s, double & outValue);

    static void          AddInfoRows       (vector<TableRow> & inOut, const WozInfo & info);
    static void          AddMetaRows       (vector<TableRow> & inOut, const WozFileLayout & layout);
    static void          AddMapRows        (vector<TableRow> & inOut, LPCWSTR title, const std::array<Byte, WozFileLayout::kMapEntries> & map);
    static void          AddRecordRows     (vector<TableRow> & inOut, const WozFileLayout & layout);
    static void          AddChunkRows      (vector<TableRow> & inOut, const WozFileLayout & layout);
    static std::wstring  FormatImageFormat (const ImageDetails & image);
    static void          AddRow            (vector<TableRow> & inOut, const std::wstring & item, const std::wstring & value);
};
