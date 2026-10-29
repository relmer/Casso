#include "Pch.h"

#include "Ui/DiskInspector/InspectorTip.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/PlatterGeometry.h"
#include "Ui/DiskInspector/SectorAccess.h"





static const std::wstring  s_kSeparator   = std::wstring (L" ") + s_kpszMiddleDot + L" ";
static constexpr double    s_kHitReachPx = 5.0;





////////////////////////////////////////////////////////////////////////////////
//
//  KindShare
//
//  One kind of cell in a spot, its color, and how many of the spot's cells
//  it covers.
//
////////////////////////////////////////////////////////////////////////////////

struct KindShare
{
    std::wstring  label;
    uint32_t      argb  = 0;
    uint32_t      cells = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IsTimed
//
//  A flux track with a time for every cell.
//
////////////////////////////////////////////////////////////////////////////////

static bool IsTimed (const TrackAnalysis & track)
{
    return track.framed.isFlux && track.framed.cellCount > 0 && track.framed.cellTicks.size() >= track.framed.cellCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddLine
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddLine (const std::wstring & line)
{
    m_text += (m_lines == 0) ? line : L"\n" + line;
    m_lines++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddColor
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddColor (uint32_t argb, const std::wstring & line)
{
    m_swatches.push_back ({ m_lines, argb });
    AddLine (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::BuildPlatterTip
//
//  The track and sector, the ring's sectors good out of found, once nibble
//  values are drawn the nibble's value and its cell, then the colors of the
//  spot. A damaged or empty ring has a color line of its own, and an empty
//  ring a line on the random bits the drive reads there (FR-027).
//
////////////////////////////////////////////////////////////////////////////////

InspectorTip InspectorTip::BuildPlatterTip (const InspectorViewContext & context, int quarterTrack, double turn, double turnSpan, bool isShowingNibbles)
{
    const DiskAnalysis       & analysis   = *context.analysis;
    const QuarterTrackEntry  & entry      = analysis.entries[quarterTrack];
    int                        wholeTrack = quarterTrack / DiskImage::kQuarterTracksPerWholeTrack;
    int                        fileSlot   = analysis.entries[wholeTrack * DiskImage::kQuarterTracksPerWholeTrack].slot;
    const TrackAnalysis      * track      = (entry.slot >= 0 && entry.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[entry.slot].get() : nullptr;
    const TrackAnalysis      * fileTrack  = (fileSlot >= 0 && fileSlot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[fileSlot].get() : nullptr;
    std::wstring               title      = InspectorText::FormatTrackTitle (quarterTrack);
    InspectorTip               tip;
    uint32_t                   cell       = 0;
    uint32_t                   first      = 0;
    uint32_t                   last       = 0;
    uint32_t                   count      = 0;
    int                        nibble     = -1;
    int                        sector     = -1;
    bool                       isRandom   = false;
    bool                       isTimed    = false;



    if (entry.content == QuarterTrackContent::Damaged)
    {
        tip.AddLine  (title);
        tip.AddColor (context.palette.colors.damaged, L"Damaged: " + FindingFormatter::FormatDamageReason (entry.damageReason));
    }
    else if (track == nullptr || track->framed.cellCount == 0)
    {
        tip.AddLine  (title);
        tip.AddColor (context.palette.colors.nothingRecorded, L"Nothing recorded");
        tip.AddLine  (L"The drive reads random bits here");
    }
    else
    {
        cell     = PlatterGeometry::GetCellAtTurn (*track, turn);
        nibble   = PlatterGeometry::GetNibbleAt (*track, cell);
        sector   = PlatterGeometry::GetSectorAt (*track, cell);
        isRandom = cell < track->framed.isRandomCell.size() && track->framed.isRandomCell[cell] != 0;
        isTimed  = context.isTimingMode && IsTimed (*track);
        first    = PlatterGeometry::GetCellAtTurn (*track, turn - turnSpan / 2 - std::floor (turn - turnSpan / 2));
        last     = PlatterGeometry::GetCellAtTurn (*track, turn + turnSpan / 2 - std::floor (turn + turnSpan / 2));
        count    = (turnSpan >= 1.0) ? track->framed.cellCount : (last + track->framed.cellCount - first) % track->framed.cellCount + 1;

        tip.AddLine ((sector >= 0) ? title + s_kSeparator + L"Sector " + InspectorFormat::FormatSector (track->sectors[sector].sector) : title);

        if (track->sectorsFound > 0)
        {
            tip.AddLine (std::format (L"{} of {} sectors good", track->sectorsGood, track->sectorsFound));
        }

        if (sector >= 0)
        {
            tip.AddAccess (context, entry.slot, *track, sector);
        }

        if (isShowingNibbles && nibble >= 0 && !isRandom)
        {
            tip.AddLine (std::format (L"Nibble {:02X} at offset {}{}cell {} of {}", track->framed.nibbles[nibble].value, InspectorFormat::FormatHexOffset (nibble),
                                      s_kSeparator, InspectorFormat::FormatCount (cell + 1), InspectorFormat::FormatCount (track->framed.cellCount)));
        }

        tip.AddKinds (context, *track, first, count, !isTimed);

        if (IsTimed (*track))
        {
            tip.AddTiming (context, *track, first, count, isTimed);
        }

        if (fileTrack != nullptr && fileTrack->framed.cellCount > 0)
        {
            tip.AddFileRole (context, wholeTrack, *fileTrack, PlatterGeometry::GetNibbleAt (*fileTrack, PlatterGeometry::GetCellAtTurn (*fileTrack, turn)));
        }

        //  A hit's mark on the platter is a few pixels long however short the
        //  hit, starting at the hit, so a hit up to that far before the spot
        //  counts.
        first = PlatterGeometry::GetCellAtTurn (*track, turn - turnSpan * s_kHitReachPx - std::floor (turn - turnSpan * s_kHitReachPx));
        tip.AddFindHit (context, *track, first, (turnSpan >= 1.0) ? track->framed.cellCount : (last + track->framed.cellCount - first) % track->framed.cellCount + 1);
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::BuildNibbleTip
//
//  A nibble's value and sector, its cell and its own cells (its eight bits
//  and any extra zero cells), the colors of the spot, on flux its mean cell
//  time (FR-037), the random-bit cells after it, and its sector's role in
//  the Files overlay.
//
////////////////////////////////////////////////////////////////////////////////

InspectorTip InspectorTip::BuildNibbleTip (const InspectorViewContext & context, const TrackAnalysis & track, int nibble, uint32_t firstCell, uint32_t cellCount,
                                           bool isTimingMode)
{
    static constexpr uint32_t  kBitsCells = 8;



    const FramedTrack   & framed   = track.framed;
    const FramedNibble  & n        = framed.nibbles[nibble];
    uint32_t              next     = framed.nibbles[(nibble + 1) % framed.nibbles.size()].startCell;
    uint32_t              width    = (next + framed.cellCount - n.startCell) % std::max<uint32_t> (framed.cellCount, 1);
    uint32_t              own      = kBitsCells + n.extraZeroCells;
    int                   sector   = PlatterGeometry::GetSectorAt (track, n.startCell);
    int                   qt       = (context.model != nullptr) ? context.model->GetQuarterTrack() : -1;
    bool                  isTimed  = isTimingMode && IsTimed (track);
    std::wstring          line;
    InspectorTip          tip;



    width = (width == 0) ? framed.cellCount : width;
    own   = std::min (own, width);
    line  = std::format (L"{:02X} at offset {}", n.value, InspectorFormat::FormatHexOffset (nibble));

    if (sector >= 0)
    {
        line += s_kSeparator + L"Sector " + InspectorFormat::FormatSector (track.sectors[sector].sector);
    }

    tip.AddLine (line);

    //  A nibble's own cells: the "+N" (or for a sync nibble, the width) in
    //  the Nibbles tab.
    line = std::format (L"Cell {}: ", InspectorFormat::FormatCount (n.startCell));
    line += (n.extraZeroCells == 0) ? InspectorText::FormatCount (static_cast<int> (own), L"cell", L"cells")
                                    : std::format (L"{}: 8 bits and {}", InspectorText::FormatCount (static_cast<int> (own), L"cell", L"cells"),
                                                   InspectorText::FormatCount (n.extraZeroCells, L"extra zero cell", L"extra zero cells"));
    tip.AddLine (line);

    tip.AddKinds (context, track, firstCell, cellCount, !isTimed);

    if (IsTimed (track))
    {
        tip.AddTiming (context, track, n.startCell, width, isTimed);
    }

    //  Cells with no flux transition for too long, which the drive reads as
    //  random bits rather than as part of this nibble.
    if (width > own)
    {
        tip.AddLine (std::format (L"Then {} with no flux transitions, which the drive reads as random bits",
                                  InspectorText::FormatCount (static_cast<int> (width - own), L"cell", L"cells")));
    }

    //  The Files overlay colors whole tracks only.
    if (qt >= 0 && qt % DiskImage::kQuarterTracksPerWholeTrack == 0)
    {
        tip.AddFileRole (context, qt / DiskImage::kQuarterTracksPerWholeTrack, track, nibble);
    }

    tip.AddFindHit (context, track, firstCell, cellCount);

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddAccess
//
//  How many times the guest read, wrote and scanned a sector since the disk
//  was inserted, summed over the quarter tracks that share its record, as
//  the heat maps count them (FR-161). No line when all three are 0.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddAccess (const InspectorViewContext & context, int slot, const TrackAnalysis & track, int sector)
{
    std::array<uint32_t, 3>  counts = {};
    std::wstring             line;



    for (int qt = 0; context.drive != nullptr && context.drive->access != nullptr && qt < DiskImage::kQuarterTrackCount; qt++)
    {
        if (context.analysis->entries[static_cast<size_t> (qt)].slot == slot)
        {
            counts[0] += SectorAccess::CountReads  (track, context.drive->access->reads[static_cast<size_t> (qt)])[static_cast<size_t> (sector)];
            counts[1] += SectorAccess::CountWrites (track, context.drive->access->writes[static_cast<size_t> (qt)])[static_cast<size_t> (sector)];
            counts[2] += SectorAccess::CountScans  (track, context.drive->access->scans[static_cast<size_t> (qt)])[static_cast<size_t> (sector)];
        }
    }

    for (size_t k = 0; k < counts.size(); k++)
    {
        if (counts[k] > 0)
        {
            line += (line.empty() ? L"" : s_kSeparator) + std::format (L"{} {} {}", (k == 0) ? L"Read" : (k == 1) ? L"Written" : L"Scanned",
                                                                        counts[k], counts[k] == 1 ? L"time" : L"times");
        }
    }

    if (!line.empty())
    {
        AddLine (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddKinds
//
//  Each kind of cell among `cellCount` cells from `firstCell`, most cells
//  first: random bits, or a nibble's kind or failed checksum. Colored, each
//  takes a line and its swatch; otherwise, as in Timing mode where timing
//  colors the spot, the kinds share one plain line.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddKinds (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount, bool isColored)
{
    const FramedTrack  & framed = track.framed;
    vector<KindShare>    shares;
    std::wstring         label;
    std::wstring         joined;
    uint32_t             argb   = 0;
    uint32_t             cell   = 0;
    int                  nibble = -1;
    bool                 failed = false;



    cellCount = std::min (std::max<uint32_t> (cellCount, 1), framed.cellCount);

    for (uint32_t i = 0; i < cellCount; i++)
    {
        cell = (firstCell + i) % framed.cellCount;

        if (cell < framed.isRandomCell.size() && framed.isRandomCell[cell] != 0)
        {
            label = L"Random bits";
            argb  = context.palette.GetKindColor (PlatterKind::RandomBits);
        }
        else
        {
            nibble = PlatterGeometry::GetNibbleAt (track, cell);

            if (nibble < 0)
            {
                continue;
            }

            failed = nibble < static_cast<int> (track.isFailedChecksum.size()) && track.isFailedChecksum[nibble] != 0;
            label  = InspectorText::FormatNibbleKind (track.nibbleKinds[nibble], failed);
            argb   = context.palette.GetNibbleColor (track, nibble, false, context.timingRange);
        }

        auto  it = std::find_if (shares.begin(), shares.end(), [&] (const KindShare & s) { return s.label == label; });

        if (it == shares.end())
        {
            shares.push_back ({ label, argb, 1 });
        }
        else
        {
            it->cells++;
        }
    }

    std::stable_sort (shares.begin(), shares.end(), [] (const KindShare & a, const KindShare & b) { return a.cells > b.cells; });

    for (const KindShare & share : shares)
    {
        if (isColored)
        {
            AddColor (share.argb, share.label);
        }
        else
        {
            joined += (joined.empty() ? L"" : L", ") + share.label;
        }
    }

    if (!joined.empty())
    {
        AddLine (joined);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddTiming
//
//  The mean time of `cellCount` cells from `firstCell` against nominal, with
//  its timing color when timing colors the spot.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddTiming (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount, bool isColored)
{
    const FramedTrack  & framed = track.framed;
    double               ticks  = 0;
    double               mean   = 0;
    std::wstring         line;



    cellCount = std::min (std::max<uint32_t> (cellCount, 1), framed.cellCount);

    for (uint32_t i = 0; i < cellCount; i++)
    {
        ticks += framed.cellTicks[(firstCell + i) % framed.cellCount];
    }

    mean = ticks / cellCount;
    line = InspectorFormat::FormatMicroseconds (mean) + L" cells (" + InspectorFormat::FormatPercent (mean / TrackAnalyzer::kNominalCellTicks - 1.0) + L")";

    if (isColored)
    {
        AddColor (context.palette.GetTimingColor (mean / TrackAnalyzer::kNominalCellTicks - 1.0, context.timingRange), line);
    }
    else
    {
        AddLine (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddFindHit
//
//  A line with the find hit color while the spot's nibbles include a hit
//  (FR-056).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddFindHit (const InspectorViewContext & context, const TrackAnalysis & track, uint32_t firstCell, uint32_t cellCount)
{
    int  size  = static_cast<int> (track.framed.nibbles.size());
    int  first = 0;
    int  last  = 0;



    if (context.findMarks != nullptr && size > 0 && cellCount > 0 && track.framed.cellCount > 0)
    {
        first = PlatterGeometry::GetNibbleAt (track, firstCell % track.framed.cellCount);
        last  = PlatterGeometry::GetNibbleAt (track, (firstCell + cellCount - 1) % track.framed.cellCount);

        if (first >= 0 && last >= 0 && context.findMarks->IsRangeMarked (&track, first, (last - first + size) % size + 1))
        {
            AddColor (context.palette.colors.findHit, context.findMarks->FormatHitLine (1));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTip::AddFileRole
//
//  While the Files overlay is on, the role and owners of the sector whose
//  data field contains the nibble, in the overlay's color for that role, as
//  "File data: HELLO, data sector 3 of 5".
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTip::AddFileRole (const InspectorViewContext & context, int wholeTrack, const TrackAnalysis & track, int nibble)
{
    const FileMap *  map  = context.fileMap;
    int              cell = -1;
    int              half = 0;
    bool             isOn = context.isFilesOverlay && map != nullptr && map->notMapped == NotMappedReason::None && wholeTrack < map->tracks && nibble >= 0;



    for (const AnalyzedSector & sector : isOn ? track.sectors : vector<AnalyzedSector>())
    {
        if (cell < 0 && map != nullptr && sector.dataField >= 0 && nibble >= track.fields[sector.dataField].firstNibble &&
            nibble < track.fields[sector.dataField].firstNibble + track.fields[sector.dataField].nibbleCount)
        {
            cell = map->GetCellOf (wholeTrack, sector.sector, half);
        }
    }

    if (cell >= 0)
    {
        const MapCell &  at = map->cells[cell];

        AddColor (context.palette.GetRoleColor (static_cast<MapRole> (at.role)),
                  FileMapText::FormatRole (map->fileSystem, at.role) + (at.owners.empty() ? L"" : L": " + FileMapText::FormatOwners (*map, cell)));
    }
}
