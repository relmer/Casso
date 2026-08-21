#pragma once

#include "Pch.h"

#include "DiskImage.h"





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

    // Blocks 0..2 hold the header and the INFO / TMAP / TRKS chunks; every
    // per-track bit stream lives at block 3 or later.
    static constexpr uint16_t  kV2FirstDataBlock = 3;

    static HRESULT  Load (const vector<Byte> & raw, DiskImage & out);

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
};
