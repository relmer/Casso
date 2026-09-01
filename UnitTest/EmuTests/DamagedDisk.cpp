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
//  DamagedDisk::CountAddressFields
//
////////////////////////////////////////////////////////////////////////////////

int DamagedDisk::CountAddressFields (const DiskImage & image, int track)
{
    const vector<Byte> &  bits     = image.GetTrackBits (track);
    size_t                bitCount = image.GetTrackBitCount (track);
    size_t                at       = 0;
    int                   found    = 0;



    for (at = 0; at + 3 * s_kBitsPerNibble <= bitCount; at++)
    {
        if (ReadNibbleAtBit (bits, at)                        == s_kAddrProlog0
         && ReadNibbleAtBit (bits, at + s_kBitsPerNibble)     == s_kAddrProlog1
         && ReadNibbleAtBit (bits, at + 2 * s_kBitsPerNibble) == s_kAddrProlog2)
        {
            found++;
        }
    }

    return found;
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

    for (at = 0; at < limit; at++)
    {
        if (ReadNibbleAtBit (bits, at)                        != s_kAddrProlog0
         || ReadNibbleAtBit (bits, at + s_kBitsPerNibble)     != s_kAddrProlog1
         || ReadNibbleAtBit (bits, at + 2 * s_kBitsPerNibble) != s_kAddrProlog2)
        {
            continue;
        }

        if (ReadOddEvenAtBit (bits, at + s_kSectorNibble * s_kBitsPerNibble)
                == static_cast<Byte> (sectorNumber))
        {
            return at;
        }
    }

    return SIZE_MAX;
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
