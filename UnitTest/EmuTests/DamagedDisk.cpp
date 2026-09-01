#include "Pch.h"

#include "DamagedDisk.h"

#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





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
//  DamagedDisk::BreakOneSector
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::BreakOneSector (DiskImage & inOutImage, int track, int whichSector)
{
    vector<Byte> &  bits   = inOutImage.GetTrackBitsForWrite (track);
    size_t          addrAt = FindAddressField (bits, whichSector);



    Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to break");

    PatchFieldChecksum (bits, addrAt,
                        NibblizationLayer::kDefaultVolume,
                        static_cast<Byte> (track));
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
//  DamagedDisk::DuplicateSectorIntoSlot
//
//  The header stays VALID -- its checksum is recomputed for the slot it now
//  claims -- because a header that fails its checksum is the BreakOneSector
//  case. This one decodes cleanly and lands in the wrong place, so the track
//  yields sixteen sectors covering only fifteen distinct slots.
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::DuplicateSectorIntoSlot (DiskImage & inOutImage,
                                           int         track,
                                           int         whichSector,
                                           int         claimSlot)
{
    vector<Byte> &  bits   = inOutImage.GetTrackBitsForWrite (track);
    size_t          addrAt = FindAddressField (bits, whichSector);
    Byte            volume = 0;
    Byte            onDisk = 0;
    Byte            slot   = static_cast<Byte> (claimSlot);



    Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to redirect");

    volume = ReadOddEven (bits, addrAt + 3);
    onDisk = ReadOddEven (bits, addrAt + 5);

    WriteOddEven (bits, addrAt + 7, slot);
    WriteOddEven (bits, addrAt + 9, static_cast<Byte> (volume ^ onDisk ^ slot));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::FindAddressField
//
////////////////////////////////////////////////////////////////////////////////

size_t DamagedDisk::FindAddressField (const vector<Byte> & bits, int which)
{
    size_t  i     = 0;
    int     seen  = 0;
    size_t  found = SIZE_MAX;



    for (i = 0; i + 2 < bits.size() && found == SIZE_MAX; i++)
    {
        if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0x96)
        {
            if (seen == which)
            {
                found = i;
            }

            seen++;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::PatchFieldChecksum
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::PatchFieldChecksum (vector<Byte> & bits,
                                      size_t         addrAt,
                                      Byte           volume,
                                      Byte           track)
{
    Byte  sector = ReadOddEven (bits, addrAt + 7);
    Byte  wrong  = static_cast<Byte> ((volume ^ track ^ sector) ^ 0xFF);



    WriteOddEven (bits, addrAt + 9, wrong);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::WriteOddEven
//
//  4-and-4: the odd bits ride one byte and the even bits the next, both with
//  the gaps forced to 1 so every byte still reads as a valid nibble.
//
////////////////////////////////////////////////////////////////////////////////

void DamagedDisk::WriteOddEven (vector<Byte> & bits, size_t at, Byte value)
{
    bits[at]     = static_cast<Byte> ((value >> 1) | 0xAA);
    bits[at + 1] = static_cast<Byte> (value | 0xAA);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk::ReadOddEven
//
////////////////////////////////////////////////////////////////////////////////

Byte DamagedDisk::ReadOddEven (const vector<Byte> & bits, size_t at)
{
    return static_cast<Byte> (((bits[at] << 1) | 1) & bits[at + 1]);
}
