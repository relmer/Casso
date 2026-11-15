#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Finding
//
//  One irregular thing the disk inspector found, with where it is. Findings
//  about what a track records are worded neutrally; findings about the image
//  file call it damaged only when a record cannot be read, a map entry is out
//  of range or the checksum does not match. FindingFormatter writes the text.
//
////////////////////////////////////////////////////////////////////////////////

enum class FindingCategory
{
    Track,
    Field,
    Sector,
    QuarterTrack,
    ImageFile,
    FileSystem,
    Protection,
};


enum class FindingKind
{
    AddressChecksumFailed,
    DataChecksumFailed,
    AddressTrackDiffers,
    VolumeDiffers,
    SectorRepeated,
    SectorsMissing,
    SectorNumberOutOfRange,
    AddressEpilogueNonstandard,
    DataEpilogueNonstandard,
    AddressWithoutData,
    DataWithoutAddress,
    NibblesOutsideTable,
    StrayFieldMark,
    ThirteenSectorFields,
    TrackLengthDiffers,
    RandomBitsOnFormattedTrack,
    RecordBetweenTracks,
    RecordWithoutWholeTrack,
    RecordAcrossWholeTracks,
    DamagedRecord,
    DamagedMapEntry,
    ChecksumMismatch,
    LargestTrackTooSmall,
    MetaValueOutsideList,
    ImageDateNotRfc3339,
    DuplicateChunk,
    ChunkOutOfOrder,
    DataPastLastChunk,
    UnreferencedRecord,
    MapEntryToEmptyRecord,
    NibTrackWithoutSync,

    //  File system (FR-093); each carries its sentence in the detail.
    FileCrossLinked,
    FileAllocatedUnowned,
    FileOwnedMarkedFree,
    FileChainBroken,
    FileTouchesBadSectors,
    FileMapUnchecked,
    FileCountDiffers,
    PascalEntryOutOfOrder,
    PascalEntryOutOfRange,
    PascalFileCountDiffers,
    CpmInvalidEntry,
    CpmDuplicateEntry,

    //  The DOS 3.3 checks of the VTOC's header and the catalog's entries,
    //  and the checks of each file's bytes.
    Dos33HeaderLost,
    Dos33SectorBytesDiffer,
    Dos33PairsDiffer,
    Dos33NextTrackOffDisk,
    Dos33ReleaseDiffers,
    Dos33VolumeOutOfRange,
    Dos33BitmapPadding,
    Dos33EntryNameMixed,
    Dos33EntryTypeUnknown,
    Dos33EntryCountTooLarge,
    Dos33CatalogNonstandard,
    Dos33StoredLengthDiffers,
    Dos33ListOffsetDiffers,
    ProDosFileCountDiffers,
    FileRepeatedByte,
    ImageAlmostBlank,
    CpmExtentMissing,
    CpmRecordsPastBlocks,
    CpmBlockInDirectory,
    CpmBlocksPastRecords,
    FileUsesStructure,
    Dos33StructureMarkedFree,
    ProDosDirectoryPointer,
    ProDosParentPointer,
    ProDosBitmapPastEnd,
    ProDosEofPastStorage,
    ProDosNameInvalid,
    PascalLastBytesInvalid,
    PascalNameInvalid,
};


//  How much a file-system finding weighs in the verdict: damage makes the
//  volume Bad, a suspect finding makes it Suspect, and a note doesn't
//  change it.
enum class FindingWeight
{
    Note,
    Suspect,
    Damage,
};


struct Finding
{
    FindingCategory             category     = FindingCategory::Track;
    FindingKind                 kind         = FindingKind::AddressChecksumFailed;
    FindingWeight               weight       = FindingWeight::Note;
    int                         quarterTrack = -1;
    int                         slot         = -1;
    int                         sector       = -1;
    int                         field        = -1;
    uint32_t                    cell         = 0;
    bool                        hasCell      = false;
    int                         mapCell      = -1;      // a file-system finding's sector or block in its file map
    int                         value        = 0;
    int                         value2       = 0;
    vector<int>                 quarterTracks;
    vector<int>                 sectors;
    vector<InvalidNibbleCount>  nibbles;
    vector<Byte>                bytes;
    std::string                 detail;
};
