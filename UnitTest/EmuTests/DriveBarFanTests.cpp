#include "Pch.h"

#include "Shell/Layout/DriveBarFan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanTests
//
//  The drive bar's widget per class and the fan of the class's other drives
//  above it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveBarFanTests)
{
public:

    static DeskDrivePlacement  OnBar (DeskDriveModel model, int slot, int drive, int level)
    {
        DeskDrivePlacement  placement;



        placement.drive = { model, slot, drive };
        placement.spot  = DeskDriveSpot::DriveBar;
        placement.level = level;

        return placement;
    }


    //  Three floppies and two hard disks on the bar: a floppy class (from the
    //  bottom of its stack up) and a hard disk class.
    static DriveBarFan  MakeFan()
    {
        std::vector<DeskDrivePlacement>  plan;
        DriveBarFan                      fan;
        DeskDrivePlacement               onDesk = OnBar (DeskDriveModel::DiskII, 6, 1, 0);



        plan.push_back (OnBar (DeskDriveModel::ProFile,      2, 0, 1));
        plan.push_back (OnBar (DeskDriveModel::UniDisk35,    5, 0, 2));
        plan.push_back (OnBar (DeskDriveModel::DiskII,       6, 0, 0));
        plan.push_back (OnBar (DeskDriveModel::HardDisk20SC, 7, 6, 0));
        plan.push_back (OnBar (DeskDriveModel::Apple35Drive, 4, 0, 1));

        onDesk.spot = DeskDriveSpot::RightStack;
        plan.push_back (onDesk);

        fan.SetClasses (DriveBarFan::Group (plan));
        fan.Layout (100, 500, SIZE { 200, 40 }, 20, 4, 6);

        return fan;
    }


    TEST_METHOD (Group_OneListPerClass_FloppiesFirst_FromTheBottomUp)
    {
        DriveBarFan  fan = MakeFan();



        Assert::AreEqual (2, fan.GetClassCount());
        Assert::AreEqual (3, fan.GetEntryCount (0), L"the Disk II on the desk is left out");
        Assert::IsTrue (fan.GetDrive (0, 0).model == DeskDriveModel::DiskII);
        Assert::IsTrue (fan.GetDrive (0, 1).model == DeskDriveModel::Apple35Drive);
        Assert::IsTrue (fan.GetDrive (0, 2).model == DeskDriveModel::UniDisk35);
        Assert::AreEqual (2, fan.GetEntryCount (1));
        Assert::IsTrue (fan.GetDrive (1, 0).model == DeskDriveModel::HardDisk20SC);
    }


    TEST_METHOD (Layout_WidgetsInARow_FanStacksUpward)
    {
        DriveBarFan  fan   = MakeFan();
        RECT         head0 = fan.GetSlotRect (0, 0);
        RECT         head1 = fan.GetSlotRect (1, 0);
        RECT         fan1  = fan.GetSlotRect (0, 1);
        RECT         fan2  = fan.GetSlotRect (0, 2);



        Assert::AreEqual (100L, head0.left);
        Assert::AreEqual (500L, head0.top);
        Assert::AreEqual (320L, head1.left, L"one slot and a gap to the right");
        Assert::AreEqual (100L, fan1.left,  L"directly above its widget");
        Assert::AreEqual (456L, fan1.top,   L"one slot and a gap up");
        Assert::AreEqual (412L, fan2.top);
        Assert::AreEqual (440, DriveBarFan::GetRowWidth (2, 200, 40));
    }


    TEST_METHOD (Hover_OpensTheFan_AndKeepsItOpenAcrossThePanel)
    {
        DriveBarFan  fan = MakeFan();
        RECT         panel;



        Assert::IsFalse (fan.IsSlotShown (0, 1), L"closed at first");
        Assert::IsTrue  (fan.IsSlotShown (0, 0), L"the widget is always visible");

        Assert::IsTrue (fan.UpdateHover (POINT { 150, 510 }));
        Assert::AreEqual (0, fan.GetOpenClass());
        Assert::IsTrue (fan.IsSlotShown (0, 2));

        panel = fan.GetPanelRect();
        Assert::AreEqual (500L, panel.bottom, L"the panel extends to the widget's top");
        Assert::AreEqual (406L, panel.top);

        Assert::IsFalse (fan.UpdateHover (POINT { 150, 498 }), L"between a fan slot and the widget");
        Assert::IsFalse (fan.UpdateHover (POINT { 150, 420 }), L"on the top fan slot");

        Assert::IsTrue (fan.UpdateHover (POINT { 150, 300 }), L"off the panel");
        Assert::AreEqual (-1, fan.GetOpenClass());
        Assert::IsTrue (fan.GetPanelRect().right == 0);
    }


    TEST_METHOD (Hover_ClassWithOneDrive_HasNoFan)
    {
        std::vector<DeskDrivePlacement>  plan  = { OnBar (DeskDriveModel::ProFile, 2, 0, 0) };
        DriveBarFan                      fan;



        fan.SetClasses (DriveBarFan::Group (plan));
        fan.Layout (0, 100, SIZE { 200, 40 }, 20, 4, 6);

        Assert::IsFalse (fan.UpdateHover (POINT { 10, 110 }));
        Assert::AreEqual (-1, fan.GetOpenClass());
    }


    TEST_METHOD (HitTest_OnlyShownDrives)
    {
        DriveBarFan  fan   = MakeFan();
        int          c     = -1;
        int          entry = -1;



        Assert::IsFalse (fan.HitTest (POINT { 150, 420 }, c, entry), L"a closed fan is not hit");

        Assert::IsTrue (fan.HitTest (POINT { 330, 510 }, c, entry));
        Assert::AreEqual (1, c);
        Assert::AreEqual (0, entry);

        fan.UpdateHover (POINT { 150, 510 });
        Assert::IsTrue (fan.HitTest (POINT { 150, 420 }, c, entry));
        Assert::AreEqual (0, c);
        Assert::AreEqual (2, entry);
    }
};
