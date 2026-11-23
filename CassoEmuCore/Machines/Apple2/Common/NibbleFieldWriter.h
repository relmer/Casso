#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter
//
//  Packs a 16-sector track's sync, address fields and data fields into a bit
//  buffer, MSB first. The destination is zero-filled and sized by the caller.
//
////////////////////////////////////////////////////////////////////////////////

class NibbleFieldWriter
{
public:
    static constexpr int  kAddrPrologueGap = 20;
    static constexpr int  kDataPrologueGap = 6;
    static constexpr int  kSyncTailZeros   = 2;

    static void  PackNibbleBits     (vector<Byte> & dst, size_t & bitOffset, Byte nibble);
    static void  PackSyncNibbleBits (vector<Byte> & dst, size_t & bitOffset);

    //  Sync gap, address prologue, 4-and-4 volume, track, sector and
    //  checksum, then the epilogue.
    static void  AppendAddressField (vector<Byte>  &  dst,
                                     size_t        &  bitOffset,
                                     Byte             volume,
                                     Byte             track,
                                     Byte             sector);

    //  Sync gap, data prologue, the 6-and-2 payload and its checksum, then
    //  the epilogue.
    static void  AppendDataField    (vector<Byte>  &  dst,
                                     size_t        &  bitOffset,
                                     const Byte    *  sectorData);

private:
    static Byte  Encode44Odd  (Byte val);
    static Byte  Encode44Even (Byte val);
};
