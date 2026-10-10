#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SummaryChip / HeaderItem
//
//  One chip of the disk summary beside the file name, and one labeled value
//  of the Sector data tab's header.
//
////////////////////////////////////////////////////////////////////////////////

enum class SummaryChipKind
{
    Format,
    TracksWithData,
    SectorsGood,
    BadSectors,
    Nonstandard,
    Unformatted,
    Flux,
    Damaged,
    Volume,
};


struct SummaryChip
{
    SummaryChipKind  kind  = SummaryChipKind::Format;
    std::wstring     text;
    bool             isBad = false;
};


struct HeaderItem
{
    std::wstring  label;
    std::wstring  value;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorText
//
//  The words the inspector's views show, kept out of the views so they can
//  be tested: the summary chips (FR-018), the track header (FR-032), the
//  sector row and its tooltips (FR-039), the Sector data header and its
//  empty states (FR-040), and the platter's tooltip (FR-027).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorText
{
public:
    static vector<SummaryChip>  BuildChips          (const DiskSummary & summary);
    static std::wstring         FormatDiskFormat    (DiskFormatClass format);
    static std::wstring         FormatOrdinal       (int n);
    static std::wstring         FormatCount         (int count, LPCWSTR singular, LPCWSTR plural);

    static std::wstring         FormatTrackTitle    (int quarterTrack);
    static std::wstring         FormatTrackClass    (TrackClass trackClass);
    static std::wstring         FormatTrackLine     (const TrackAnalysis * track);
    static std::wstring         FormatMeasureLine   (const TrackAnalysis & track);

    static std::wstring         FormatSectorTooltip (const AnalyzedSector & sector, const TrackAnalysis & track);
    static std::wstring         FormatNoSectors     (const QuarterTrackEntry & entry);
    static vector<HeaderItem>   BuildSectorHeader   (const AnalyzedSector & sector, const TrackAnalysis & track);
    static std::wstring         FormatNoSectorData  (const QuarterTrackEntry & entry, const TrackAnalysis * track);

    static std::wstring         FormatNibbleKind    (NibbleKind kind, bool isFailedChecksum);
    static std::wstring         FormatPlatterTooltip (const DiskAnalysis & analysis, int quarterTrack, double turn, bool isShowingNibbles);
    static std::wstring         FormatNibbleTooltip  (const TrackAnalysis & track, int nibble);
    static std::wstring         FormatStripReadout   (double span, uint32_t firstCell, uint32_t lastCell);
};
