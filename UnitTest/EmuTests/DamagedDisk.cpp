#include "Pch.h"

#include "DamagedDisk.h"

#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

//  The address prologue, and the field offsets that follow it, in NIBBLES.
static constexpr Byte  s_kAddrProlog0   = 0xD5;
static constexpr Byte  s_kAddrProlog1   = 0xAA;
static constexpr Byte  s_kAddrProlog2   = 0x96;
static constexpr int   s_kVolumeNibble  = 3;
static constexpr int   s_kTrackNibble   = 5;
static constexpr int   s_kSectorNibble  = 7;
static constexpr int   s_kCheckNibble   = 9;
static constexpr int   s_kBitsPerNibble = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::BuildGoodDos33
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::BuildGoodDos33 (DiskImage & outImage)
{
    BlankDiskSpec  spec;                       // defaults: WOZ, DOS 3.3
    vector<Byte>   woz;
    HRESULT        hrBuild = S_OK;
    HRESULT        hrLoad  = S_OK;



    hrBuild = BlankDiskBuilder::Build (spec, BootPayload{}, woz);
    Assert::IsTrue (SUCCEEDED (hrBuild), L"the blank-disk builder must produce a WOZ to damage");

    hrLoad = WozLoader::Load (woz, outImage);
    Assert::IsTrue (SUCCEEDED (hrLoad), L"the WOZ it produced must load");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::BreakSector
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::BreakSector (DiskImage & inOutImage, int track, int sectorNumber)
{
    vector<Byte> &  bits     = inOutImage.GetTrackBitsForWrite (track);
    size_t          bitCount = inOutImage.GetTrackBitCount (track);
    size_t          fieldAt  = FindAddressFieldBySector (bits, bitCount, sectorNumber);
    Byte            volume   = 0;
    Byte            onDisk   = 0;
    Byte            wrong    = 0;



    Assert::AreNotEqual (SIZE_MAX, fieldAt, L"the track must carry the sector being broken");

    volume = ReadOddEvenAtBit (bits, fieldAt + s_kVolumeNibble * s_kBitsPerNibble);
    onDisk = ReadOddEvenAtBit (bits, fieldAt + s_kTrackNibble  * s_kBitsPerNibble);

    //  A checksum that cannot be right for this header. FLIPPING A BIT IN THE
    //  ENCODED NIBBLE WOULD NOT DO: 4-and-4 forces the gaps to 1, so clearing
    //  one changes the decoded value only when that bit was already set -- a
    //  corruption that silently does nothing for half the sectors on a track.
    wrong = static_cast<Byte> ((volume ^ onDisk ^ static_cast<Byte> (sectorNumber)) ^ 0xFF);

    WriteOddEvenAtBit (bits, fieldAt + s_kCheckNibble * s_kBitsPerNibble, wrong);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::WipeTrack
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::WipeTrack (DiskImage & inOutImage, int track)
{
    vector<Byte> &  bits = inOutImage.GetTrackBitsForWrite (track);



    std::fill (bits.begin(), bits.end(), static_cast<Byte> (0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::RedirectSectorToSlot
//
//  The header stays VALID -- its checksum is recomputed for the slot it now
//  claims -- because a header that fails its checksum is the BreakSector case.
//  This one decodes cleanly and lands in the wrong place, so the track yields
//  sixteen sectors covering only fifteen distinct slots.
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::RedirectSectorToSlot (DiskImage & inOutImage,
                                        int         track,
                                        int         sectorNumber,
                                        int         claimSlot)
{
    vector<Byte> &  bits     = inOutImage.GetTrackBitsForWrite (track);
    size_t          bitCount = inOutImage.GetTrackBitCount (track);
    size_t          fieldAt  = FindAddressFieldBySector (bits, bitCount, sectorNumber);
    Byte            volume   = 0;
    Byte            onDisk   = 0;
    Byte            slot     = static_cast<Byte> (claimSlot);



    Assert::AreNotEqual (SIZE_MAX, fieldAt, L"the track must carry the sector being redirected");

    volume = ReadOddEvenAtBit (bits, fieldAt + s_kVolumeNibble * s_kBitsPerNibble);
    onDisk = ReadOddEvenAtBit (bits, fieldAt + s_kTrackNibble  * s_kBitsPerNibble);

    WriteOddEvenAtBit (bits, fieldAt + s_kSectorNibble * s_kBitsPerNibble, slot);
    WriteOddEvenAtBit (bits, fieldAt + s_kCheckNibble  * s_kBitsPerNibble,
                       static_cast<Byte> (volume ^ onDisk ^ slot));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::DuplicateSector
//
//  The copy runs from the sector's address prologue to the NEXT sector's, so
//  it carries the address field, the data field and the sync gap that follows
//  them, and it is inserted where that gap ends: directly in front of the next
//  sector's prologue. The decoder therefore meets the copy in the same state it
//  met the original, having just read the same run of self-sync nibbles.
//
//  A BIT-LEVEL INSERT, NOT A BYTE COPY. Sector 5's prologue begins 4 bits into
//  a byte, and the tail of the track moves by a span that is not a multiple of
//  8 either, so nothing here can be a memmove. The original is copied out whole
//  and every bit of the grown track is written from it, because ResizeTrack
//  zeros the buffer it grows.
//
//  THE TRACK GROWS rather than giving up sync gap for the copy. A sector is
//  3,164 bits and the whole track carries 4,160 bits of sync, so paying for the
//  copy out of gap would strip nearly every self-sync nibble from every sector
//  and make the result a test of gapless decoding rather than of duplication.
//  Growing leaves every original bit where the builder put it, and the decoder
//  walks whatever bit count it is handed -- WOZ track lengths vary anyway.
//
//  DIRECTLY AFTER THE ORIGINAL, not at the end of the track, because the
//  decoder stops once every slot is filled: a copy that arrives after the
//  sixteenth distinct sector is never read. That also means the LAST physical
//  sector cannot be duplicated visibly this way; pick one from the middle.
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::DuplicateSector (DiskImage & inOutImage, int track, int sectorNumber)
{
    vector<Byte>    original = inOutImage.GetTrackBits (track);
    vector<Byte> *  grown    = nullptr;
    size_t          bitCount = inOutImage.GetTrackBitCount (track);
    size_t          fieldAt  = FindAddressFieldBySector (original, bitCount, sectorNumber);
    size_t          spanEnd  = 0;
    size_t          spanBits = 0;



    Assert::AreNotEqual (SIZE_MAX, fieldAt, L"the track must carry the sector being duplicated");

    //  Up to the next prologue, or to the end of the track when this is the
    //  last sector on it.
    spanEnd = FindAddressFieldFrom (original, bitCount, fieldAt + 3 * s_kBitsPerNibble);

    if (spanEnd == SIZE_MAX)
    {
        spanEnd = bitCount;
    }

    spanBits = spanEnd - fieldAt;

    inOutImage.ResizeTrack (track, bitCount + spanBits);
    grown = &inOutImage.GetTrackBitsForWrite (track);

    CopyBits (original, 0,       *grown, 0,                  spanEnd);
    CopyBits (original, fieldAt, *grown, spanEnd,            spanBits);
    CopyBits (original, spanEnd, *grown, spanEnd + spanBits, bitCount - spanEnd);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::CountAddressFields
//
////////////////////////////////////////////////////////////////////////////////

int DamagedDisk::CountAddressFields (const DiskImage & image, int track)
{
    const vector<Byte> &  bits     = image.GetTrackBits (track);
    size_t                bitCount = image.GetTrackBitCount (track);
    size_t                at       = 0;
    int                   found    = 0;



    at = FindAddressFieldFrom (bits, bitCount, 0);

    while (at != SIZE_MAX)
    {
        found++;
        at = FindAddressFieldFrom (bits, bitCount, at + 1);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::FindAddressFieldFrom
//
////////////////////////////////////////////////////////////////////////////////

size_t DamagedDisk::FindAddressFieldFrom (const vector<Byte> & bits, size_t bitCount, size_t fromBit)
{
    size_t  at = 0;



    for (at = fromBit; at + 3 * s_kBitsPerNibble <= bitCount; at++)
    {
        if (ReadNibbleAtBit (bits, at)                        == s_kAddrProlog0
         && ReadNibbleAtBit (bits, at + s_kBitsPerNibble)     == s_kAddrProlog1
         && ReadNibbleAtBit (bits, at + 2 * s_kBitsPerNibble) == s_kAddrProlog2)
        {
            return at;
        }
    }

    return SIZE_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::FindAddressFieldBySector
//
////////////////////////////////////////////////////////////////////////////////

size_t DamagedDisk::FindAddressFieldBySector (const vector<Byte> & bits,
                                              size_t               bitCount,
                                              int                  sectorNumber)
{
    size_t  at    = 0;
    size_t  limit = 0;



    limit = (bitCount > 11 * s_kBitsPerNibble) ? bitCount - 11 * s_kBitsPerNibble : 0;

    at = FindAddressFieldFrom (bits, bitCount, 0);

    while (at != SIZE_MAX && at < limit)
    {
        if (ReadOddEvenAtBit (bits, at + s_kSectorNibble * s_kBitsPerNibble)
                == static_cast<Byte> (sectorNumber))
        {
            return at;
        }

        at = FindAddressFieldFrom (bits, bitCount, at + 1);
    }

    return SIZE_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::CopyBits
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::CopyBits (
    const vector<Byte>  &  src,
    size_t                 srcAt,
    vector<Byte>        &  dst,
    size_t                 dstAt,
    size_t                 count)
{
    size_t  i    = 0;
    size_t  from = 0;
    size_t  to   = 0;
    Byte    mask = 0;



    for (i = 0; i < count; i++)
    {
        from = srcAt + i;
        to   = dstAt + i;
        mask = static_cast<Byte> (1 << (7 - (to & 7)));

        if (((src[from >> 3] >> (7 - (from & 7))) & 1) != 0)
        {
            dst[to >> 3] = static_cast<Byte> (dst[to >> 3] | mask);
        }
        else
        {
            dst[to >> 3] = static_cast<Byte> (dst[to >> 3] & ~mask);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::ReadNibbleAtBit
//
////////////////////////////////////////////////////////////////////////////////

Byte DamagedDisk::ReadNibbleAtBit (const vector<Byte> & bits, size_t bitAt)
{
    Byte    value = 0;
    int     bit   = 0;
    size_t  at    = 0;



    for (bit = 0; bit < s_kBitsPerNibble; bit++)
    {
        at    = bitAt + static_cast<size_t> (bit);
        value = static_cast<Byte> (value << 1);

        if ((at >> 3) < bits.size() && ((bits[at >> 3] >> (7 - (at & 7))) & 1) != 0)
        {
            value = static_cast<Byte> (value | 1);
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::WriteNibbleAtBit
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::WriteNibbleAtBit (vector<Byte> & bits, size_t bitAt, Byte value)
{
    int     bit  = 0;
    size_t  at   = 0;
    Byte    mask = 0;



    for (bit = 0; bit < s_kBitsPerNibble; bit++)
    {
        at   = bitAt + static_cast<size_t> (bit);
        mask = static_cast<Byte> (1 << (7 - (at & 7)));

        if ((at >> 3) >= bits.size())
        {
            continue;
        }

        if (((value >> (s_kBitsPerNibble - 1 - bit)) & 1) != 0)
        {
            bits[at >> 3] = static_cast<Byte> (bits[at >> 3] | mask);
        }
        else
        {
            bits[at >> 3] = static_cast<Byte> (bits[at >> 3] & ~mask);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::ReadOddEvenAtBit
//
////////////////////////////////////////////////////////////////////////////////

Byte DamagedDisk::ReadOddEvenAtBit (const vector<Byte> & bits, size_t bitAt)
{
    Byte  odd  = ReadNibbleAtBit (bits, bitAt);
    Byte  even = ReadNibbleAtBit (bits, bitAt + s_kBitsPerNibble);



    return static_cast<Byte> (((odd << 1) | 1) & even);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::WriteOddEvenAtBit
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::WriteOddEvenAtBit (vector<Byte> & bits, size_t bitAt, Byte value)
{
    WriteNibbleAtBit (bits, bitAt, static_cast<Byte> ((value >> 1) | 0xAA));
    WriteNibbleAtBit (bits, bitAt + s_kBitsPerNibble, static_cast<Byte> (value | 0xAA));
}
