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
#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Machines/Apple2/Apple2cPlus/MigChip.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/LironCard.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/StockDisk800K.h"
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
    static constexpr const char *  kpszTrollSportsMath = "TrollSportsMath.woz";
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
            spent += host.GetRunner().RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }


    //  The machine runs a frame's cycles at a time, as the shell runs it, so
    //  the drives catch up once a frame.
    static void RunFrames (MachineHost & host, uint64_t cycles)
    {
        constexpr uint64_t  kFrameCycles = 17'030ULL;

        uint64_t  ran = 0;



        while (ran < cycles)
        {
            ran += host.GetRunner().RunCycles (std::min (kFrameCycles, cycles - ran));
        }
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

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
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
        machine.GetRunner().RunCycles (kDeselectCycles);

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
        machine.GetRunner().RunCycles (4'000'000);

        Assert::IsFalse (card.GetDrive (0).GetMechanism().HasDisk(), L"the CONTROL call ejects the disk");
    }


    //  AN 800K DISK IN A UNIDISK 3.5 ON THE //C'S DISK PORT BOOTS through the
    //  //c's ROM 4 SmartPort firmware, the built-in drive being empty.
    TEST_METHOD (Apple2c_BootsProDosFromAUniDiskOnTheDiskPort)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;
        std::vector<PortConfig>  diskPort (1);



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);
        diskPort[0].name = "disk";
        diskPort[0].chain.resize (1);
        diskPort[0].chain[0].device = kpszUniDisk35Device;
        diskPort[0].chain[0].image  = kDiskPath;

        TestMachine  machine ("Apple2c", diskPort, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::IsNotNull (machine.GetRefs().apple2cSmartPort);
        Assert::AreEqual ((size_t) 1, machine.GetRefs().apple2cSmartPort->GetPort().GetDriveCount());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the UniDisk on the disk port");
    }


    //  A 5.25-INCH DRIVE AT THE END OF THE //C'S CHAIN, behind an empty UniDisk
    //  3.5, is drive 2 of port 6: DOS 3.3, booted from the built-in drive,
    //  saves a program to it and catalogs it there.
    TEST_METHOD (Apple2c_FloppyBehindAUniDiskIsDriveTwo)
    {
        FakeDiskFileIo             io;
        std::vector<PortConfig>    diskPort (1);
        std::vector<std::string>   rows;



        diskPort[0].name = "disk";
        diskPort[0].chain.resize (2);
        diskPort[0].chain[0].device = kpszUniDisk35Device;
        diskPort[0].chain[1].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2c", diskPort, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::AreEqual ((size_t) 1, machine.GetRefs().apple2cSmartPort->GetPort().GetDriveCount(), L"the UniDisk is on the chain");
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "second.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));

        GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());

        rows = GuestSession::TypeAndCollect (machine, "10 PRINT \"TWO\"");
        rows = GuestSession::TypeAndCollect (machine, "SAVE DRIVETWO,D2");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"DOS saves to drive 2 without an error");

        rows = GuestSession::TypeAndCollect (machine, "CATALOG,D2");

        Assert::IsTrue (GuestSession::AnyRowContains (rows, "DRIVETWO"), L"drive 2's catalog lists the program");
    }


    //  A //C WITH A WRITE-PROTECTED DISK IN DRIVE 2 starts at once. At power-on
    //  the ROM selects drive 2 and reads the IWM's sense line; with it high,
    //  as a write-protected disk sets it, the ROM's SmartPort probe writes the
    //  bus and polls the HANDSHAKE register for each byte.
    TEST_METHOD (Apple2c_WithAProtectedDiskInDriveTwo_GetsPastTheSmartPortProbe)
    {
        constexpr uint64_t  kStartCap = 3'000'000;

        TestMachine  machine ("Apple2c");



        Assert::IsNull (machine.GetRefs().apple2cSmartPort, L"no chain on the disk port");
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "boot.dsk",   DiskFormat::Dsk, GuestSession::RequireDos33Master()));
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "second.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));
        machine.GetDiskStore().GetImage (6, 1)->SetUserWriteProtected (true);

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "Apple", kStartCap), L"the banner is on the screen after power-on");
    }


    //  A //C PLUS POWER CYCLED AGAIN after running a while starts at once. The
    //  power cycle zeroes the Apple's cycle count, and the disk port's clock,
    //  which follows that count, advances from the time it had reached.
    TEST_METHOD (Apple2cPlus_SecondPowerCycleStartsAtOnce)
    {
        constexpr uint64_t  kRanFirst = 20'000'000;
        constexpr uint64_t  kStartCap = 3'000'000;

        TestMachine  machine ("Apple2cPlus");



        machine.PowerCycle();
        Assert::IsTrue (RunUntilScreenShows (machine, "Apple", kStartCap), L"the banner is on the screen after the first power-on");

        machine.GetRunner().RunCycles (kRanFirst);
        machine.PowerCycle();

        Assert::IsTrue (RunUntilScreenShows (machine, "Apple", kStartCap), L"and as soon after the second");
    }

    //  THE //C PLUS BOOTS AN 800K DISK IN ITS INTERNAL APPLE 3.5 DRIVE through
    //  ROM 05, the IWM driving the drive through the MIG at the accelerator's
    //  4 MHz.
    TEST_METHOD (Apple2cPlus_BootsFromTheInternalDrive)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;
        std::vector<PortConfig>  internal (1);



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);
        internal[0].name   = "internal";
        internal[0].device = kpszApple35DriveDevice;
        internal[0].image  = kDiskPath;

        TestMachine  machine ("Apple2cPlus", internal, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::IsNotNull (machine.GetRefs().apple2cPlusDiskPort);
        Assert::IsNotNull (machine.GetAccelerator());
        Assert::IsTrue (machine.GetRefs().apple2cPlusDiskPort->GetInternalDrive().HasDisk());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the internal drive");
    }


    //  THE 800K STOCK DISK BOOTS in the //c Plus's internal drive and in an
    //  Apple 3.5 Drive on the //e's card. It is a 1,600-block volume built
    //  from a 140K ProDOS disk, here the User's Disk.
    TEST_METHOD (StockDisk800K_Boots)
    {
        std::vector<Byte>  proDosOrder = Make800K (GuestSession::RequireProDosUsersDisk());
        std::vector<Byte>  image;



        proDosOrder.resize (kBlocks140K * 2 * kSector);

        AssertSucceeded (StockDisk800K::Build (proDosOrder, image));

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;
            std::vector<PortConfig>  internal (1);



            Attach (io, image, ports);
            internal[0].name   = "internal";
            internal[0].device = kpszApple35DriveDevice;
            internal[0].image  = kDiskPath;

            TestMachine  machine ("Apple2cPlus", internal, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"the stock disk boots in the //c Plus's internal drive");
        }

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, image, ports);

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"the stock disk boots from the //e's 3.5 controller card");
        }
    }


    //  A //C PLUS WITH NO DISK IS QUIET after ROM 05 reports that it can't find
    //  a bootable disk: the motor-on commands of its startup don't turn an
    //  empty drive's spindle, so the motors are stopped and there is no motor
    //  sound.
    TEST_METHOD (Apple2cPlus_EmptyDrivesStayStopped)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (2);



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (1);
        ports[1].chain[0].device = kpszApple35DriveDevice;

        TestMachine            machine ("Apple2cPlus", ports, io);
        Apple2cPlusDiskPort  & port = *machine.GetRefs().apple2cPlusDiskPort;

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "UNABLE TO FIND A BOOTABLE DISK", kBootCap), L"ROM 05 reports that there is no bootable disk");
        Assert::IsFalse (port.GetInternalDrive().IsMotorOn(), L"the internal drive's motor is stopped");
        Assert::IsFalse (port.GetExternalDrive (0).IsMotorOn(), L"the Apple 3.5 Drive's motor is stopped");
    }


    //  A //C PLUS'S INTERNAL DRIVE STOPS after ROM 05 reads a disk that isn't
    //  bootable: ROM 05 disables the drive, and a disabled drive's motor
    //  stops. ROM 05 sends the motor-off command only before an eject.
    TEST_METHOD (Apple2cPlus_InternalDriveStopsAfterANonBootableDisk)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  scratch;
        std::vector<PortConfig>  ports (1);



        Attach (io, std::vector<Byte> (kBlocks800K * Gcr35Track::kDataBytes, 0), scratch);
        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[0].image  = kDiskPath;

        TestMachine            machine ("Apple2cPlus", ports, io);
        Apple2cPlusDiskPort  & port = *machine.GetRefs().apple2cPlusDiskPort;

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "UNABLE TO FIND A BOOTABLE DISK", kBootCap), L"ROM 05 reports that the disk isn't bootable");
        Assert::IsFalse (port.GetInternalDrive().IsMotorOn(), L"the internal drive's motor is stopped");
    }


    //  EACH 3.5-INCH DRIVE'S MOTOR STOPS once the ProDOS Users Disk's menu is
    //  up and the drive is idle: the //c Plus's ROM 05 and the 3.5 controller card's
    //  firmware disable the drive after each access, and the UniDisk 3.5's
    //  firmware deselects its mechanism after a timeout. The motor stops a
    //  second after the drive is disabled.
    TEST_METHOD (MotorsStopWhenIdleAfterABoot)
    {
        constexpr uint64_t  kIdle = 8'000'000ULL;

        std::vector<Byte>  image = Make800K (GuestSession::RequireProDosUsersDisk());



        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  scratch;
            std::vector<PortConfig>  ports (1);



            Attach (io, image, scratch);
            ports[0].name   = "internal";
            ports[0].device = kpszApple35DriveDevice;
            ports[0].image  = kDiskPath;

            TestMachine  machine ("Apple2cPlus", ports, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "PLEASE SELECT ONE OF THE ABOVE", kBootCap), L"the //c Plus boots to the menu");
            RunFrames (machine, kIdle);
            Assert::IsFalse (machine.GetRefs().apple2cPlusDiskPort->GetInternalDrive().IsMotorOn(), L"the //c Plus's internal drive stops");
        }

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, image, ports);

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "PLEASE SELECT ONE OF THE ABOVE", kBootCap), L"the 3.5 controller card boots to the menu");
            RunFrames (machine, kIdle);
            Assert::IsFalse (machine.GetRefs().apple35Cards[0]->GetCardMemory().GetDrive (0).IsMotorOn(), L"the Apple 3.5 Drive stops");
        }

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, image, ports);
            ports.resize (1);
            ports[0].device = kpszUniDisk35Device;

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "PLEASE SELECT ONE OF THE ABOVE", kBootCap), L"the UniDisk 3.5 boots to the menu");
            RunFrames (machine, kIdle);
            Assert::IsFalse (machine.GetRefs().lironCards[0]->GetDrive (0).GetMechanism().IsMotorOn(), L"the UniDisk 3.5's mechanism stops");
        }
    }


    //  A 140K DISK IN A 5.25-INCH DRIVE at the end of the //c Plus's disk port
    //  chain boots with the internal drive empty: drive 1 returns nibbles
    //  with 3.5 select off, so ROM 05 runs the slot 6 firmware. The drive is
    //  the disk port's device, as on the //c.
    TEST_METHOD (Apple2cPlus_BootsFromAnExternalFloppy)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (2);



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::IsNotNull (machine.GetRefs().diskController, L"the 5.25-inch drive has a controller");
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "SYSTEM MASTER", kBootCap), L"DOS 3.3 boots from the external 5.25-inch drive");
    }


    //  A WRITE-PROTECTED FLOPPY in the //c Plus's external drive boots: the
    //  drive's sense line is high through ROM 05's reset, which writes the
    //  IWM's mode and reads it back.
    TEST_METHOD (Apple2cPlus_BootsAWriteProtectedFloppy)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (2);



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));
        machine.GetRefs().diskController->GetDrive (0).GetDisk()->SetUserWriteProtected (true);

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "SYSTEM MASTER", kBootCap), L"DOS 3.3 boots from the write-protected floppy");
    }


    //  DOS 3.3 SAVES A PROGRAM to the //c Plus's external 5.25-inch drive and
    //  catalogs it: the accelerator runs the machine at 1 MHz around the
    //  slot 6 accesses, so the write's timing is the //c's.
    TEST_METHOD (Apple2cPlus_SavesToTheExternalFloppy)
    {
        FakeDiskFileIo             io;
        std::vector<PortConfig>    ports (2);
        std::vector<std::string>   rows;



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (1);
        ports[1].chain[0].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());

        rows = GuestSession::TypeAndCollect (machine, "10 PRINT \"PLUS\"");
        rows = GuestSession::TypeAndCollect (machine, "SAVE CPLUS");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"DOS saves without an error");

        rows = GuestSession::TypeAndCollect (machine, "CATALOG");

        Assert::IsTrue (GuestSession::AnyRowContains (rows, "CPLUS"), L"the catalog lists the program DOS saved");
    }


    //  WITH A DISK IN BOTH, the //c Plus boots from its internal drive: ROM
    //  05's startup order has the internal drive ahead of port 6.
    TEST_METHOD (Apple2cPlus_BootsTheInternalDriveAheadOfTheFloppy)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  scratch;
        std::vector<PortConfig>  ports (2);



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), scratch);
        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[0].image  = kDiskPath;
        ports[1].name   = "disk";
        ports[1].chain.resize (1);
        ports[1].chain[0].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the internal drive");
    }


    //  AN EMPTY UNIDISK 3.5 AHEAD OF A 5.25-INCH DRIVE on the //c Plus's port
    //  passes the port through: with no disk on the SmartPort bus, the floppy
    //  boots.
    TEST_METHOD (Apple2cPlus_BootsAFloppyBehindAnEmptyUniDisk)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (2);



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (2);
        ports[1].chain[0].device = kpszUniDisk35Device;
        ports[1].chain[1].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::AreEqual ((size_t) 1, machine.GetRefs().apple2cPlusDiskPort->GetPort().GetDriveCount(), L"the UniDisk is on the chain");
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "SYSTEM MASTER", kBootCap), L"DOS 3.3 boots from the 5.25-inch drive behind the UniDisk");
    }


    //  AN 800K DISK IN AN EXTERNAL APPLE 3.5 DRIVE, first on the //c Plus's
    //  disk port chain, boots with the internal drive empty: the drive is
    //  IWM drive 1 with the MIG's 3.5 select on.
    TEST_METHOD (Apple2cPlus_BootsFromAnExternalApple35Drive)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  scratch;
        std::vector<PortConfig>  ports (2);



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), scratch);
        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (1);
        ports[1].chain[0].device = kpszApple35DriveDevice;
        ports[1].chain[0].image  = kDiskPath;

        TestMachine  machine ("Apple2cPlus", ports, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::AreEqual (1, machine.GetRefs().apple2cPlusDiskPort->GetExternalCount());
        Assert::IsTrue (machine.GetRefs().apple2cPlusDiskPort->GetExternalDrive (0).HasDisk());

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the external Apple 3.5 Drive");
    }


    //  A UNIDISK 3.5 BEHIND AN EXTERNAL APPLE 3.5 DRIVE is on the SmartPort
    //  bus with 3.5 select off, and takes its unit number at startup: ROM 05
    //  numbers the internal drive 1 and the Apple 3.5 Drive 2. ROM 05 boots
    //  from the Apple 3.5 Drive.
    TEST_METHOD (Apple2cPlus_UniDiskBehindAnApple35DriveIsOnline)
    {
        constexpr Word  kUnitNumber = 0x0B;

        FakeDiskFileIo           io;
        std::vector<PortConfig>  scratch;
        std::vector<PortConfig>  ports (2);
        std::vector<Byte>        image = Make800K (GuestSession::RequireProDosUsersDisk());



        Attach (io, image, scratch);
        io.files["C:\\disks\\second.po"]  = image;
        io.stamps["C:\\disks\\second.po"] = FileStamp { image.size(), 1 };
        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (2);
        ports[1].chain[0].device = kpszApple35DriveDevice;
        ports[1].chain[0].image  = kDiskPath;
        ports[1].chain[1].device = kpszUniDisk35Device;
        ports[1].chain[1].image  = "C:\\disks\\second.po";

        TestMachine  machine ("Apple2cPlus", ports, io);
        SmartPortHost  & port = machine.GetRefs().apple2cPlusDiskPort->GetPort();

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        Assert::AreEqual ((size_t) 1, port.GetDriveCount(), L"the UniDisk is on the chain");

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS 1.0", kBootCap), L"ProDOS boots from the Apple 3.5 Drive");
        Assert::AreEqual ((Byte) 0x83, port.GetDrive (0).Read (kUnitNumber), L"the UniDisk is unit 3, after the internal drive and the Apple 3.5 Drive");
    }

    //  A 5.25-INCH DRIVE BEHIND AN EMPTY EXTERNAL APPLE 3.5 DRIVE boots: the
    //  Apple 3.5 Drive is drive 1 with 3.5 select on, and the floppy is
    //  drive 1 with it off.
    TEST_METHOD (Apple2cPlus_BootsAFloppyBehindAnEmptyApple35Drive)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports (2);



        ports[0].name   = "internal";
        ports[0].device = kpszApple35DriveDevice;
        ports[1].name   = "disk";
        ports[1].chain.resize (2);
        ports[1].chain[0].device = kpszApple35DriveDevice;
        ports[1].chain[1].device = kpszDiskIicDevice;

        TestMachine  machine ("Apple2cPlus", ports, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));

        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "SYSTEM MASTER", kBootCap), L"DOS 3.3 boots from the 5.25-inch drive behind the Apple 3.5 Drive");
    }


    //  AN 800K PRODOS DISK BOOTS through Apple's 3.5 controller card firmware
    //  in slot 5 of an enhanced //e with no floppy controller.
    TEST_METHOD (Apple35Card_BootsProDosFrom800K)
    {
        FakeDiskFileIo           io;
        std::vector<PortConfig>  ports;



        Attach (io, Make800K (GuestSession::RequireProDosUsersDisk()), ports);

        TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
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

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());
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

        Assert::IsTrue (machine.GetStorage().GetBlockMediaProblems().empty());

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


    //  The hi-res page 1 bytes, hashed: the copy-protected disk draws its
    //  screens on the hi-res page, which the text scraper can't read.
    static uint32_t GetHiResHash (MachineHost & host)
    {
        constexpr Word      kHiResStart = 0x2000;
        constexpr Word      kHiResEnd   = 0x4000;
        constexpr uint32_t  kFnvBasis   = 2166136261u;
        constexpr uint32_t  kFnvPrime   = 16777619u;
        uint32_t            hash        = kFnvBasis;



        for (Word at = kHiResStart; at < kHiResEnd; at++)
        {
            hash = (hash ^ host.GetMemoryBus().ReadByte (at)) * kFnvPrime;
        }

        return hash;
    }


    static bool RunUntilHiResIs (MachineHost & host, uint32_t hash, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice  = 1'000'000ULL;
        uint64_t            spent   = 0;
        bool                isThere = false;



        while (!isThere && spent < cycleCap)
        {
            spent  += host.GetRunner().RunCycles (kSlice);
            isThere = GetHiResHash (host) == hash;
        }

        return isThere;
    }


    //  A COPY-PROTECTED 800K WOZ DISK, Troll Sports Math (1991), runs past
    //  its protection to the name prompt in the //c Plus's internal drive and
    //  in a UniDisk 3.5 on the //e's interface card. The disk runs only on
    //  those drives and a IIgs: on the 3.5 controller card it falls to the
    //  BASIC prompt with a screen of letters, as it does in MAME's
    //  `superdrive` card. The name prompt's hash is of the screen on MAME's
    //  //c Plus at the same point (validation.md).
    TEST_METHOD (TrollSportsMath_ProtectedWozRunsOnTheDrivesItSupports)
    {
        constexpr uint32_t  kNamePromptHash = 0x35B52330;
        constexpr uint64_t  kRunCap         = 60'000'000ULL;
        std::vector<Byte>   woz             = GuestSession::FindTestDisk (kpszTrollSportsMath);



        Assert::IsFalse (woz.empty(), L"TrollSportsMath.woz is REQUIRED. To get it, run scripts/FetchStockDisks.ps1, which RunTests.ps1 -Scenario also runs");

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;
            std::vector<PortConfig>  internal (1);



            Attach (io, woz, ports);
            internal[0].name   = "internal";
            internal[0].device = kpszApple35DriveDevice;
            internal[0].image  = kDiskPath;

            TestMachine  machine ("Apple2cPlus", internal, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilHiResIs (machine, kNamePromptHash, kRunCap), L"the name prompt, from the //c Plus's internal drive");
        }

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, woz, ports);
            ports[0].device = kpszUniDisk35Device;

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszLironCardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilHiResIs (machine, kNamePromptHash, kRunCap), L"the name prompt, from a UniDisk 3.5 on the //e's card");
        }

        {
            FakeDiskFileIo           io;
            std::vector<PortConfig>  ports;



            Attach (io, woz, ports);

            TestMachine  machine ("Apple2eEnhanced", TestMachine::Slots::Empty, { { 5, kpszApple35CardDevice, ports } }, io);

            machine.PowerCycle();
            GuestSession::StartFromPowerOn (machine);
            Assert::IsTrue (RunUntilScreenShows (machine, "]", kRunCap), L"on the 3.5 controller card the disk falls to BASIC, as in MAME");
            Assert::IsFalse (GetHiResHash (machine) == kNamePromptHash);
        }
    }
};
