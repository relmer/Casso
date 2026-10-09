#include "Pch.h"

#include "Ui/DiskInspector/InspectorText.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/PlatterGeometry.h"





static constexpr LPCWSTR  s_kpszSeparator = L" \x00B7 ";     // U+00B7 MIDDLE DOT, between items on a line





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::BuildChips
//
//  In display order; a chip with a count of zero is left out, and so is the
//  volume chip on a disk with no standard sectors (FR-018).
//
////////////////////////////////////////////////////////////////////////////////

vector<SummaryChip> InspectorText::BuildChips (const DiskSummary & summary)
{
    vector<SummaryChip>  chips;
    std::wstring         good;



    chips.push_back ({ SummaryChipKind::Format, FormatDiskFormat (summary.format), false });

    if (summary.tracksWithData > 0)
    {
        chips.push_back ({ SummaryChipKind::TracksWithData, FormatCount (summary.tracksWithData, L"track with data", L"tracks with data"), false });
    }

    if (summary.sectorsFound > 0)
    {
        good = std::format (L"{} of {} sectors good", InspectorFormat::FormatCount (summary.sectorsGood), InspectorFormat::FormatCount (summary.sectorsFound));

        if (summary.sectorsNotChecked > 0)
        {
            good += std::format (L", {} not checked", InspectorFormat::FormatCount (summary.sectorsNotChecked));
        }

        chips.push_back ({ SummaryChipKind::SectorsGood, good, summary.badSectors > 0 });
    }

    if (summary.badSectors > 0)
    {
        chips.push_back ({ SummaryChipKind::BadSectors, FormatCount (summary.badSectors, L"bad sector", L"bad sectors"), true });
    }

    if (summary.nonstandardTracks > 0)
    {
        chips.push_back ({ SummaryChipKind::Nonstandard, FormatCount (summary.nonstandardTracks, L"nonstandard track", L"nonstandard tracks"), false });
    }

    if (summary.unformattedTracks > 0)
    {
        chips.push_back ({ SummaryChipKind::Unformatted, FormatCount (summary.unformattedTracks, L"unformatted track", L"unformatted tracks"), false });
    }

    if (summary.fluxTracks > 0)
    {
        chips.push_back ({ SummaryChipKind::Flux, FormatCount (summary.fluxTracks, L"flux track", L"flux tracks"), false });
    }

    if (summary.damagedTracks > 0)
    {
        chips.push_back ({ SummaryChipKind::Damaged, FormatCount (summary.damagedTracks, L"damaged track", L"damaged tracks"), true });
    }

    if (summary.commonVolume >= 0 && summary.sectorsFound > 0)
    {
        chips.push_back ({ SummaryChipKind::Volume, std::format (L"Volume {}", summary.commonVolume), false });
    }

    return chips;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatDiskFormat
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatDiskFormat (DiskFormatClass format)
{
    std::wstring  text;



    switch (format)
    {
        case DiskFormatClass::NothingRecorded:    text = L"Nothing recorded";   break;
        case DiskFormatClass::ThirteenAndSixteen: text = L"13 and 16 sector";   break;
        case DiskFormatClass::Sixteen:            text = L"16 sector";          break;
        case DiskFormatClass::Thirteen:           text = L"13 sector";          break;
        case DiskFormatClass::Nonstandard:        text = L"Nonstandard";        break;
        case DiskFormatClass::Unformatted:        text = L"Unformatted";        break;
        case DiskFormatClass::Damaged:            text = L"Damaged";            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatOrdinal
//
//  1st, 2nd, 3rd, 4th ... 11th, 12th, 13th ... 21st.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatOrdinal (int n)
{
    static constexpr int  kTens     = 10;
    static constexpr int  kHundreds = 100;



    LPCWSTR  suffix = L"th";
    int      teen   = n % kHundreds;



    if (teen < 11 || teen > 13)
    {
        switch (n % kTens)
        {
            case 1: suffix = L"st"; break;
            case 2: suffix = L"nd"; break;
            case 3: suffix = L"rd"; break;
            default:                break;
        }
    }

    return std::to_wstring (n) + suffix;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatCount
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatCount (int count, LPCWSTR singular, LPCWSTR plural)
{
    return InspectorFormat::FormatCount (static_cast<uint64_t> (count)) + L" " + (count == 1 ? singular : plural);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatTrackTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatTrackTitle (int quarterTrack)
{
    return L"Track " + InspectorFormat::FormatQuarterTrack (quarterTrack);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatTrackClass
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatTrackClass (TrackClass trackClass)
{
    std::wstring  text;



    switch (trackClass)
    {
        case TrackClass::NothingRecorded:    text = L"Nothing recorded"; break;
        case TrackClass::Damaged:            text = L"Damaged";          break;
        case TrackClass::Sixteen:            text = L"16 sector";        break;
        case TrackClass::Thirteen:           text = L"13 sector";        break;
        case TrackClass::ThirteenAndSixteen: text = L"13 and 16 sector"; break;
        case TrackClass::Unformatted:        text = L"Unformatted";      break;
        case TrackClass::Nonstandard:        text = L"Nonstandard";      break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatTrackLine
//
//  "51,007 cells · 6,352 nibbles · 16 sectors, 16 good · flux" (FR-032).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatTrackLine (const TrackAnalysis * track)
{
    std::wstring  text = L"Nothing recorded on this quarter track";



    if (track != nullptr && track->framed.cellCount > 0)
    {
        text = InspectorFormat::FormatCount (track->framed.cellCount) + L" cells" + s_kpszSeparator
             + InspectorFormat::FormatCount (track->framed.nibbles.size()) + L" nibbles" + s_kpszSeparator
             + (track->sectorsFound > 0
                ? std::format (L"{}, {} good", FormatCount (track->sectorsFound, L"sector", L"sectors"), track->sectorsGood)
                : std::wstring (L"no standard sectors"));

        if (track->framed.isFlux)
        {
            text += s_kpszSeparator + std::wstring (L"flux");
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatMeasureLine
//
//  "Turn 199.6 ms (300.6 RPM) · mean cell 3.93µs (+0.5%) · longest sync 40
//  nibbles at cell 12,400", the turn and cell items on flux tracks only.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatMeasureLine (const TrackAnalysis & track)
{
    static constexpr double  kTicksPerMs = 8000.0;



    const TrackMeasurements &  m    = track.measurements;
    vector<std::wstring>       items;
    std::wstring               text;



    if (track.framed.isFlux && m.turnTicks > 0)
    {
        items.push_back (std::format (L"Turn {:.1f} ms ({:.1f} RPM)", m.turnTicks / kTicksPerMs, m.rpm));
        items.push_back (L"mean cell " + InspectorFormat::FormatMicroseconds (m.meanCellTicks) + L" (" + InspectorFormat::FormatPercent (m.deviation) + L")");
    }

    if (m.longestSync.count > 0)
    {
        items.push_back (std::format (L"longest sync {} at cell {}", FormatCount (m.longestSync.count, L"nibble", L"nibbles"),
                                      InspectorFormat::FormatCount (m.longestSync.startCell)));
    }

    for (const std::wstring & item : items)
    {
        text += (text.empty() ? L"" : s_kpszSeparator) + item;
    }

    if (!text.empty() && text[0] >= L'a' && text[0] <= L'z')
    {
        text[0] = static_cast<wchar_t> (text[0] - L'a' + L'A');
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatSectorTooltip
//
//  "Sector $5, 6th past the index", the volume and track fields, and which
//  checksum failed or was not checked (FR-039).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatSectorTooltip (const AnalyzedSector & sector, const TrackAnalysis & track)
{
    const LocatedField &  address = track.fields[sector.addressField];
    std::wstring          text;



    text = std::format (L"Sector {}, {} past the index\nVolume {}, track {}", InspectorFormat::FormatSector (sector.sector),
                        FormatOrdinal (sector.passingIndex + 1), address.volume, address.track);

    switch (sector.state)
    {
        case SectorState::BadAddress:  text += L"\nAddress field checksum failed"; break;
        case SectorState::BadData:     text += L"\nData field checksum failed";    break;
        case SectorState::NoDataField: text += L"\nNo data field";                 break;
        case SectorState::NotChecked:  text += L"\nChecksum not checked";          break;
        default:                                                                    break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatNoSectors
//
//  What the sector row shows in place of buttons.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatNoSectors (const QuarterTrackEntry & entry)
{
    return (entry.content == QuarterTrackContent::Nothing) ? L"No track" : L"None in a standard format";
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::BuildSectorHeader
//
//  The Sector data header (FR-040): the sector, its address fields, both
//  results and the encoding, and on a 16-sector track the DOS 3.3 sector and
//  ProDOS block it holds under the standard skew.
//
////////////////////////////////////////////////////////////////////////////////

vector<HeaderItem> InspectorText::BuildSectorHeader (const AnalyzedSector & sector, const TrackAnalysis & track)
{
    const LocatedField &  address = track.fields[sector.addressField];
    vector<HeaderItem>    items;
    std::wstring          dataResult;



    if (sector.dataField < 0)
    {
        dataResult = L"Missing";
    }
    else if (!sector.isDataCheck)
    {
        dataResult = L"Not checked";
    }
    else
    {
        dataResult = sector.isDataGood ? L"Good" : L"Bad";
    }

    items.push_back ({ L"Sector",      InspectorFormat::FormatSector (sector.sector) });
    items.push_back ({ L"Track field", std::to_wstring (address.track) });
    items.push_back ({ L"Volume",      std::to_wstring (address.volume) });
    items.push_back ({ L"Address",     !sector.isAddressCheck ? L"Not checked" : (sector.isAddressGood ? L"Good" : L"Bad") });
    items.push_back ({ L"Data",        dataResult });
    items.push_back ({ L"Encoding",    sector.kind == DiskFieldKind::Sixteen ? L"6-and-2" : L"5-and-3" });

    if (sector.kind == DiskFieldKind::Sixteen && sector.dos33Logical >= 0)
    {
        items.push_back ({ L"DOS 3.3 sector", InspectorFormat::FormatSector (sector.dos33Logical) });
        items.push_back ({ L"ProDOS block",   std::format (L"{} ({} half)", sector.prodosBlock, sector.prodosHalf == 0 ? L"first" : L"second") });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatNoSectorData
//
//  What the Sector data tab shows when there are no bytes to show (FR-040).
//  A sector with no data field keeps its header; the caller adds this text
//  after it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatNoSectorData (const QuarterTrackEntry & entry, const TrackAnalysis * track)
{
    std::wstring  text;



    if (entry.content == QuarterTrackContent::Damaged)
    {
        text = L"This quarter track is damaged: " + FindingFormatter::FormatDamageReason (entry.damageReason) + L".";
    }
    else if (entry.content == QuarterTrackContent::Nothing || track == nullptr)
    {
        text = L"Nothing recorded on this quarter track.";
    }
    else if (track->sectors.empty())
    {
        text = L"No standard sectors on this track. The Nibbles tab shows what is recorded on it.";
    }
    else
    {
        text = L"This sector has no data field.";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatNibbleKind
//
//  A failed address field is an address field whose checksum failed, never
//  a data field (FR-023).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatNibbleKind (NibbleKind kind, bool isFailedChecksum)
{
    std::wstring  text;



    switch (kind)
    {
        case NibbleKind::Sync:            text = L"Sync";             break;
        case NibbleKind::AddressPrologue: text = L"Address prologue"; break;
        case NibbleKind::AddressField:    text = L"Address field";    break;
        case NibbleKind::AddressEpilogue: text = L"Address epilogue"; break;
        case NibbleKind::DataPrologue:    text = L"Data prologue";    break;
        case NibbleKind::DataField:       text = L"Data field";       break;
        case NibbleKind::DataEpilogue:    text = L"Data epilogue";    break;
        case NibbleKind::Other:           text = L"Other";            break;
        case NibbleKind::Noise:           text = L"Noise";            break;
    }

    if (isFailedChecksum)
    {
        text += L", checksum failed";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatPlatterTooltip
//
//  The track, the kind and sector under the pointer, the ring's sectors good
//  out of found, and on flux the cell time against nominal; once nibble
//  values show, the nibble's value, its offset and "cell N of M". An empty
//  ring says the drive reads random bits there (FR-027).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatPlatterTooltip (const DiskAnalysis & analysis, int quarterTrack, double turn, bool isShowingNibbles)
{
    const QuarterTrackEntry  & entry    = analysis.entries[quarterTrack];
    const TrackAnalysis      * track    = (entry.slot >= 0 && entry.slot < static_cast<int> (analysis.tracks.size())) ? analysis.tracks[entry.slot].get() : nullptr;
    std::wstring               text     = FormatTrackTitle (quarterTrack);
    uint32_t                   cell     = 0;
    int                        nibble   = -1;
    int                        sector   = -1;
    bool                       isRandom = false;



    if (entry.content == QuarterTrackContent::Damaged)
    {
        text += L"\nDamaged: " + FindingFormatter::FormatDamageReason (entry.damageReason);
    }
    else if (track == nullptr || track->framed.cellCount == 0)
    {
        text += L"\nNothing recorded\nThe drive reads random bits here";
    }
    else
    {
        cell     = PlatterGeometry::GetCellAtTurn (*track, turn);
        nibble   = PlatterGeometry::GetNibbleAt (*track, cell);
        sector   = PlatterGeometry::GetSectorAt (*track, cell);
        isRandom = cell < track->framed.isRandomCell.size() && track->framed.isRandomCell[cell] != 0;

        if (isRandom)
        {
            text += L"\nRandom bits";
        }
        else if (nibble >= 0)
        {
            text += L"\n" + FormatNibbleKind (track->nibbleKinds[nibble], nibble < static_cast<int> (track->isFailedChecksum.size()) && track->isFailedChecksum[nibble] != 0);
        }

        if (sector >= 0)
        {
            text += s_kpszSeparator + std::wstring (L"Sector ") + InspectorFormat::FormatSector (track->sectors[sector].sector);
        }

        if (track->sectorsFound > 0)
        {
            text += std::format (L"\n{} of {} sectors good", track->sectorsGood, track->sectorsFound);
        }

        if (track->framed.isFlux && cell < track->framed.cellTicks.size())
        {
            text += L"\n" + InspectorFormat::FormatMicroseconds (track->framed.cellTicks[cell]) + L" cells ("
                  + InspectorFormat::FormatPercent (track->framed.cellTicks[cell] / TrackAnalyzer::kNominalCellTicks - 1.0) + L")";
        }

        if (isShowingNibbles && nibble >= 0 && !isRandom)
        {
            text += std::format (L"\nNibble {:02X} at offset {}{}cell {} of {}", track->framed.nibbles[nibble].value, InspectorFormat::FormatHexOffset (nibble),
                                 s_kpszSeparator, InspectorFormat::FormatCount (cell + 1), InspectorFormat::FormatCount (track->framed.cellCount));
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText::FormatNibbleTooltip
//
//  A nibble's value, its cell and width in cells, its kind and sector, and
//  on flux its mean cell time (FR-037). Extra zero cells after a nibble
//  count in its width.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorText::FormatNibbleTooltip (const TrackAnalysis & track, int nibble)
{
    const FramedTrack &   framed = track.framed;
    const FramedNibble &  n      = framed.nibbles[nibble];
    uint32_t              next   = framed.nibbles[(nibble + 1) % framed.nibbles.size()].startCell;
    uint32_t              width  = (next + framed.cellCount - n.startCell) % std::max<uint32_t> (framed.cellCount, 1);
    int                   sector = PlatterGeometry::GetSectorAt (track, n.startCell);
    double                ticks  = 0;
    uint32_t              k      = 0;
    std::wstring          text;



    width = (width == 0) ? framed.cellCount : width;
    text  = std::format (L"{:02X} at offset {}\nCell {}, {} wide\n", n.value, InspectorFormat::FormatHexOffset (nibble),
                         InspectorFormat::FormatCount (n.startCell), FormatCount (static_cast<int> (width), L"cell", L"cells"))
          + FormatNibbleKind (track.nibbleKinds[nibble], nibble < static_cast<int> (track.isFailedChecksum.size()) && track.isFailedChecksum[nibble] != 0);

    if (sector >= 0)
    {
        text += s_kpszSeparator + std::wstring (L"Sector ") + InspectorFormat::FormatSector (track.sectors[sector].sector);
    }

    if (framed.isFlux && framed.cellTicks.size() >= framed.cellCount && width > 0)
    {
        for (k = 0; k < width; k++)
        {
            ticks += framed.cellTicks[(n.startCell + k) % framed.cellCount];
        }

        text += L"\n" + InspectorFormat::FormatMicroseconds (ticks / width) + L" cells ("
              + InspectorFormat::FormatPercent (ticks / width / TrackAnalyzer::kNominalCellTicks - 1.0) + L")";
    }

    return text;
}
