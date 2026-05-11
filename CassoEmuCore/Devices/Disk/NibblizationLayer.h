#pragma once

#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationLayer
//
//  Static helpers that convert between flat sector images (.dsk / .do /
//  .po — 143360 bytes) and packed nibble bit streams suitable for the
//  Disk II nibble engine.
//
//  Encoding: standard Apple DOS 3.3 6+2 GCR per Sather UTAIIe Ch. 9.
//      Address field: $D5 $AA $96 ... $DE $AA $EB
//      Data    field: $D5 $AA $AD ... $DE $AA $EB
//      4-and-4 encoded volume / track / sector / checksum
//      6-and-2 encoded 256-byte sector → 342 nibble bytes + checksum
//
//  Format differences:
//      .dsk / .do — DOS 3.3 logical sector order (identical layout)
//      .po        — ProDOS order; sectors remapped via po_to_dos table
//
////////////////////////////////////////////////////////////////////////////////

class NibblizationLayer
{
public:
    static constexpr int    kSectorByteSize    = 256;
    static constexpr int    kSectorsPerTrack   = 16;
    static constexpr int    kTrackCount        = 35;
    static constexpr int    kImageByteSize     = kSectorByteSize * kSectorsPerTrack * kTrackCount;
    static constexpr Byte   kDefaultVolume     = 254;
    static constexpr size_t kTrackBitCapacity  = 6400 * 8;

    static HRESULT  Nibblize    (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out);
    static HRESULT  NibblizeDsk (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizeDo  (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizePo  (const vector<Byte> & raw, DiskImage & out);

    static HRESULT  Denibblize  (const DiskImage & img, DiskFormat fmt, vector<Byte> & out);
};
