#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Woz35Builder.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/Gcr35Track.h"
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


    static void Attach (FakeDiskFileIo & io, const std::vector<Byte> & image, std::vector<PortConfig> & ports)
    {
        io.files[kDiskPath]  = image;
        io.stamps[kDiskPath] = FileStamp { image.size(), 1 };

        ports.assign (2, PortConfig {});
        ports[0].device = kpszApple35DriveDevice;
        ports[0].image  = kDiskPath;
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

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), L"ProDOS boots from the 800K disk");
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

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", kBootCap), L"ProDOS boots from the WOZ image");
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
