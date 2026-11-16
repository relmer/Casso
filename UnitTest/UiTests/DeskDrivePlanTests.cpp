#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/DriveImageRule.h"
#include "Ui/Scene/DeskDrivePlan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskDrivePlanTests
//
//  Where the desk puts each drive: under the monitor, in the left or right
//  stack, or on the drive bar, for the two classes' settings.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskDrivePlanTests)
{
public:

    static constexpr float  kMonitorII = 370.0f;
    static constexpr float  kMonitorC  = 241.0f;

    using M = DeskDriveModel;
    using S = DeskDriveSpot;
    using P = DeskClassPlace;


    static const DeskDrivePlacement & Find (const std::vector<DeskDrivePlacement> & plan, M model, int drive = 0)
    {
        for (const DeskDrivePlacement & placed : plan)
        {
            if (placed.drive.model == model && placed.drive.drive == drive)
            {
                return placed;
            }
        }

        Assert::Fail (L"the drive is in the plan");
    }


    //  Two Disk IIs sit under the monitor as now; the HD20SC, narrower than
    //  the //e's monitor, is on the left.
    TEST_METHOD (TwoFloppiesAndAnHd20sc_FloppiesUnderTheMonitor)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 }, { M::HardDisk20SC, 7, 6 } },
                                           P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (Find (plan, M::DiskII, 0).spot == S::UnderMonitor);
        Assert::AreEqual (0, Find (plan, M::DiskII, 0).level, L"drive 1 on the left");
        Assert::AreEqual (1, Find (plan, M::DiskII, 1).level);
        Assert::IsTrue (Find (plan, M::HardDisk20SC, 6).spot == S::LeftStack);
    }


    //  The //c's built-in drive and its external Disk IIc sit under the
    //  monitor.
    TEST_METHOD (CPair_UnderTheMonitor)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::Disk2c, 6, 0, true }, { M::Disk2c, 6, 1 } }, P::Auto, P::Auto, kMonitorC);



        Assert::IsTrue (Find (plan, M::Disk2c, 0).spot == S::UnderMonitor);
        Assert::IsTrue (Find (plan, M::Disk2c, 1).spot == S::UnderMonitor);
    }


    //  A pair that isn't two Disk IIs, such as the //c Plus's 3.5-inch drive
    //  and a Disk IIc, stacks beside the monitor.
    TEST_METHOD (MixedFloppyPair_StacksBesideTheMonitor)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::Disk2c, 6, 0 }, { M::Apple35Plus, 0, 0 } }, P::Auto, P::Auto, kMonitorC);



        Assert::IsTrue (Find (plan, M::Disk2c, 0).spot == S::RightStack);
        Assert::IsTrue (Find (plan, M::Apple35Plus, 0).spot == S::RightStack);
    }


    //  The shipped //e: two Disk IIs and a UniDisk 3.5 are three floppies, so
    //  they go on the drive bar; the HD20SC stays on the desk.
    TEST_METHOD (ThreeFloppies_GoOnTheDriveBarByDefault)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 }, { M::UniDisk35, 5, 0 }, { M::HardDisk20SC, 7, 6 } },
                                           P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (Find (plan, M::DiskII, 0).spot == S::DriveBar);
        Assert::IsTrue (Find (plan, M::UniDisk35).spot == S::DriveBar);
        Assert::AreEqual (2, Find (plan, M::UniDisk35).level, L"the narrowest at the top of the bar's stack");
        Assert::IsTrue (Find (plan, M::HardDisk20SC, 6).spot == S::LeftStack);
    }


    //  IN THE COMPACT THEMES every drive is on the drive bar when the machine
    //  has a drive other than its Disk II drives. With only Disk II drives
    //  the band has the Disk II widgets, and the plan has nothing on the bar.
    TEST_METHOD (Compact_DrivesBeyondTheDiskIiPair_GoOnTheBar)
    {
        std::vector<DeskDrive>  diskIi    = { { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 } };
        std::vector<DeskDrive>  withHd    = { { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 }, { M::HardDisk20SC, 7, 6 } };
        P                       floppies  = P::Auto;
        P                       hardDisks = P::Auto;



        DeskDrivePlan::GetCompactPlaces (diskIi, floppies, hardDisks);

        for (const DeskDrivePlacement & placed : DeskDrivePlan::Place (diskIi, floppies, hardDisks, kMonitorII))
        {
            Assert::IsTrue (placed.spot != S::DriveBar, L"the Disk II widgets, not the bar");
        }

        DeskDrivePlan::GetCompactPlaces (withHd, floppies, hardDisks);

        for (const DeskDrivePlacement & placed : DeskDrivePlan::Place (withHd, floppies, hardDisks, kMonitorII))
        {
            Assert::IsTrue (placed.spot == S::DriveBar, L"every drive on the bar");
        }
    }


    //  A single ProFile goes under the monitor, and the floppies stack on
    //  the right.
    TEST_METHOD (SingleProFile_UnderTheMonitorAndFloppiesOnTheRight)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 }, { M::ProFile, 2, 0 } },
                                           P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (Find (plan, M::ProFile).spot == S::UnderMonitor);
        Assert::IsTrue (Find (plan, M::DiskII, 0).spot == S::RightStack);
        Assert::IsTrue (Find (plan, M::DiskII, 1).spot == S::RightStack);
    }


    //  The HD20SC is as wide as the //c's monitor, so it goes under it.
    TEST_METHOD (Hd20sc_UnderTheMonitorCOnly)
    {
        auto  underC  = DeskDrivePlan::Place ({ { M::HardDisk20SC, 7, 6 } }, P::Auto, P::Auto, kMonitorC);
        auto  underII = DeskDrivePlan::Place ({ { M::HardDisk20SC, 7, 6 } }, P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (underC[0].spot == S::UnderMonitor);
        Assert::IsTrue (underII[0].spot == S::LeftStack);
    }


    //  With every floppy on the desk and more than two of them, the hard
    //  disks stack on the left and the floppies on the right, narrowest on
    //  top, and nothing is under the monitor.
    TEST_METHOD (ManyOfEach_StacksOnBothSidesNarrowestOnTop)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::UniDisk35, 5, 0 }, { M::Apple35Drive, 4, 0 },
                                             { M::HardDisk20SC, 7, 6 }, { M::ProFile, 2, 0 } },
                                           P::Desk, P::Desk, kMonitorII);



        for (const DeskDrivePlacement & placed : plan)
        {
            Assert::IsTrue (placed.spot != S::UnderMonitor);
        }

        Assert::AreEqual (0, Find (plan, M::DiskII).level, L"the widest at the bottom");
        Assert::AreEqual (1, Find (plan, M::UniDisk35).level);
        Assert::AreEqual (2, Find (plan, M::Apple35Drive).level, L"one width keeps the order given");
        Assert::IsTrue (Find (plan, M::Apple35Drive).spot == S::RightStack);

        Assert::AreEqual (0, Find (plan, M::ProFile).level);
        Assert::AreEqual (1, Find (plan, M::HardDisk20SC, 6).level);
        Assert::IsTrue (Find (plan, M::ProFile).spot == S::LeftStack);
    }


    //  The drives come from the Disk II count, the //c Plus's built-in drive
    //  and its chain, and the storage cards' ports; a disabled card and the
    //  chain's 5.25-inch drive, which the Disk II count covers, are left out.
    TEST_METHOD (Collect_ListsEveryDrive)
    {
        MachineConfig  config;
        PortConfig     internal;
        PortConfig     disk;
        SlotConfig     scsi;
        SlotConfig     liron;



        internal.name   = "internal";
        internal.device = kpszApple35DriveDevice;
        disk.name       = "disk";
        disk.chain.resize (2);
        disk.chain[0].device = kpszUniDisk35Device;
        disk.chain[1].device = kpszDiskIicDevice;
        config.ports  = { internal, disk };

        scsi.slot   = 7;
        scsi.device = kpszAppleScsiCardDevice;
        scsi.ports.resize (7);
        scsi.ports[6].device = kpszHd20scDevice;
        scsi.ports[2].device = kpszHd20scDevice;
        liron.slot    = 5;
        liron.device  = kpszLironCardDevice;
        liron.enabled = false;
        liron.ports.resize (1);
        liron.ports[0].device = kpszUniDisk35Device;
        config.slots = { scsi, liron };

        auto  drives = DeskDrivePlan::Collect (config, 1, true);

        Assert::AreEqual ((size_t) 5, drives.size());
        Assert::IsTrue (drives[0].model == M::Disk2c);
        Assert::IsFalse (drives[0].builtIn, L"with a built-in 3.5-inch drive, the 5.25-inch drive is on the port");
        Assert::IsTrue (drives[1].model == M::Apple35Plus);
        Assert::IsTrue (drives[2].model == M::UniDisk35);
        Assert::AreEqual (1, drives[2].drive, L"after the built-in drive");
        Assert::AreEqual (2, drives[3].drive);
        Assert::AreEqual (6, drives[4].drive);
    }


    //  On the //c Plus port, drive 0 is the built-in drive, then the Apple
    //  3.5 Drives, then the UniDisks, and each drive's picker takes the
    //  images for its kind.
    TEST_METHOD (Collect_CPlusPort_Apple35DrivesComeFirst)
    {
        MachineConfig  config;
        PortConfig     internal;
        PortConfig     disk;



        internal.name   = "internal";
        internal.device = kpszApple35DriveDevice;
        disk.name       = "disk";
        disk.chain.resize (3);
        disk.chain[0].device = kpszApple35DriveDevice;
        disk.chain[1].device = kpszApple35DriveDevice;
        disk.chain[2].device = kpszUniDisk35Device;
        config.ports  = { internal, disk };

        auto  drives = DeskDrivePlan::Collect (config, 0, true);

        Assert::AreEqual ((size_t) 4, drives.size());
        Assert::IsTrue   (drives[0].model == M::Apple35Plus);
        Assert::AreEqual (0, drives[0].drive);

        for (int i = 1; i <= 2; i++)
        {
            Assert::IsTrue   (drives[i].model == M::Apple35Drive);
            Assert::AreEqual (i, drives[i].drive);
            Assert::IsTrue   (DeskDrivePlan::GetReceivingKind (drives[i].model) == ReceivingDriveKind::Apple35);
        }

        Assert::IsTrue   (drives[3].model == M::UniDisk35);
        Assert::AreEqual (3, drives[3].drive);
        Assert::IsTrue   (DeskDrivePlan::GetReceivingKind (drives[3].model) == ReceivingDriveKind::UniDisk35);
    }


    //  THE //C'S DRIVE 1 IS BUILT IN; its drive 2 is on the port.
    TEST_METHOD (Collect_TheCsFirstDriveIsBuiltIn)
    {
        MachineConfig  config;
        auto           drives = DeskDrivePlan::Collect (config, 2, true);



        Assert::AreEqual ((size_t) 2, drives.size());
        Assert::IsTrue (drives[0].builtIn);
        Assert::IsFalse (drives[1].builtIn);
    }


    //  The //c Plus's 5.25-inch drives: the disk port's device with no chain,
    //  or the chain's last entries.
    TEST_METHOD (CountDiskPortFloppies_DeviceOrChain)
    {
        MachineConfig  config;
        PortConfig     disk;



        disk.name   = "disk";
        disk.device = kpszDiskIicDevice;
        config.ports = { disk };

        Assert::AreEqual (1, config.CountDiskPortFloppies());

        config.ports[0].device = "disk-iic-drive";

        Assert::AreEqual (1, config.CountDiskPortFloppies(), L"the //c's id for its external drive");

        config.ports[0].chain.resize (3);
        config.ports[0].chain[0].device = kpszUniDisk35Device;
        config.ports[0].chain[1].device = kpszDiskIicDevice;
        config.ports[0].chain[2].device = kpszDiskIicDevice;

        Assert::AreEqual (2, config.CountDiskPortFloppies(), L"a chain lists every drive on the port");
    }


    TEST_METHOD (Place_ParsesAndFormats)
    {
        Assert::IsTrue (DeskDrivePlan::ParsePlace ("desk") == P::Desk);
        Assert::IsTrue (DeskDrivePlan::ParsePlace ("bar") == P::Bar);
        Assert::IsTrue (DeskDrivePlan::ParsePlace ("") == P::Auto);
        Assert::AreEqual ("bar", DeskDrivePlan::FormatPlace (P::Bar));
    }


    //  A single floppy stands on the right rather than under the monitor.
    TEST_METHOD (SingleFloppy_GoesOnTheRight)
    {
        auto  plus = DeskDrivePlan::Place ({ { M::Apple35Plus, 0, 0 } }, P::Auto, P::Auto, kMonitorC);
        auto  iie  = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::HardDisk20SC, 7, 6 } }, P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (plus[0].spot == S::RightStack);
        Assert::IsTrue (Find (iie, M::DiskII).spot == S::RightStack);
        Assert::IsTrue (Find (iie, M::HardDisk20SC, 6).spot == S::LeftStack);
    }


    //  With the pair set beside the monitor, two floppies stack on the right.
    TEST_METHOD (FloppyPair_BesideTheMonitorWhenSet)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 } }, P::Auto, P::Auto, kMonitorII, true);



        Assert::IsTrue (Find (plan, M::DiskII, 0).spot == S::RightStack);
        Assert::IsTrue (Find (plan, M::DiskII, 1).spot == S::RightStack);
    }


    //  A class set to the drive bar goes there whatever its size, and a
    //  class set to the desk stays there.
    TEST_METHOD (Settings_OverrideTheCount)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::HardDisk20SC, 7, 6 } }, P::Bar, P::Auto, kMonitorII);



        Assert::IsTrue (Find (plan, M::DiskII).spot == S::DriveBar);
        Assert::IsTrue (Find (plan, M::HardDisk20SC, 6).spot == S::LeftStack);
        Assert::IsTrue (DeskDrivePlan::IsOnDesk (P::Desk, 7));
        Assert::IsFalse (DeskDrivePlan::IsOnDesk (P::Auto, 3));
    }
};
