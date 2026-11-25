#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/IDiskImage.h"

class BlockInspectorModel;
enum class BlockFileSystem;
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
    Partitions,
    Blocks,
    BlocksUsed,
    Files,
    StampMarks,
    CrossLinked,
    FileSystem,
    ReadAsCpm,
};


struct SummaryChip
{
    SummaryChipKind  kind   = SummaryChipKind::Format;
    std::wstring     text;
    bool             isBad  = false;
    bool             isInfo = false;   // drawn in the accent, as a note of a choice the user made
};


//  A labeled value. The part of the value from badStart for badLength
//  characters is drawn in the error color, where the value is unusual.
struct HeaderItem
{
    std::wstring  label;
    std::wstring  value;
    size_t        badStart  = std::wstring::npos;
    size_t        badLength = 0;

    static HeaderItem  MakeResult (LPCWSTR label, const std::wstring & value);

    //  A check as the sector buttons mark it, the mark before the label: a
    //  check mark when it passed, an X in the error color when it failed, a
    //  question mark when it wasn't checked, and the empty set when the field
    //  is missing. An item without a label is drawn as its value alone.
    static HeaderItem  MakeCheck  (LPCWSTR label, bool isPresent, bool isChecked, bool isGood);
};



//  The Sector data header's first lines (FR-040): where the sector is, its
//  checks, and where the disk's file system puts it.
static constexpr int  kSectorHeaderLines = 3;

using SectorHeader = std::array<vector<HeaderItem>, kSectorHeaderLines>;





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
    static vector<SummaryChip>  BuildBlockChips     (const BlockInspectorModel & model);
    static vector<SummaryChip>  BuildStampChips     (const ImageDetails & image);
    static bool                 HasCustomMark       (const vector<vector<Byte>> & prologs);
    static std::wstring         FormatFileSystem    (BlockFileSystem fileSystem);
    static vector<SummaryChip>  BuildChips          (const DiskSummary & summary, FileSystemVerdict verdict);
    static std::wstring         FormatDiskFormat    (DiskFormatClass format);
    static std::wstring         FormatOrdinal       (int n);
    static std::wstring         FormatCount         (int count, LPCWSTR singular, LPCWSTR plural);

    //  The chip of a disk read as CP/M by the user's choice, and its tip.
    static SummaryChip          MakeReadAsCpmChip   ();

    //  The same chip for the caption, after the title, with the tip.
    static DxuiTitleChip        MakeReadAsCpmTitleChip ();
    static constexpr LPCWSTR    kpszReadAsCpmTip    = L"You chose to read this disk as CP/M. Choose Detect file system in the inspector's Disk menu to undo it.";

    //  The disk inspector's title: "Disk inspector - ", a lock while the disk
    //  is write-protected, the image's path, and "(mounted in ...)" while
    //  the disk is in a drive. outMarkAt is the end of the path while edits
    //  are unsaved, where the title bar draws the unsaved-changes dot, and
    //  npos otherwise.
    static std::wstring         FormatWindowTitle   (const std::wstring & path, bool isWriteProtected, bool hasEdits, const std::wstring & mount, size_t & outMarkAt);

    //  "There are unsaved changes to 3 sectors in GAME.dsk." for the
    //  question before unsaved changes are discarded.
    static std::wstring         FormatUnsavedChanges (int count, LPCWSTR singular, LPCWSTR plural, const std::wstring & fileName);

    static std::wstring         FormatTrackTitle    (int quarterTrack);

    //  Each reason the drive's disk is write-protected, separated by
    //  semicolons, or empty when there is none (FR-065).
    static std::wstring         FormatWriteProtect  (const WriteProtectInfo & info, const std::wstring & program = L"Casso");
    static std::wstring         FormatTrackClass    (TrackClass trackClass);
    static std::wstring         FormatTrackLine     (const TrackAnalysis * track);
    static std::wstring         FormatMeasureLine   (const TrackAnalysis & track);

    static std::wstring         FormatSectorTooltip (const AnalyzedSector & sector, const TrackAnalysis & track);
    static std::wstring         FormatAccessCounts  (uint32_t reads, uint32_t writes, uint32_t scans);
    static std::wstring         FormatSelection     (const TrackAnalysis * track, int quarterTrack, int sectorIndex, int firstNibble, int nibbleCount,
                                                     const FileMap * map);
    static std::wstring         FormatNoSectors     (const QuarterTrackEntry & entry);
    static SectorHeader         BuildSectorHeader   (const AnalyzedSector & sector, const TrackAnalysis & track, MapFileSystem fileSystem);
    static std::wstring         FormatNoSectorData  (const QuarterTrackEntry & entry, const TrackAnalysis * track);

    static std::wstring  FormatNibbleKind   (NibbleKind kind, bool isFailedChecksum);
    static std::wstring  FormatStripReadout (uint32_t firstCell, uint32_t lastCell, const std::wstring & driveState = std::wstring());
    static std::wstring  FormatSelection    (const TrackAnalysis & track, int firstNibble, int nibbleCount);
};
