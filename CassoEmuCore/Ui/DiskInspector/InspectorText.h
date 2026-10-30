#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/IDiskImage.h"

struct FileMap;





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
//  sector row and its tooltips (FR-039), and the Sector data header and its
//  empty states (FR-040).
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

    //  Each reason the drive's disk is write-protected, separated by
    //  semicolons, or empty when there is none (FR-065).
    static std::wstring         FormatWriteProtect  (const WriteProtectInfo & info);
    static std::wstring         FormatTrackClass    (TrackClass trackClass);
    static std::wstring         FormatTrackLine     (const TrackAnalysis * track);
    static std::wstring         FormatMeasureLine   (const TrackAnalysis & track);

    static std::wstring         FormatSectorTooltip (const AnalyzedSector & sector, const TrackAnalysis & track);
    static std::wstring         FormatAccessCounts  (uint32_t reads, uint32_t writes, uint32_t scans);
    static std::wstring         FormatSelection     (const TrackAnalysis * track, int quarterTrack, int sectorIndex, int firstNibble, int nibbleCount,
                                                     const FileMap * map);
    static std::wstring         FormatNoSectors     (const QuarterTrackEntry & entry);
    static vector<HeaderItem>   BuildSectorHeader   (const AnalyzedSector & sector, const TrackAnalysis & track);
    static std::wstring         FormatNoSectorData  (const QuarterTrackEntry & entry, const TrackAnalysis * track);

    static std::wstring         FormatNibbleKind    (NibbleKind kind, bool isFailedChecksum);
    static std::wstring         FormatStripReadout   (uint32_t firstCell, uint32_t lastCell, const std::wstring & driveState = std::wstring());
    static std::wstring         FormatSelection      (const TrackAnalysis & track, int firstNibble, int nibbleCount);
};
