#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFormat
//
//  The geometry and field marks of a 16-sector, 35-track Disk II image
//  (.dsk / .do / .po, 143360 bytes), as its nibblizer and denibblizer use
//  them.
//
////////////////////////////////////////////////////////////////////////////////

class NibbleFormat
{
public:
    static constexpr int     kSectorByteSize   = 256;
    static constexpr int     kSectorsPerTrack  = 16;
    static constexpr int     kTrackCount       = 35;
    static constexpr int     kImageByteSize    = kSectorByteSize * kSectorsPerTrack * kTrackCount;
    static constexpr Byte    kDefaultVolume    = 254;
    static constexpr size_t  kTrackBitCapacity = 6400 * 8;

    //  The marks that begin a field. Finding a real gap means finding what
    //  follows one, so these are shared rather than copied.
    static constexpr Byte    kProlog0          = 0xD5;
    static constexpr Byte    kProlog1          = 0xAA;
    static constexpr Byte    kAddressProlog2   = 0x96;
    static constexpr Byte    kDataProlog2      = 0xAD;

    //  A data field's nibbles between its prologue and epilogue: 342 6-and-2
    //  nibbles and the checksum nibble.
    static constexpr size_t  kDataNibbleCount  = 343;
};
