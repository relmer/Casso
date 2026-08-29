#pragma once

#include "Pch.h"

#include "DiskImage.h"
#include "MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoader
//
//  Native nibble-level loader for the WOZ disk-image format (v1 + v2)
//  per https://applesaucefdc.com/woz/reference2/. Parses the chunked
//  layout (INFO, TMAP, TRKS, optional META) and populates the target
//  DiskImage's per-track bit streams directly. WOZ images are already
//  stored at nibble level — no NibblizationLayer pass is needed.
//
//  Test fixtures use synthetic WOZ images built in memory to avoid any
//  third-party content dependencies.
//
////////////////////////////////////////////////////////////////////////////////

class WozLoader
{
public:
    static constexpr size_t  kHeaderSize        = 12;   // 8-byte sig + 4-byte CRC
    static constexpr size_t  kInfoChunkSize     = 60;
    static constexpr size_t  kTmapChunkSize     = 160;
    static constexpr size_t  kV1TrackRecordSize = 6656;
    static constexpr size_t  kV2TrkRecordSize   = 8;
    static constexpr size_t  kV2BlockSize       = 512;
    static constexpr size_t  kV2TrkRecordCount  = 160;

    //
    //  What a WOZ image says about ITSELF, as opposed to what its tracks hold.
    //
    //  INFO's fields and META's key/value pairs describe the disk and the
    //  software on it -- who made the image, which drive it belongs in, whether
    //  it is write-protected, and, for a commercially pressed disk, its title,
    //  publisher and the machine it wants. None of that reaches DiskImage,
    //  which models the surface and not the paperwork, so it is read on demand
    //  rather than retained.
    //
    struct MetaField
    {
        std::string  key;

        //  UTF-8, which is what the format stores. A consumer putting this in
        //  front of a person has to say so at its own output boundary.
        std::string  value;
    };

    struct Description
    {
        //  False when the bytes are not a WOZ at all, or are too damaged to
        //  walk. Every other field is meaningless in that case.
        bool                    isWoz               = false;

        int                     wozVersion          = 0;
        int                     infoVersion         = 0;
        Byte                    diskType            = 0;
        bool                    writeProtected      = false;
        bool                    synchronized        = false;
        bool                    cleaned             = false;

        //  INFO version 1 stops before this field, so its absence is recorded
        //  rather than reported as the "unknown" value -- the two mean
        //  different things and a reader must not be told the image answered.
        bool                    hasBootSectorFormat = false;
        Byte                    bootSectorFormat    = 0;

        std::string             creator;
        std::vector<MetaField>  meta;

        //  How much of the surface carries data. The head steps in quarter
        //  tracks, so a disk formatted on half or quarter tracks -- which is a
        //  copy protection, not a defect -- shows more positions than slots.
        int                     quarterTracksWithData = 0;
        int                     trackSlotsWithData    = 0;
    };

    // Blocks 0..2 hold the header and the INFO / TMAP / TRKS chunks; every
    // per-track bit stream lives at block 3 or later.
    static constexpr uint16_t  kV2FirstDataBlock = 3;
    static HRESULT  Load (const vector<Byte> & raw, DiskImage & out);

    //  Why a Load of these bytes was refused: not a WOZ at all, or a WOZ whose
    //  chunks do not hold together. Only meaningful after Load has failed --
    //  it re-reads the header rather than remembering anything, so asking it
    //  about bytes that loaded fine gets an answer about a failure that never
    //  happened.
    static MountFailure  ClassifyLoadFailure (const vector<Byte> & raw);

    //  Reads INFO, TMAP and META without loading any track data.
    //
    //  It walks the same chunk table Load does, and stops where Load stops --
    //  at the first identifier that is not a chunk, which in a v2 file is the
    //  bit-stream blocks after TRKS. A separate parser would be a second place
    //  for the layout constants and the version quirks to be got wrong.
    static void  Describe (const vector<Byte> & raw, Description & out);

    // Serialize a DiskImage back to a WOZ v2 byte image (INFO + TMAP +
    // TRKS + block-aligned bit streams + retained chunks, with a valid
    // header CRC32). The per-track bit streams come straight from the
    // image's live buffers, so guest writes round-trip; the write-protect
    // flag is preserved. Always emits v2 regardless of the source variant.
    // INFO fields Casso does not model, and every chunk it does not parse
    // (META above all), are re-emitted verbatim from the image's retained
    // WozMetadata rather than synthesized.
    static HRESULT  Serialize (const DiskImage & img, vector<Byte> & outBytes);

    // Set the write-protect flag in a WOZ file's own bytes, in place.
    // Touches exactly the INFO flag byte and the four header CRC bytes; every
    // other byte of the file is left alone, including the chunks Casso does
    // not model. That is the point: flipping this flag has to reach the file,
    // and rebuilding the file from the track model to carry one bit is how a
    // single menu click came to rewrite an entire preservation dump.
    //
    // Works on v1 and v2 alike and does NOT upgrade the container -- this is
    // an edit of one field, not a rewrite. Fails without modifying anything
    // if the bytes are not a WOZ or carry no usable INFO chunk.
    static HRESULT  SetWriteProtectFlag (vector<Byte> & fileBytes, bool writeProtected);

    static HRESULT  BuildSyntheticV2 (
        Byte                  diskType,
        bool                  writeProtected,
        const vector<Byte> &  trackZeroBitStream,
        size_t                trackZeroBitCount,
        vector<Byte>       &  outBytes);

    //  INFO chunk field offsets, from the payload's first byte. Named here
    //  because Describe and Serialize both address them and a second set of
    //  numbers is a second chance to be wrong.
    static constexpr size_t  kInfoOffsetVersion          = 0;
    static constexpr size_t  kInfoOffsetDiskType         = 1;
    static constexpr size_t  kInfoOffsetWriteProtected   = 2;
    static constexpr size_t  kInfoOffsetSynchronized     = 3;
    static constexpr size_t  kInfoOffsetCleaned          = 4;
    static constexpr size_t  kInfoOffsetCreator          = 5;
    static constexpr size_t  kInfoCreatorLength          = 32;
    static constexpr size_t  kInfoOffsetBootSectorFormat = 38;

    static constexpr Byte    kDiskType525 = 1;
    static constexpr Byte    kDiskType35  = 2;

    static constexpr Byte    kBootSectorUnknown  = 0;
    static constexpr Byte    kBootSector16       = 1;
    static constexpr Byte    kBootSector13       = 2;
    static constexpr Byte    kBootSectorBoth     = 3;

private:
    //  A fixed-width, space-padded field as a string with the padding removed.
    //  INFO stores its creator that way.
    static std::string  ReadPaddedField (const Byte * bytes, size_t length);

    //  META's tab-separated key/value lines. Values are left exactly as stored
    //  -- what a key MEANS is the caller's business, and this only reads.
    static void  ParseMetaChunk (const Byte              *  bytes,
                                 size_t                     length,
                                 std::vector<MetaField>  &  out);
};
