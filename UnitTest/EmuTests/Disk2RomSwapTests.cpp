#include "Pch.h"

#include "../EhmTestHelper.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2RomSwapTests
//
//  The slot 6 card's boot ROM follows the disk in drive 1: the 13-sector ROM
//  for a 13-sector disk, and the 16-sector ROM again for any other.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2RomSwapTests)
{
public:

    TEST_METHOD (A13SectorDisk_SwapsInThe13SectorRom_OnEachMachine)
    {
        for (const char * id : { "Apple2Plus", "Apple2eEnhanced" })
        {
            TestMachine        machine (id);
            std::vector<Byte>  rom16   = ReadSlotRom (machine);
            std::vector<Byte>  woz;

            Make13SectorWoz (woz);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Woz, woz));
            Assert::IsTrue (ReadSlotRom (machine) != rom16, TextEncoding::NarrowToWide (id).c_str());

            machine.GetDiskStore().Eject (6, 0);
            Assert::IsTrue (ReadSlotRom (machine) == rom16, TextEncoding::NarrowToWide (id).c_str());
        }
    }



    TEST_METHOD (A13SectorDisk_SwapsTheBuiltInDrivesBootPage_OnIIc)
    {
        TestMachine        machine ("Apple2c");
        std::vector<Byte>  rom16   = ReadSlotRom (machine);
        std::vector<Byte>  woz;

        Make13SectorWoz (woz);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Woz, woz));
        Assert::IsTrue (ReadSlotRom (machine) != rom16, L"the 13-sector page is at $C600");

        machine.GetDiskStore().Eject (6, 0);
        Assert::IsTrue (ReadSlotRom (machine) == rom16, L"the //c's page is back at $C600");
    }



    TEST_METHOD (A13SectorDiskInDrive2_LeavesThe16SectorRom)
    {
        TestMachine        machine ("Apple2eEnhanced");
        std::vector<Byte>  rom16   = ReadSlotRom (machine);
        std::vector<Byte>  woz;

        Make13SectorWoz (woz);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Woz, woz));
        Assert::IsTrue (ReadSlotRom (machine) == rom16);
    }

private:

    static std::vector<Byte> ReadSlotRom (TestMachine & machine)
    {
        std::vector<Byte>  rom (256);

        for (Word i = 0; i < 256; i++)
        {
            rom[i] = machine.GetMemoryBus().ReadByte ((Word) (0xC600 + i));
        }

        return rom;
    }

    static void Make13SectorWoz (std::vector<Byte> & outWoz)
    {
        std::vector<Byte>  flat (ThirteenSectorCodec::kImageByteSize, 0x5A);
        DiskImage          image;

        AssertSucceeded (ThirteenSectorCodec::Nibblize (flat, 254, image));
        AssertSucceeded (WozLoader::Serialize (image, outWoz));
    }
};
