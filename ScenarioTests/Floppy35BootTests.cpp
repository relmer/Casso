#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Woz35Builder.h"
#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/LironCard.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35BootTests
//
//  Apple's firmware on the 3.5-inch controllers, booting real ProDOS from an
//  800K disk. Each disk is a 140K disk's blocks in ProDOS order, padded to
//  1,600 blocks: the volume inside is the 140K one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Floppy35BootTests)
{
public:

    static constexpr const char *  kDiskPath     = "C:\\disks\\800k.po";
    static constexpr uint64_t      kBootCap      = 80'000'000ULL;
    static constexpr size_t        kSector       = 256;
    static constexpr size_t        kTrackBlocks  = 8;
    static constexpr size_t        kTrackSectors = 16;
    static constexpr size_t        kBlocks800K   = 1600;
    static constexpr size_t        kBlocks140K   = 280;

    //  ProDOS block half -> DOS 3.3 logical sector within the track.
    static constexpr int  kDosSector[] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };


    //  The image offset of DOS-order sector `half` of block `block`.
    static size_t GetDosOffset (size_t block, size_t half)
    {
        size_t  track = block / kTrackBlocks;



        return (track * kTrackSectors + (size_t) kDosSector[(block % kTrackBlocks) * 2 + half]) * kSector;
    }


    //  The 140K disk's blocks in ProDOS order, then empty blocks to 800K.
    static std::vector<Byte> Make800K (const std::vector<Byte> & dosOrder)
    {
        std::vector<Byte>  out (kBlocks800K * 2 * kSector, 0);



        for (size_t block = 0; block < kBlocks140K; block++)
        {
            for (size_t half = 0; half < 2; half++)
            {
                std::copy_n (dosOrder.begin() + (ptrdiff_t) GetDosOffset (block, half), kSector, out.begin() + (ptrdiff_t) ((block * 2 + half) * kSector));
            }
        }

        return out;
    }


    //  The first 140K of an 800K image, back in DOS order.
    static std::vector<Byte> Take140K (const std::vector<Byte> & image)
    {
        std::vector<Byte>  out (kBlocks140K * 2 * kSector, 0);



        for (size_t block = 0; block < kBlocks140K; block++)
        {
            for (size_t half = 0; half < 2; half++)
            {
                std::copy_n (image.begin() + (ptrdiff_t) ((block * 2 + half) * kSector), kSector, out.begin() + (ptrdiff_t) GetDosOffset (block, half));
            }
        }

        return out;
    }


    static bool RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 100'000ULL;

        std::vector<std::string>  rows;
        uint64_t                  spent    = 0;
        bool                      onScreen = false;



        while (!onScreen && spent < cycleCap)
        {
            spent += host.RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }


    static void Attach (FakeDiskFileIo & io, const std::vector<Byte> & image, std::vector<PortConfig> & ports)
    {
        io.files[kDiskPath]  = image;
        io.stamps[kDiskPath] = FileStamp { image.size(), 1 };

        ports.assign (2, PortConfig {});
        ports[0].device = kpszApple35DriveDevice;
        ports[0].image  = kDiskPath;
    }


    //  AN 800K DISK IN A UNIDISK 3.5 BOOTS through the UniDisk 3.5 interface
    //  card in slot 5 of an enhanced //e: the card's firmware and the drive's
    //  talk over the SmartPort bus, each IWM's bits to the other.
    TEST_METHOD (LironCard_BootsProDosFromAUniDisk)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);
        ports[0].device = kpszUniDisk35Device;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsTrue (machine.GetRefs().lironCards[0]->HasFirmware(), L"the card's ROM is in the fixtures");
        Assert::AreEqual ((size_t) 1, machine.GetRefs().lironCards[0]->GetDriveCount(), L"the drive's ROM is in the fixtures");

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the UniDisk");
    }


    //  THE SAME DISK BOOTS FROM THE CARD on a ][+ and an unenhanced //e,
    //  whose ROMs don't start a SmartPort card at power on, so PR#5 starts
    //  it.
    TEST_METHOD (LironCard_BootsOnTheOtherHosts)
    {
        std::vector<Byte>  image = Make800K (GuestSession::RequireProDosUsersDisk());



        for (const char * machineId : { "Apple2Plus", "Apple2e" })
        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, image, ports);
            ports[0].device = kpszUniDisk35Device;

            TestMachine  machine (machineId, TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);

            Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"the machine stops at BASIC");
            KeystrokeInjector::InjectLine (machine, "PR#5");

            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), TextEncoding::NarrowToWide (machineId).c_str());
        }
    }


    //  TWO CHAINED UNIDISKS both take unit numbers: the first passes the bus
    //  enable to the second once it has its number.
    TEST_METHOD (LironCard_TwoChainedUniDisksAreOnline)
    {
        constexpr Word  kUnitNumber = 0x0B;

        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;
        std::vector<Byte>        image = Make800K (GuestSession::RequireProDosUsersDisk());



        Attach (io, image, ports);
        ports[0].device = kpszUniDisk35Device;
        ports[1].device = kpszUniDisk35Device;
        ports[1].image  = "C:\\disks\\second.po";
        io.files[ports[1].image]  = image;
        io.stamps[ports[1].image] = FileStamp { image.size(), 1 };

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);
        LironCard  & card = *machine.GetRefs().lironCards[0];

        Assert::AreEqual ((size_t) 2, card.GetDriveCount());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the first UniDisk");
        Assert::AreEqual ((Byte) 0x81, card.GetDrive (0).Read (kUnitNumber), L"the first drive is unit 1, bus ID $81");
        Assert::AreEqual ((Byte) 0x82, card.GetDrive (1).Read (kUnitNumber), L"the second drive is unit 2, bus ID $82");
    }


    //  AN EMPTY UNIDISK IN SLOT 5 DOESN'T STOP the floppy in slot 6 from
    //  booting: the card's boot code returns to the slot scan.
    TEST_METHOD (LironCard_EmptyDrivePassesToTheFloppy)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (1);



        ports[0].device = kpszUniDisk35Device;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::DiskOnly, { { 5, kpszLironCardDevice, ports } }, io);

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "users.dsk", DiskFormat::Dsk, GuestSession::RequireProDosUsersDisk()));

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"the floppy boots past the empty UniDisk");
    }


    //  A DIRECTORY THE GUEST CREATES on a UniDisk is in the image once the
    //  drive's firmware deselects the mechanism. A SmartPort CONTROL call
    //  with code 4 then ejects the disk.
    TEST_METHOD (LironCard_GuestWriteAndEject)
    {
        constexpr Word      kProgram        = 0x0300;
        constexpr Word      kParams         = 0x0310;
        constexpr Word      kList           = 0x0320;
        constexpr Word      kEntryByte      = 0xC5FF;
        constexpr Byte      kControl        = 0x04;
        constexpr Byte      kEjectCode      = 0x04;
        constexpr uint64_t  kDeselectCycles = 3'000'000;

        FakeDiskFileIo            io;
        FixtureProvider           fixtures;
        std::vector<Byte>         merlin;
        std::vector<PortConfig>   ports;
        std::vector<std::string>  rows;
        VolumeListing             listing;
        bool                      found = false;
        Word                      entry = 0;



        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-b.dsk", merlin));
        Attach (io, Make800K (merlin), ports);
        ports[0].device = kpszUniDisk35Device;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);
        LironCard  & card = *machine.GetRefs().lironCards[0];

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "RETURN", kBootCap), L"ProDOS, BASIC.SYSTEM and STARTUP run from the UniDisk");
        Assert::IsTrue (GuestSession::TryPageToPrompt (machine, rows), L"the disk's STARTUP pages to the prompt");

        GuestSession::TypeAndCollect (machine, "CREATE UNIDIR");
        GuestSession::TypeAndCollect (machine, "CAT");

        //  The firmware's motor timeout deselects the drive about 2.3 seconds
        //  after its last command.
        machine.RunCycles (kDeselectCycles);

        AssertSucceeded (ProDosVolume (Take140K (io.files[kDiskPath])).Enumerate (listing));

        for (const FileEntry & each : listing.entries)
        {
            found = found || (each.name == "UNIDIR" && each.isDirectory);
        }

        Assert::IsTrue (found, L"the guest's directory is in the image");

        //  JSR to the SmartPort entry, the ProDOS entry plus 3, with CONTROL
        //  for unit 1, code 4, and an empty control list.
        entry = (Word) (0xC500 + card.OnSlotPageAccess (kEntryByte, false, 0) + 3);

        {
            const Byte  program[] = { 0x20, (Byte) entry, (Byte) (entry >> 8), kControl, (Byte) kParams, (Byte) (kParams >> 8), 0x60 };
            const Byte  params[]  = { 0x03, 0x01, (Byte) kList, (Byte) (kList >> 8), kEjectCode };



            for (size_t i = 0; i < std::size (program); i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (kProgram + i), program[i]);
            }

            for (size_t i = 0; i < std::size (params); i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (kParams + i), params[i]);
            }

            machine.GetMemoryBus().WriteByte (kList,     0);
            machine.GetMemoryBus().WriteByte (kList + 1, 0);
        }

        KeystrokeInjector::InjectLine (machine, "CALL 768");
        machine.RunCycles (4'000'000);

        Assert::IsFalse (card.GetDrive (0).GetMechanism().HasDisk(), L"the CONTROL call ejects the disk");
    }


    //  AN 800K DISK IN A UNIDISK 3.5 ON THE //C'S DISK PORT BOOTS through the
    //  //c's ROM 4 SmartPort firmware, the built-in drive being empty.
    TEST_METHOD (Apple2c_BootsProDosFromAUniDiskOnTheDiskPort)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;
        std::vector<PortConfig>  chain (1);



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);
        chain[0].device = kpszUniDisk35Device;
        chain[0].image  = kDiskPath;

        TestMachine  machine ("Apple2c", chain, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsNotNull (machine.GetRefs().apple2cSmartPort);
        Assert::AreEqual ((size_t) 1, machine.GetRefs().apple2cSmartPort->GetPort().GetDriveCount());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the UniDisk on the disk port");
    }


    //  AN 800K PRODOS DISK BOOTS through Apple's 3.5 controller card firmware
    //  in slot 5 of an enhanced //e with no floppy controller.
    TEST_METHOD (Apple35Card_BootsProDosFrom800K)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsTrue (machine.GetRefs().apple35Cards[0]->HasFirmware(), L"the card's ROM is in the fixtures");

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the 800K disk");
    }


    //  A DIRECTORY THE GUEST CREATES on the 800K disk is in the image once the
    //  drive flushes: GCR written through the card's SWIM decodes back to the
    //  blocks ProDOS wrote.
    TEST_METHOD (Apple35Card_GuestWriteReachesImage)
    {
        FakeDiskFileIo            io;
        FixtureProvider           fixtures;
        std::vector<Byte>         merlin;
        std::vector<PortConfig>   ports;
        std::vector<std::string>  rows;
        VolumeListing             listing;
        bool                      found = false;



        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-b.dsk", merlin));
        Attach (io, Make800K (merlin), ports);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        //  The prompt appears before BASIC.SYSTEM runs STARTUP, so the check
        //  is for STARTUP's first page instead.
        Assert::IsTrue (RunUntilScreenShows (machine, "RETURN", kBootCap), L"ProDOS, BASIC.SYSTEM and STARTUP run from the 800K disk");
        Assert::IsTrue (GuestSession::TryPageToPrompt (machine, rows), L"the disk's STARTUP pages to the prompt");

        GuestSession::TypeAndCollect (machine, "CREATE SONYDIR");
        GuestSession::TypeAndCollect (machine, "CAT");

        AssertSucceeded (machine.GetRefs().apple35Cards[0]->GetCardMemory().GetDrive (0).Flush());
        AssertSucceeded (ProDosVolume (Take140K (io.files[kDiskPath])).Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            found = found || (entry.name == "SONYDIR" && entry.isDirectory);
        }

        Assert::IsTrue (found, L"the guest's directory is in the image");
    }


    //  A WOZ 2 3.5-INCH IMAGE BOOTS through the card's firmware, its tracks
    //  read from the WOZ bit streams.
    TEST_METHOD (Apple35Card_BootsAWoz35Image)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;



        Attach (io, Woz35Builder::Build (Make800K (GuestSession::RequireProDosUsersDisk())), ports);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsTrue (machine.GetRefs().apple35Cards[0]->GetCardMemory().GetDrive (0).IsWoz());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the WOZ image");
    }


    //  A DISKCOPY 4.2 IMAGE BOOTS on an unenhanced //e, whose ROM doesn't
    //  start a SmartPort card at power on, so PR#5 starts it. A guest write
    //  doesn't invalidate the image's data checksum.
    TEST_METHOD (Apple35Card_DiskCopy42OnAnUnenhancedIIe)
    {
        FakeDiskFileIo            io;
        FixtureProvider           fixtures;
        std::vector<Byte>         merlin;
        std::vector<Byte>         image;
        std::vector<Byte>         payload;
        std::vector<PortConfig>   ports;
        std::vector<std::string>  rows;
        DiskCopy42Header          header;
        std::string               parseError;



        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-b.dsk", merlin));

        header.name     = "Merlin";
        header.dataSize = (uint32_t) (kBlocks800K * 2 * kSector);
        DiskCopy42Container::Build (header, Make800K (merlin), image);
        Attach (io, image, ports);

        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"the //e stops at BASIC");
        KeystrokeInjector::InjectLine (machine, "PR#5");

        Assert::IsTrue (RunUntilScreenShows (machine, "RETURN", kBootCap), L"ProDOS, BASIC.SYSTEM and STARTUP run from the DiskCopy image");
        Assert::IsTrue (GuestSession::TryPageToPrompt (machine, rows), L"the disk's STARTUP pages to the prompt");

        GuestSession::TypeAndCollect (machine, "CREATE DC42DIR");
        AssertSucceeded (machine.GetRefs().apple35Cards[0]->GetCardMemory().GetDrive (0).Flush());

        AssertSucceeded (DiskCopy42Container::Parse (io.files[kDiskPath], header, payload, parseError));
        Assert::IsFalse (header.isDataChecksumBad, L"the data checksum matches the written data");
        Assert::IsFalse (payload == Make800K (merlin), L"the guest's write is in the image");
    }
};
