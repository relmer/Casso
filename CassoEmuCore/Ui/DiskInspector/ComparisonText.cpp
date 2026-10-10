#include "Pch.h"

#include "Ui/DiskInspector/ComparisonText.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr LPCWSTR  s_kpszNone      = s_kpszEmDash;
static constexpr LPCWSTR  s_kpszComparing = L"Comparing";

enum DifferenceColumn
{
    kColumnTrack,
    kColumnSector,
    kColumnCell,
    kColumnKind,
    kColumnDetail,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatVerdict
//
//  The verdict with what qualifies it: the rotation of the same cells and
//  what is known of their timing, the rotation and length change of the
//  same nibbles, the count of sectors that differ, or why a track was not
//  compared (FR-118).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatVerdict (const TrackComparison & track)
{
    std::wstring  text;



    switch (track.verdict)
    {
        case TrackVerdict::NothingRecorded: text = L"Nothing recorded"; break;
        case TrackVerdict::StandardLayout:  text = L"Standard layout";  break;
        case TrackVerdict::Identical:       text = L"Identical";        break;
        case TrackVerdict::SameSectorData:  text = L"Same sector data"; break;
        case TrackVerdict::NibblesDiffer:   text = L"Nibbles differ";   break;
        case TrackVerdict::OnlyInA:         text = L"Only in A";        break;
        case TrackVerdict::OnlyInB:         text = L"Only in B";        break;

        case TrackVerdict::SameCells:
            text = L"Same cells";

            if (track.rotationCells != 0 && track.cellCount > 0)
            {
                text += L", rotated " + InspectorFormat::FormatDegrees (static_cast<double> (track.rotationCells) / track.cellCount);
            }

            text += track.isFluxOnOneSide ? L", only one side records flux timing" : (track.isTimingDiffer ? L", flux timing differs" : L"");
            break;

        case TrackVerdict::SameNibbles:
            text = L"Same nibbles";

            if (track.rotationNibbles != 0)
            {
                text += std::format (L", rotated {} nibbles", track.rotationNibbles);
            }

            if (track.lengthChange != 0)
            {
                text += std::format (L", B {} cells {}", InspectorFormat::FormatCount (static_cast<uint64_t> (std::abs (track.lengthChange))),
                                     track.lengthChange > 0 ? L"longer" : L"shorter");
            }

            break;

        case TrackVerdict::SectorsDiffer:
            text = std::format (L"Sectors differ ({})", track.sectorsDiffer);
            break;

        case TrackVerdict::NotCompared:
            text = track.reason.empty() ? std::wstring (L"Not compared") : L"Not compared: " + track.reason;
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatResult
//
//  "31 identical · 3 differ · 1 only in B" (FR-121): each count that is not
//  zero, with tracks that show "Nothing recorded" or "Standard layout" left
//  out.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatResult (const DiskComparison & comparison)
{
    int                        identical   = 0;
    int                        differ      = 0;
    int                        onlyInA     = 0;
    int                        onlyInB     = 0;
    int                        notCompared = 0;
    vector<std::wstring>       parts;
    std::wstring               text;



    comparison.Count (identical, differ, onlyInA, onlyInB, notCompared);

    for (const auto & [count, label] : { std::pair { identical, L"identical" }, std::pair { differ, L"differ" }, std::pair { onlyInA, L"only in A" },
                                         std::pair { onlyInB, L"only in B" }, std::pair { notCompared, L"not compared" } })
    {
        if (count > 0)
        {
            parts.push_back (std::format (L"{} {}", count, label));
        }
    }

    for (size_t i = 0; i < parts.size(); i++)
    {
        text += (i > 0) ? std::format (L" {} {}", s_kpszMiddleDot, parts[i]) : parts[i];
    }

    return text.empty() ? std::wstring (L"Nothing to compare") : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatKind
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatKind (DifferenceKind kind)
{
    LPCWSTR  text = L"";



    switch (kind)
    {
        case DifferenceKind::TrackOnlyInA:    text = L"Track only in A";    break;
        case DifferenceKind::TrackOnlyInB:    text = L"Track only in B";    break;
        case DifferenceKind::Nibbles:         text = L"Nibbles";            break;
        case DifferenceKind::Timing:          text = L"Flux timing";        break;
        case DifferenceKind::SectorBytes:     text = L"Sector data";        break;
        case DifferenceKind::SectorChecksum:  text = L"Checksum results";   break;
        case DifferenceKind::SectorOnlyInA:   text = L"Sector only in A";   break;
        case DifferenceKind::SectorOnlyInB:   text = L"Sector only in B";   break;
        case DifferenceKind::Volume:          text = L"Volume number";      break;
        case DifferenceKind::FileContents:    text = L"File contents";      break;
        case DifferenceKind::FileLength:      text = L"File length";        break;
        case DifferenceKind::FileAttributes:  text = L"File attributes";    break;
        case DifferenceKind::FileDates:       text = L"File dates";         break;
        case DifferenceKind::FileOnlyInA:     text = L"File only in A";     break;
        case DifferenceKind::FileOnlyInB:     text = L"File only in B";     break;
        case DifferenceKind::FileNotCompared: text = L"File not compared";  break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatDetail
//
//  What the kind leaves to say: the nibbles on each side, the cells whose
//  timing differs, the bytes of a sector or file that differ and the first
//  of them, the change in a file's length, or the file and the reason.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatDetail (const Difference & difference)
{
    const Difference &  d    = difference;
    std::wstring        text;



    switch (d.kind)
    {
        case DifferenceKind::Nibbles:
            text = std::format (L"A {}, B {}{}", FormatNibbles (d.firstNibbleA, d.nibbleCountA), FormatNibbles (d.firstNibbleB, d.nibbleCountB),
                                d.isSyncOnly ? L", sync only" : L"");
            break;

        case DifferenceKind::Timing:
            text = std::format (L"{} {} differ by more than {:.0f}%", InspectorFormat::FormatCount (static_cast<uint64_t> (d.count)), d.count == 1 ? L"cell" : L"cells",
                                DiskComparer::kTimingTolerance * 100.0);
            break;

        case DifferenceKind::SectorBytes:
            text = std::format (L"{} {} differ, the first at {}", d.count, d.count == 1 ? L"byte" : L"bytes", InspectorFormat::FormatHexOffset (d.firstOffset));
            break;

        case DifferenceKind::FileContents:
            text = std::format (L"{}: {} {} differ, the first at offset {}", d.path, InspectorFormat::FormatCount (static_cast<uint64_t> (d.count)),
                                d.count == 1 ? L"byte" : L"bytes", InspectorFormat::FormatCount (static_cast<uint64_t> (d.firstOffset)));
            break;

        case DifferenceKind::FileLength:
            text = std::format (L"{}: B is {} {} {}", d.path, InspectorFormat::FormatCount (static_cast<uint64_t> (std::abs (d.count))),
                                std::abs (d.count) == 1 ? L"byte" : L"bytes", d.count > 0 ? L"longer" : L"shorter");
            break;

        case DifferenceKind::FileNotCompared:
            text = d.path + L": " + d.detail;
            break;

        case DifferenceKind::FileAttributes:
        case DifferenceKind::FileDates:
        case DifferenceKind::FileOnlyInA:
        case DifferenceKind::FileOnlyInB:
            text = d.path;
            break;

        default:
            text = d.detail;
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatSource
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatSource (const ComparisonSource & source)
{
    std::wstring  text;



    switch (source.kind)
    {
        case ComparisonSourceKind::DriveNow:   text = std::format (L"Drive {} now", source.drive + 1);         break;
        case ComparisonSourceKind::AsInserted: text = std::format (L"Drive {} as inserted", source.drive + 1); break;
        case ComparisonSourceKind::ItsFile:    text = std::format (L"Drive {}'s file", source.drive + 1);      break;
        case ComparisonSourceKind::ImageFile:  text = L"Image file";                                          break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatGroup
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatGroup (int group)
{
    static constexpr LPCWSTR  s_kpszGroups[kGroupCount] = { L"Tracks", L"Nibbles", L"Timing", L"Sectors", L"Volumes", L"Files" };



    return (group >= 0 && group < kGroupCount) ? s_kpszGroups[group] : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::GetGroup
//
////////////////////////////////////////////////////////////////////////////////

int ComparisonText::GetGroup (DifferenceKind kind)
{
    int  group = kGroupFiles;



    switch (kind)
    {
        case DifferenceKind::TrackOnlyInA:
        case DifferenceKind::TrackOnlyInB:   group = kGroupTracks;  break;
        case DifferenceKind::Nibbles:        group = kGroupNibbles; break;
        case DifferenceKind::Timing:         group = kGroupTiming;  break;
        case DifferenceKind::SectorBytes:
        case DifferenceKind::SectorChecksum:
        case DifferenceKind::SectorOnlyInA:
        case DifferenceKind::SectorOnlyInB:  group = kGroupSectors; break;
        case DifferenceKind::Volume:         group = kGroupVolumes; break;
        default:                                                    break;
    }

    return group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::GetAllGroups
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ComparisonText::GetAllGroups()
{
    return (1u << kGroupCount) - 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::GetColumns
//
////////////////////////////////////////////////////////////////////////////////

vector<std::wstring> ComparisonText::GetColumns()
{
    return { L"Track", L"Sector", L"Cell", L"Kind", L"Details" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::BuildRows
//
//  In the order the comparison lists them, quarter track then cell and the
//  files last, unless a column sorts them; a difference whose group is not
//  in the mask is left out. Each row refers to its difference by index.
//
////////////////////////////////////////////////////////////////////////////////

vector<TableRow> ComparisonText::BuildRows (const vector<Difference> & listed, uint32_t groupMask, int sortColumn, bool isDescending)
{
    vector<TableRow>  rows;
    size_t            i       = 0;
    bool              hasCell = false;



    for (i = 0; i < listed.size(); i++)
    {
        const Difference &  d   = listed[i];
        TableRow            row;

        if ((groupMask & (1u << GetGroup (d.kind))) == 0)
        {
            continue;
        }

        hasCell          = d.quarterTrack >= 0 && d.kind != DifferenceKind::TrackOnlyInA && d.kind != DifferenceKind::TrackOnlyInB && d.kind != DifferenceKind::Volume;
        row.quarterTrack = d.quarterTrack;
        row.finding      = static_cast<int> (i);
        row.cells        = { d.quarterTrack >= 0 ? InspectorFormat::FormatQuarterTrack (d.quarterTrack) : std::wstring (s_kpszNone),
                             d.sector >= 0       ? InspectorFormat::FormatSector (d.sector)             : std::wstring (s_kpszNone),
                             hasCell             ? InspectorFormat::FormatCount (d.cell)                : std::wstring (s_kpszNone),
                             FormatKind (d.kind),
                             FormatDetail (d) };

        rows.push_back (std::move (row));
    }

    if (sortColumn >= kColumnTrack && sortColumn <= kColumnDetail)
    {
        std::stable_sort (rows.begin(), rows.end(), [sortColumn, isDescending] (const TableRow & a, const TableRow & b)
        {
            return isDescending ? InspectorTables::IsLess (b.cells[sortColumn], a.cells[sortColumn]) : InspectorTables::IsLess (a.cells[sortColumn], b.cells[sortColumn]);
        });
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::CountGroups
//
////////////////////////////////////////////////////////////////////////////////

std::array<int, ComparisonText::kGroupCount> ComparisonText::CountGroups (const vector<Difference> & listed)
{
    std::array<int, kGroupCount>  counts = {};



    for (const Difference & d : listed)
    {
        counts[GetGroup (d.kind)]++;
    }

    return counts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::AddVerdictColumn
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonText::AddVerdictColumn (vector<std::wstring> & inOutColumns, vector<TableRow> & inOutRows, const DiskComparison & comparison, bool isComparing)
{
    int  qt = 0;



    inOutColumns.insert (inOutColumns.begin() + 1, L"Comparison");

    for (TableRow & row : inOutRows)
    {
        qt = row.quarterTrack;

        if (isComparing || qt < 0 || qt >= static_cast<int> (comparison.tracks.size()))
        {
            row.cells.insert (row.cells.begin() + 1, s_kpszComparing);
        }
        else
        {
            row.cells.insert (row.cells.begin() + 1, FormatVerdict (comparison.tracks[qt]));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonText::FormatNibbles
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ComparisonText::FormatNibbles (int first, int count)
{
    std::wstring  text = L"no nibbles";



    if (count == 1)
    {
        text = std::format (L"nibble {}", InspectorFormat::FormatCount (static_cast<uint64_t> (first)));
    }
    else if (count > 1)
    {
        text = std::format (L"nibbles {} to {}", InspectorFormat::FormatCount (static_cast<uint64_t> (first)), InspectorFormat::FormatCount (static_cast<uint64_t> (first + count - 1)));
    }

    return text;
}
