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
    static constexpr size_t kHeaderSize       = 12;     // 8-byte sig + 4-byte CRC
    static constexpr size_t kInfoChunkSize    = 60;
    static constexpr size_t kTmapChunkSize    = 160;
    static constexpr size_t kV1TrackRecordSize = 6656;
    static constexpr size_t kV2TrkRecordSize  = 8;
    static constexpr size_t kV2BlockSize      = 512;
    static constexpr size_t kV2TrkRecordCount = 160;

    static HRESULT  Load (const vector<Byte> & raw, DiskImage & out);

    static HRESULT  BuildSyntheticV2 (
        Byte                  diskType,
        bool                  writeProtected,
        const vector<Byte> &  trackZeroBitStream,
        size_t                trackZeroBitCount,
        vector<Byte>       &  outBytes);
};
