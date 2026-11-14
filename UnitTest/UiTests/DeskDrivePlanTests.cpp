#include "Pch.h"

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


    //  The shipped //e: two Disk IIs and a UniDisk 3.5 are three floppies, so
    //  they go on the drive bar; the HD20SC stays on the desk.
    TEST_METHOD (ThreeFloppies_GoOnTheDriveBarByDefault)
    {
        auto  plan = DeskDrivePlan::Place ({ { M::DiskII, 6, 0 }, { M::DiskII, 6, 1 }, { M::UniDisk35, 5, 0 }, { M::HardDisk20SC, 7, 6 } },
                                           P::Auto, P::Auto, kMonitorII);



        Assert::IsTrue (Find (plan, M::DiskII, 0).spot == S::DriveBar);
        Assert::IsTrue (Find (plan, M::UniDisk35).spot == S::DriveBar);
        Assert::AreEqual (0, Find (plan, M::UniDisk35).level, L"the widest at the bottom of the bar's stack");
        Assert::IsTrue (Find (plan, M::HardDisk20SC, 6).spot == S::LeftStack);
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

        Assert::AreEqual (0, Find (plan, M::UniDisk35).level);
        Assert::AreEqual (1, Find (plan, M::DiskII).level);
        Assert::AreEqual (2, Find (plan, M::Apple35Drive).level, L"the narrowest on top");
        Assert::IsTrue (Find (plan, M::Apple35Drive).spot == S::RightStack);

        Assert::AreEqual (0, Find (plan, M::ProFile).level);
        Assert::AreEqual (1, Find (plan, M::HardDisk20SC, 6).level);
        Assert::IsTrue (Find (plan, M::ProFile).spot == S::LeftStack);
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
