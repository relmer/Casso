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
};


struct Finding
{
    FindingCategory             category     = FindingCategory::Track;
    FindingKind                 kind         = FindingKind::AddressChecksumFailed;
    int                         quarterTrack = -1;
    int                         slot         = -1;
    int                         sector       = -1;
    int                         field        = -1;
    uint32_t                    cell         = 0;
    bool                        hasCell      = false;
    int                         value        = 0;
    int                         value2       = 0;
    vector<int>                 quarterTracks;
    vector<int>                 sectors;
    vector<InvalidNibbleCount>  nibbles;
    vector<Byte>                bytes;
    std::string                 detail;
};
