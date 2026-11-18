#pragma once

#include "Pch.h"

class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  Gcr35Track
//
//  The 3.5-inch GCR format of the 400K and 800K disks Apple's drives read:
//  80 tracks in five speed zones of 16, with 12, 11, 10, 9 and 8 sectors a
//  track, laid out with a 2:1 interleave. A sector is an address field,
//
//      D5 AA 96  track  sector  side  format  checksum  DE AA
//
//  each value 6-and-2 encoded, then a data field,
//
//      D5 AA AD  sector  699 data nibbles  4 checksum nibbles  DE AA
//
//  whose 524 bytes, 12 tag bytes then 512 data bytes, are encoded three at a
//  time into four nibbles with three running checksums. Tracks are bit
//  streams, one byte a bit cell, a 1 a flux transition, at 2 microseconds a
//  cell; a track's length is the cells in one revolution at its zone's
//  speed.
//
//  Blocks are numbered track by track, side 0 then side 1, sector by sector.
//
////////////////////////////////////////////////////////////////////////////////

class Gcr35Track
{
public:
    static constexpr int       kTracks         = 80;
    static constexpr int       kTracksPerZone  = 16;
    static constexpr size_t    kTagBytes       = 12;
    static constexpr size_t    kDataBytes      = 512;
    static constexpr size_t    kSectorBytes    = kTagBytes + kDataBytes;
    static constexpr size_t    kDataNibbles    = 699;
    static constexpr size_t    kSumNibbles     = 4;
    static constexpr Byte      kFormatOneSide  = 0x02;
    static constexpr Byte      kFormatTwoSides = 0x22;
    static constexpr uint32_t  kBlocksOneSide  = 800;
    static constexpr uint32_t  kBlocksTwoSides = 1600;

    static int       GetSectorsPerTrack (int track);
    static size_t    GetTrackBits       (int track);
    static uint32_t  GetFirstBlock      (int track, int side, int sides);

    //  The physical order of a track's sectors, 2:1 interleave.
    static std::vector<int>  GetInterleave (int sectors);

    //  524 bytes to 699 data nibbles and 4 checksum nibbles.
    static void  EncodeSector (std::span<const Byte> bytes, std::vector<Byte> & outNibbles);

    //  703 nibbles back to 524 bytes; false when a nibble is not GCR or the
    //  checksum does not match.
    static bool  TryDecodeSector (std::span<const Byte> nibbles, std::vector<Byte> & outBytes);

    //  A track's bit stream from its sectors' 512 data bytes each, in sector
    //  order; tags are written as zeros.
    static void  BuildTrack (int track, int side, Byte format, std::span<const Byte> sectorData, std::vector<Byte> & outBits);

    //  The sectors' data bytes found on a track's bit stream, in sector
    //  order, with whether each sector was found and decoded.
    //  Every track and side of a WOZ image's 3.5-inch disk, TMAP entry track
    //  * 2 + side, decoded to blocks in block order. `outMissing` has the
    //  numbers of the blocks that don't decode, which are zero in the blocks.
    static void  DecodeWoz (const DiskImage & woz, int sides, std::vector<Byte> & outBlocks, std::vector<uint32_t> & outMissing);

    static void  DecodeTrack (int track, int side, const std::vector<Byte> & bits,
                              std::vector<Byte> & outSectorData, std::vector<bool> & outFound);

private:
    static void  AppendByte   (std::vector<Byte> & bits, Byte value);
    static void  AppendSync   (std::vector<Byte> & bits, int count);
    static Byte  GetGcr       (Byte value);
};
