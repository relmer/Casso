#include "Pch.h"

#include "Ui/DiskInspector/NibbleSections.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/InspectorTables.h"





static const std::wstring  s_kSeparator = std::wstring (L" ") + s_kpszMiddleDot + L" ";





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::Build
//
//  The fields in track order, with the nibbles before, between and after
//  them split into sync runs and other stretches.
//
////////////////////////////////////////////////////////////////////////////////

vector<NibbleSection> NibbleSections::Build (const DiskAnalysis & analysis, int quarterTrack)
{
    const QuarterTrackEntry &  entry    = analysis.entries[static_cast<size_t> (quarterTrack)];
    const TrackAnalysis *      track    = (entry.slot >= 0 && entry.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[static_cast<size_t> (entry.slot)].get() : nullptr;
    vector<NibbleSection>      sections;
    vector<int>                order;
    int                        at       = 0;



    if (track != nullptr)
    {
        for (int f = 0; f < static_cast<int> (track->fields.size()); f++)
        {
            order.push_back (f);
        }

        std::sort (order.begin(), order.end(), [track] (int a, int b) { return track->fields[static_cast<size_t> (a)].firstNibble < track->fields[static_cast<size_t> (b)].firstNibble; });

        for (int f : order)
        {
            const LocatedField &  field = track->fields[static_cast<size_t> (f)];

            if (field.firstNibble >= at)
            {
                AddGap   (*track, at, field.firstNibble, sections);
                AddField (analysis, quarterTrack, *track, f, sections);
                at = field.firstNibble + field.nibbleCount;
            }
        }

        AddGap (*track, at, static_cast<int> (track->framed.nibbles.size()), sections);
    }

    return sections;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::AddGap
//
//  The nibbles from first up to end that lie in no field: each sync run in
//  them, and the stretches around the runs. A run that wraps past the
//  track's last nibble continues at its first.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleSections::AddGap (const TrackAnalysis & track, int first, int end, vector<NibbleSection> & inOut)
{
    int                              total = static_cast<int> (track.framed.nibbles.size());
    int                              at    = first;
    vector<std::pair<int, int>>      runs;



    for (const SyncRun & run : track.measurements.syncRuns)
    {
        runs.push_back ({ run.firstNibble, std::min (run.firstNibble + run.count, total) });

        if (run.firstNibble + run.count > total)
        {
            runs.push_back ({ 0, run.firstNibble + run.count - total });
        }
    }

    std::sort (runs.begin(), runs.end());

    for (const auto & [runStart, runStop] : runs)
    {
        int  runFirst = std::max (runStart, at);
        int  runEnd   = std::min (runStop, end);

        if (runFirst < runEnd)
        {
            AddStretch (track, NibbleSectionKind::Other, at, runFirst, inOut);
            AddStretch (track, NibbleSectionKind::Sync, runFirst, runEnd, inOut);
            at = runEnd;
        }
    }

    AddStretch (track, NibbleSectionKind::Other, at, end, inOut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::AddStretch
//
//  A sync run or other stretch: "20 self-sync FF · cells 0-199".
//
////////////////////////////////////////////////////////////////////////////////

void NibbleSections::AddStretch (const TrackAnalysis & track, NibbleSectionKind kind, int first, int end, vector<NibbleSection> & inOut)
{
    NibbleSection  section;
    uint32_t       lastCell = GetCellAt (track, end);



    if (first < end)
    {
        section.kind        = kind;
        section.firstNibble = first;
        section.count       = end - first;
        section.title       = (kind == NibbleSectionKind::Sync) ? L"Sync" : L"Other nibbles";
        section.facts       = std::format (L"{} {}{}cells {}-{}", InspectorFormat::FormatCount (static_cast<uint64_t> (section.count)),
                                           (kind == NibbleSectionKind::Sync) ? L"self-sync FF" : (section.count == 1 ? L"nibble" : L"nibbles"), s_kSeparator,
                                           InspectorFormat::FormatCount (GetCellAt (track, first)), InspectorFormat::FormatCount (lastCell > 0 ? lastCell - 1 : 0));
        inOut.push_back (std::move (section));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::AddField
//
//  An address or data field with its sector, the facts its header has, and
//  every column of its Fields row for the tooltip (FR-042).
//
////////////////////////////////////////////////////////////////////////////////

void NibbleSections::AddField (const DiskAnalysis & analysis, int quarterTrack, const TrackAnalysis & track, int field, vector<NibbleSection> & inOut)
{
    const LocatedField &  f        = track.fields[static_cast<size_t> (field)];
    const LocatedField *  addr     = (f.role == FieldRole::Address) ? &f : (f.pairedField >= 0 ? &track.fields[static_cast<size_t> (f.pairedField)] : nullptr);
    DecodeChecks          checks   = analysis.settings.GetChecksForTrack (track.context.physicalTrack);
    bool                  isAddr   = f.role == FieldRole::Address;
    bool                  isCheck  = isAddr ? checks.isAddressChecksumOn : checks.isDataChecksumOn;
    bool                  isGood   = isAddr ? f.isAddressChecksumGood : f.data.isChecksumGood;
    vector<std::wstring>  columns  = InspectorTables::GetFieldColumns();
    NibbleSection         section;



    section.kind        = isAddr ? NibbleSectionKind::Address : NibbleSectionKind::Data;
    section.firstNibble = f.firstNibble;
    section.count       = f.nibbleCount;
    section.field       = field;
    section.title       = std::format (L"{} field{}{}", isAddr ? L"Address" : L"Data", s_kSeparator,
                                       addr != nullptr ? L"sector " + InspectorFormat::FormatSector (addr->sector) : std::wstring (L"no address field"));

    for (int s = 0; s < static_cast<int> (track.sectors.size()); s++)
    {
        section.sectorIndex = (track.sectors[static_cast<size_t> (s)].addressField == field || track.sectors[static_cast<size_t> (s)].dataField == field) ? s : section.sectorIndex;
    }

    if (isAddr)
    {
        section.facts = std::format (L"volume {}{}track {}{}checksum {}{}cell {}{}{}", f.volume, s_kSeparator, f.track, s_kSeparator,
                                     !isCheck ? L"not checked" : isGood ? L"good" : L"bad", s_kSeparator, InspectorFormat::FormatCount (f.startCell), s_kSeparator,
                                     InspectorFormat::FormatDegrees (TrackAnalyzer::GetAngle (track, f.startCell)));
    }
    else
    {
        section.facts = std::format (L"{}{}checksum {}{}{} nibbles", f.kind == DiskFieldKind::Sixteen ? L"6-and-2" : L"5-and-3", s_kSeparator,
                                     !isCheck ? L"not checked" : isGood ? L"good" : L"bad", s_kSeparator, InspectorFormat::FormatCount (static_cast<uint64_t> (f.nibbleCount)));
    }

    //  The tooltip has every column of the field's row in the Fields table.
    for (const TableRow & row : InspectorTables::BuildFields (analysis, quarterTrack))
    {
        bool  isRow = row.field == field || (f.pairedField >= 0 && row.field == f.pairedField);

        for (size_t c = 0; isRow && c < row.cells.size() && c < columns.size(); c++)
        {
            section.details += (row.cells[c].empty() ? std::wstring() : columns[c] + L": " + row.cells[c] + L"\n");
        }
    }

    if (!section.details.empty())
    {
        section.details.pop_back();
    }

    inOut.push_back (std::move (section));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::GetCellAt
//
//  The cell a nibble starts at, or the track's length past its last nibble.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t NibbleSections::GetCellAt (const TrackAnalysis & track, int nibble)
{
    return (nibble < static_cast<int> (track.framed.nibbles.size())) ? track.framed.nibbles[static_cast<size_t> (nibble)].startCell : track.measurements.lengthCells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections::FormatHeader
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NibbleSections::FormatHeader (const NibbleSection & section)
{
    return section.title + L"\t" + section.facts;
}
