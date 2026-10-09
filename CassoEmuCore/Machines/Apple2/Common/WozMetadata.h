#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunk
//
//  One WOZ chunk Casso does not model, kept exactly as it was read so a
//  rewrite can put it back byte for byte. The id is the raw 4-byte chunk
//  tag rather than a string so an unrecognized tag needs no interpretation
//  to survive a round trip.
//
////////////////////////////////////////////////////////////////////////////////

struct WozChunk
{
    Byte          id[4] = {};
    vector<Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozInfo
//
//  Every INFO field of the file's version, read once by WozLoader::ReadInfo.
//  Version 2 added sides through largest track, version 3 the two flux
//  fields; a field the file's version does not have is reported absent, not
//  as the value the format gives "unknown".
//
////////////////////////////////////////////////////////////////////////////////

struct WozInfo
{
    bool         isPresent          = false;
    Byte         version            = 0;
    Byte         diskType           = 0;
    bool         isWriteProtected   = false;
    bool         isSynchronized     = false;
    bool         isCleaned          = false;
    std::string  creator;
    bool         hasVersion2Fields  = false;
    Byte         sides              = 0;
    Byte         bootSectorFormat   = 0;
    Byte         optimalBitTiming   = 0;
    uint16_t     compatibleHardware = 0;
    uint16_t     requiredRamK       = 0;
    uint16_t     largestTrack       = 0;
    bool         hasVersion3Fields  = false;
    uint16_t     fluxBlock          = 0;
    uint16_t     largestFluxTrack   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozTrackRecordFields / WozUnreferencedRecord / WozFileLayout
//
//  The file's own track maps and track record table, as read. The track model
//  resolves each quarter track to one slot, which loses what the file said:
//  a bit record that a FLUX entry overrides on the same quarter track, map
//  entries that point at an empty record, and records no map refers to. A
//  save writes the maps from here wherever the track model has not changed
//  them, so those survive, and the disk inspector shows the file from here.
//
////////////////////////////////////////////////////////////////////////////////

struct WozTrackRecordFields
{
    uint16_t  startBlock     = 0;
    uint16_t  blockCount     = 0;
    uint32_t  bitOrByteCount = 0;
};


struct WozChunkEntry
{
    std::array<Byte, 4>  id     = {};
    size_t               offset = 0;
    uint32_t             size   = 0;
};


struct WozMetaEntry
{
    std::string  key;
    std::string  value;
};


struct WozUnreferencedRecord
{
    int           index          = 0;
    uint32_t      bitOrByteCount = 0;
    vector<Byte>  bytes;
};


struct WozFileLayout
{
    static constexpr int   kMapEntries = 160;
    static constexpr Byte  kNoTrack    = 0xFF;

    bool                           isV2                 = false;
    bool                           hasMaps              = false;
    bool                           hasFluxMap           = false;
    std::array<Byte, kMapEntries>  tmap                 = {};
    std::array<Byte, kMapEntries>  flux                 = {};
    vector<WozTrackRecordFields>   records;
    vector<WozUnreferencedRecord>  unreferenced;
    vector<WozChunkEntry>          chunks;
    vector<WozMetaEntry>           metaEntries;
    uint32_t                       storedCrc            = 0;
    uint32_t                       computedCrc          = 0;
    bool                           hasDataPastLastChunk = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozMetadata
//
//  What a WOZ file carries that Casso's track model cannot express, held
//  alongside the tracks so a flush rebuilds the file instead of degrading
//  it. The writer reconstructs INFO, TMAP and TRKS from the live model --
//  that is what makes guest writes survive -- and everything else in the
//  file exists only here.
//
//      infoPayload   the source INFO chunk verbatim. Casso owns four of
//                    its fields (version, write-protect, disk type and
//                    largest track); the other fifty-odd bytes -- creator,
//                    synchronized, cleaned, boot sector format, timing,
//                    compatible hardware, required RAM -- are the source's
//                    and are re-emitted untouched.
//      layout        the maps and record table as the file holds them.
//      passThrough   every other chunk, in source order. META is the one
//                    that matters today; a later format revision's chunks
//                    round-trip through the same path without Casso
//                    learning anything about them.
//
//  An empty infoPayload means the image was synthesized rather than read
//  from a file, which is also what distinguishes a disk Casso authored
//  from one it merely edited.
//
////////////////////////////////////////////////////////////////////////////////

struct WozMetadata
{
    vector<Byte>      infoPayload;
    vector<WozChunk>  passThrough;
    WozFileLayout     layout;
    WozInfo           info;

    bool  IsFromSourceFile () const { return !infoPayload.empty(); }

    void  Clear ()
    {
        infoPayload.clear();
        passThrough.clear();
        layout = WozFileLayout();
        info   = WozInfo();
    }
};
