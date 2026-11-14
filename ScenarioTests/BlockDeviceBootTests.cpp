#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/BlockImage.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceBootTests
//
//  Apple's firmware on the emulated hard disk cards, booting real ProDOS.
//  The hard disk images are built from the ProDOS Users Disk, reordered into
//  ProDOS block order, so the volume on the hard disk is Apple's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockDeviceBootTests)
{
public:

    static constexpr const char *  kHardDiskPath = "C:\\disks\\hd.po";
    static constexpr uint64_t      kBootCap      = 60'000'000ULL;


    //  A 140K disk reordered between DOS sector order and ProDOS blocks:
    //  block b is DOS sectors map[2(b mod 8)] and map[2(b mod 8)+1] of track
    //  b/8.
    static std::vector<Byte> Reorder (const std::vector<Byte> & bytes, bool toProDos)
    {
        constexpr size_t  kSector          = 256;
        constexpr size_t  kSectorsPerTrack = 16;
        constexpr size_t  kBlocksPerTrack  = 8;
        constexpr int     kMap[]           = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

        std::vector<Byte> out (bytes.size());
        size_t            blocks = bytes.size() / (2 * kSector);



        for (size_t block = 0; block < blocks; block++)
        {
            size_t  track = block / kBlocksPerTrack;



            for (size_t half = 0; half < 2; half++)
            {
                size_t  proSector = (block % kBlocksPerTrack) * 2 + half;
                size_t  dosAt     = (track * kSectorsPerTrack + (size_t) kMap[proSector]) * kSector;
                size_t  proAt     = block * 2 * kSector + half * kSector;
                size_t  from      = toProDos ? dosAt : proAt;
                size_t  to        = toProDos ? proAt : dosAt;



                std::copy_n (bytes.begin() + (ptrdiff_t) from, kSector, out.begin() + (ptrdiff_t) to);
            }
        }

        return out;
    }


    static std::vector<Byte> ToProDosOrder (const std::vector<Byte> & dosOrder) { return Reorder (dosOrder, true); }
    static std::vector<Byte> ToDosOrder    (const std::vector<Byte> & proOrder) { return Reorder (proOrder, false); }


    static bool RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 1'000'000ULL;

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


    static std::vector<PortConfig> MakeScsiPorts (int id, const std::string & image)
    {
        std::vector<PortConfig>  ports (AppleScsiCard::kTargets);



        ports[(size_t) id].device = kpszHd20scDevice;
        ports[(size_t) id].image  = image;

        return ports;
    }


    static void Boot (TestMachine & machine)
    {
        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);
    }


    //  A PRODOS VOLUME ON A HARD DISK AT ID 6 BOOTS through Apple's SCSI card
    //  ROM in slot 7 of an enhanced //e with no floppy controller.
    TEST_METHOD (ScsiCard_BootsProDosFromAHardDisk)
    {
        constexpr Byte     kRead6 = 0x08;

        FakeDiskFileIo     io;
        std::vector<Byte>  image = ToProDosOrder (GuestSession::RequireProDosUsersDisk());
        std::vector<Byte>  log;



        io.files[kHardDiskPath]  = image;
        io.stamps[kHardDiskPath] = FileStamp { image.size(), 1 };

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty,
                              { { 7, kpszAppleScsiCardDevice, MakeScsiPorts (6, kHardDiskPath) } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsTrue (machine.GetRefs().scsiCards[0]->HasFirmware(), L"the card's ROM is in the fixtures");

        Boot (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), L"ProDOS boots from the hard disk");

        log = machine.GetRefs().scsiCards[0]->GetTarget (6).GetCommandLog();

        Assert::IsTrue (std::count (log.begin(), log.end(), kRead6) > 1, L"the boot read the hard disk's blocks");
    }


    //  A DIRECTORY THE GUEST CREATES ON THE HARD DISK IS IN THE IMAGE, and the
    //  volume is clean; a second hard disk at another ID is online.
    TEST_METHOD (ScsiCard_GuestWritesReachTheImageAndTwoIdsAreOnline)
    {
        constexpr const char *  kSecondPath = "C:\\disks\\second.po";

        FakeDiskFileIo            io;
        FixtureProvider           fixtures;
        std::vector<Byte>         merlin;
        std::vector<Byte>         second;
        BlankDiskSpec             spec;
        VolumeListing             listing;
        VolumeIntegrityReport     report;
        std::vector<PortConfig>   ports;
        std::vector<std::string>  rows;
        bool                      found    = false;



        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-b.dsk", merlin));

        spec.format     = DiskFormat::Po;
        spec.contents   = BlankDiskContents::ProDos;
        spec.volumeName = "SECOND";
        AssertSucceeded (BlankDiskBuilder::Build (spec, {}, second));

        merlin = ToProDosOrder (merlin);
        io.files[kHardDiskPath]  = merlin;
        io.stamps[kHardDiskPath] = FileStamp { merlin.size(), 1 };
        io.files[kSecondPath]    = second;
        io.stamps[kSecondPath]   = FileStamp { second.size(), 1 };

        ports = MakeScsiPorts (6, kHardDiskPath);
        ports[5].device = kpszHd20scDevice;
        ports[5].image  = kSecondPath;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 7, kpszAppleScsiCardDevice, ports } }, io);

        Boot (machine);
        Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"BASIC.SYSTEM's prompt");

        Assert::IsTrue (GuestSession::TryPageToPrompt (machine, rows), L"the disk's STARTUP pages to the prompt");

        GuestSession::TypeAndCollect (machine, "CREATE CASSODIR");
        rows = GuestSession::TypeAndCollect (machine, "CAT /SECOND");

        Assert::IsTrue  (GuestSession::AnyRowContains (rows, "/SECOND"), L"the hard disk at ID 5 is online");
        AssertSucceeded (machine.GetRefs().scsiCards[0]->GetTarget (6).GetMedia().Flush());

        merlin = ToDosOrder (io.files[kHardDiskPath]);

        AssertSucceeded (ProDosVolume (merlin).Enumerate (listing));
        AssertSucceeded (ProDosVolume (merlin).BuildIntegrityReport (report));

        for (const FileEntry & entry : listing.entries)
        {
            found = found || (entry.name == "CASSODIR" && entry.isDirectory);
        }

        Assert::IsTrue (found, L"the guest's directory is in the image");
        Assert::IsTrue (report.IsClean(), L"the volume is clean after the guest's write");
    }


    //  A PRODOS VOLUME ON A 5 MB PROFILE BOOTS through Apple's interface card
    //  ROM in slot 5 of an enhanced //e, and a directory the guest creates is
    //  in the image afterward.
    TEST_METHOD (ProFile_BootsProDosAndWrites)
    {
        FakeDiskFileIo           io;
        FixtureProvider          fixtures;
        std::vector<Byte>        merlin;
        std::vector<PortConfig>  ports (1);
        std::vector<std::string> rows;
        VolumeListing            listing;
        bool                     found = false;



        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-b.dsk", merlin));

        merlin = ToProDosOrder (merlin);
        io.files[kHardDiskPath]  = merlin;
        io.stamps[kHardDiskPath] = FileStamp { merlin.size(), 1 };

        ports[0].device = kpszProFile5Device;
        ports[0].image  = kHardDiskPath;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszProFileCardDevice, ports } }, io);

        Assert::IsTrue (machine.GetBlockMediaProblems().empty());
        Assert::IsTrue (machine.GetRefs().proFileCards[0]->HasFirmware(), L"the card's ROM is in the fixtures");

        Boot (machine);
        Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"ProDOS and BASIC.SYSTEM boot from the ProFile");
        Assert::IsTrue (GuestSession::TryPageToPrompt (machine, rows), L"the disk's STARTUP pages to the prompt");

        GuestSession::TypeAndCollect (machine, "CREATE PROFILEDIR");
        GuestSession::TypeAndCollect (machine, "CAT");

        AssertSucceeded (ProDosVolume (ToDosOrder (io.files[kHardDiskPath])).Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            found = found || (entry.name == "PROFILEDIR" && entry.isDirectory);
        }

        Assert::IsTrue (found, L"the guest's directory is in the image");
    }


    //  THE 10 MB PROFILE BOOTS THROUGH ITS OWN ROM, 341-0299-B, whose block
    //  device table leaves the block count to the driver.
    TEST_METHOD (ProFile10_Boots)
    {
        FakeDiskFileIo           io;
        std::vector<Byte>        image = ToProDosOrder (GuestSession::RequireProDosUsersDisk());
        std::vector<PortConfig>  ports (1);



        io.files[kHardDiskPath]  = image;
        io.stamps[kHardDiskPath] = FileStamp { image.size(), 1 };

        ports[0].device = kpszProFile10Device;
        ports[0].image  = kHardDiskPath;

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszProFileCardDevice, ports } }, io);

        Assert::AreEqual (19456u, machine.GetRefs().proFileCards[0]->GetDrive().GetCapacity());

        Boot (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), L"ProDOS boots from the 10 MB ProFile");
    }


    //  THE PROFILE CARD BOOTS ON THE ][+ AND THE UNENHANCED //E with PR#5,
    //  their ROMs looking for the Disk II's signature at power-on.
    TEST_METHOD (ProFile_BootsOnEachHostMachine)
    {
        std::vector<Byte>  image = ToProDosOrder (GuestSession::RequireProDosUsersDisk());



        for (const char * machineId : { "Apple2Plus", "Apple2e" })
        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports (1);



            io.files[kHardDiskPath]  = image;
            io.stamps[kHardDiskPath] = FileStamp { image.size(), 1 };

            ports[0].device = kpszProFile5Device;
            ports[0].image  = kHardDiskPath;

            TestMachine  machine (machineId, TestMachine::Slots::Empty, { { 5, kpszProFileCardDevice, ports } }, io);

            Boot (machine);

            Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"the machine stops at BASIC");
            KeystrokeInjector::InjectLine (machine, "PR#5");

            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), TextEncoding::NarrowToWide (machineId).c_str());
        }
    }


    //  THE CARD BOOTS ON THE ][+ AND THE UNENHANCED //E, the machines its
    //  host list has besides the enhanced //e.
    TEST_METHOD (ScsiCard_BootsOnEachHostMachine)
    {
        std::vector<Byte>  image = ToProDosOrder (GuestSession::RequireProDosUsersDisk());



        for (const char * machineId : { "Apple2Plus", "Apple2e" })
        {
            FakeDiskFileIo  io;



            io.files[kHardDiskPath]  = image;
            io.stamps[kHardDiskPath] = FileStamp { image.size(), 1 };

            TestMachine  machine (machineId, TestMachine::Slots::Empty,
                                  { { 7, kpszAppleScsiCardDevice, MakeScsiPorts (6, kHardDiskPath) } }, io);

            Boot (machine);

            //  The ][+'s and the unenhanced //e's ROMs boot only a slot with
            //  the Disk II's signature, so the card is started as its manual
            //  starts it.
            Assert::IsTrue (RunUntilScreenShows (machine, "]", kBootCap), L"the machine stops at BASIC");
            KeystrokeInjector::InjectLine (machine, "PR#7");

            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), TextEncoding::NarrowToWide (machineId).c_str());
        }
    }


    //  THE CARD'S DELAY BEFORE THE FLOPPY BOOTS: emulated time from power-on
    //  to the Disk II ROM's first instruction, with no card, with a card and
    //  no image, and with a blank hard disk at ID 6, logged for the record.
    TEST_METHOD (ScsiCard_DelayBeforeTheFloppyBoots)
    {
        constexpr uint64_t  kCap         = 20'000'000ULL;
        constexpr Word      kDiskRomPage = 0xC600;
        constexpr Word      kPageMask    = 0xFF00;
        constexpr size_t    kBlankBlocks = 65535;

        std::vector<Byte>   blank (kBlankBlocks * BlockDeviceMedia::kBlockBytes, 0);
        std::string         report;



        for (int setup = 0; setup < 3; setup++)
        {
            FakeDiskFileIo                       io;
            std::vector<TestMachine::BlockCard>  cards;
            uint64_t                             cycles = 0;



            io.files[kHardDiskPath]  = blank;
            io.stamps[kHardDiskPath] = FileStamp { blank.size(), 1 };

            if (setup > 0)
            {
                cards.push_back ({ 7, kpszAppleScsiCardDevice, MakeScsiPorts (6, setup == 2 ? kHardDiskPath : "") });
            }

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::DiskOnly, cards, io);

            Boot (machine);

            while (cycles < kCap && (machine.GetCpu()->GetPC() & kPageMask) != kDiskRomPage)
            {
                cycles += machine.StepOne();
            }

            Assert::IsTrue (cycles < kCap, L"the Disk II ROM runs");

            report += std::format ("setup {}: {} cycles, {:.3f} s\n", setup, cycles, (double) cycles / machine.GetConfig().clockSpeed);
        }

        Logger::WriteMessage (report.c_str());
    }


    //  WITH NO IMAGE ON THE CARD, AND WITH A BLANK BOOT BLOCK, THE FLOPPY IN
    //  SLOT 6 BOOTS.
    TEST_METHOD (ScsiCard_PassesToTheFloppyWithNoBootableDisk)
    {
        constexpr size_t   kBlankBlocks = 2048;

        std::vector<Byte>  floppy = GuestSession::RequireProDosUsersDisk();



        for (bool withBlankDisk : { false, true })
        {
            FakeDiskFileIo     io;
            std::vector<Byte>  blank (kBlankBlocks * BlockDeviceMedia::kBlockBytes, 0);



            io.files[kHardDiskPath]  = blank;
            io.stamps[kHardDiskPath] = FileStamp { blank.size(), 1 };

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::DiskOnly,
                                  { { 7, kpszAppleScsiCardDevice, MakeScsiPorts (6, withBlankDisk ? kHardDiskPath : "") } }, io);

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "users.dsk", DiskFormat::Dsk, floppy));
            Boot (machine);

            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap),
                            withBlankDisk ? L"the floppy boots past a blank hard disk" : L"the floppy boots past an empty card");
        }
    }
};
