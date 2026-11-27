#include "Pch.h"

#include "StampDiskFixtures.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::MakeSixteenSector
//
//  A DOS 3.3 order disk whose sectors each hold a pattern of their track
//  and sector, so a read that takes a wrong sector shows.
//
////////////////////////////////////////////////////////////////////////////////

void StampDiskFixtures::MakeSixteenSector (DiskImage & outImage)
{
    constexpr size_t  kSectorBytes = 256;
    constexpr int     kSectors     = 16;
    vector<Byte>  sectors ((size_t) kTracks * kSectors * kSectorBytes);



    for (size_t i = 0; i < sectors.size(); i++)
    {
        sectors[i] = (Byte) ((i / kSectorBytes) * 3 + i);
    }

    Nibblizer::NibblizeDsk (sectors, outImage);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::MakeThirteenSector
//
////////////////////////////////////////////////////////////////////////////////

void StampDiskFixtures::MakeThirteenSector (DiskImage & outImage)
{
    vector<Byte>  sectors (ThirteenSectorCodec::kImageByteSize);



    for (size_t i = 0; i < sectors.size(); i++)
    {
        sectors[i] = (Byte) ((i / ThirteenSectorCodec::kSectorByteSize) * 5 + i);
    }

    ThirteenSectorCodec::Nibblize (sectors, kVolume, outImage);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::ReplaceProlog
//
//  Rewrites each run of the `from` nibbles on a track's bits as `to`, up to
//  maxCount runs, and returns how many it rewrote.
//
////////////////////////////////////////////////////////////////////////////////

int StampDiskFixtures::ReplaceProlog (DiskImage & image, int track, std::span<const Byte> from, std::span<const Byte> to, int maxCount)
{
    constexpr size_t    kByteBits = 8;
    vector<Byte>      & bits      = image.GetTrackBitsForWrite (track);
    size_t              count     = image.GetTrackBitCount (track);
    size_t              span      = from.size() * kByteBits;
    int                 replaced  = 0;



    for (size_t at = 0; at + span <= count && (maxCount == kAllFields || replaced < maxCount); at++)
    {
        bool  isMatch = true;

        for (size_t i = 0; isMatch && i < from.size(); i++)
        {
            isMatch = ReadByteAt (bits, at + i * kByteBits) == from[i];
        }

        if (!isMatch)
        {
            continue;
        }

        for (size_t i = 0; i < to.size(); i++)
        {
            WriteByteAt (bits, at + i * kByteBits, to[i]);
        }

        replaced++;
        at += span - 1;
    }

    return replaced;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::MakeCopy
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiskCopy> StampDiskFixtures::MakeCopy (const DiskImage & image)
{
    return DiskCopy::MakeFromImage (image, 1, "stamp.woz", 0, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::ReadByteAt
//
////////////////////////////////////////////////////////////////////////////////

Byte StampDiskFixtures::ReadByteAt (const vector<Byte> & bits, size_t bitAt)
{
    constexpr size_t  kByteBits = 8;
    Byte              value     = 0;



    for (size_t i = 0; i < kByteBits; i++)
    {
        size_t  at = bitAt + i;

        value = (Byte) ((value << 1) | ((bits[at / kByteBits] >> (kByteBits - 1 - at % kByteBits)) & 1));
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StampDiskFixtures::WriteByteAt
//
////////////////////////////////////////////////////////////////////////////////

void StampDiskFixtures::WriteByteAt (vector<Byte> & bits, size_t bitAt, Byte value)
{
    constexpr size_t  kByteBits = 8;



    for (size_t i = 0; i < kByteBits; i++)
    {
        size_t  at   = bitAt + i;
        Byte    mask = (Byte) (0x80 >> (at % kByteBits));

        bits[at / kByteBits] = (Byte) (((value >> (kByteBits - 1 - i)) & 1) ? (bits[at / kByteBits] | mask) : (bits[at / kByteBits] & ~mask));
    }
}
