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
        fan.Layout (100, 500, SIZE { 200, 40 }, 20, 4, 6, 10);

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
        RECT         head0 = {};
        RECT         head1 = {};
        RECT         fan1  = {};
        RECT         fan2  = {};



        fan.UpdateHover (POINT { 150, 510 });

        head0 = fan.GetSlotRect (0, 0);
        head1 = fan.GetSlotRect (1, 0);
        fan1  = fan.GetSlotRect (0, 1);
        fan2  = fan.GetSlotRect (0, 2);

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

        panel = fan.GetPanelRect (0);
        Assert::AreEqual (546L, panel.bottom, L"the box takes in the widget");
        Assert::AreEqual (406L, panel.top);
        Assert::AreEqual (316L, panel.right, L"the arrow's room remains, so the box does not change width");

        Assert::IsFalse (fan.UpdateHover (POINT { 150, 498 }), L"between a fan slot and the widget");
        Assert::IsFalse (fan.UpdateHover (POINT { 150, 420 }), L"on the top fan slot");

        Assert::IsTrue (fan.UpdateHover (POINT { 150, 300 }), L"off the panel");
        Assert::AreEqual (-1, fan.GetOpenClass());
        Assert::IsFalse (fan.IsClassOpen (0));
    }


    TEST_METHOD (Panel_ClosedClass_BoxesItsWidget_WithRoomForTheArrow)
    {
        DriveBarFan  fan   = MakeFan();
        RECT         panel = fan.GetPanelRect (0);



        Assert::AreEqual (494L, panel.top,    L"the widget's top less the margin");
        Assert::AreEqual (546L, panel.bottom);
        Assert::AreEqual (316L, panel.right,  L"the margin and the arrow's room");
    }


    TEST_METHOD (Progress_SlidesTheFanUpFromBehindTheWidget)
    {
        DriveBarFan  fan = MakeFan();



        fan.UpdateHover (POINT { 150, 510 });

        fan.SetFanProgress (0.0f);
        Assert::AreEqual (500L, fan.GetSlotRect (0, 2).top, L"behind the widget at the start");

        fan.SetFanProgress (0.5f);
        Assert::AreEqual (456L, fan.GetSlotRect (0, 2).top, L"halfway up");
        Assert::AreEqual (500L, fan.GetSlotRect (0, 1).top, L"the stack rises at its full spacing, so the drive below is behind the widget");
        Assert::AreEqual (450L, fan.GetPanelRect (0).top,   L"the box grows with it");

        fan.SetFanProgress (1.0f);
        Assert::AreEqual (412L, fan.GetSlotRect (0, 2).top);
    }


    TEST_METHOD (ExpandAll_OpensEveryClass_AndIgnoresHover)
    {
        DriveBarFan  fan = MakeFan();



        fan.SetExpandAll (true);

        Assert::IsTrue  (fan.IsClassOpen (0));
        Assert::IsTrue  (fan.IsClassOpen (1));
        Assert::IsTrue  (fan.IsSlotShown (1, 1));
        Assert::IsFalse (fan.UpdateHover (POINT { 150, 300 }), L"the pointer does not close it");
        Assert::IsTrue  (fan.IsClassOpen (0));
        Assert::AreEqual (134, fan.GetExpandedHeight(), L"three drives, two gaps and the margin");
        Assert::AreEqual (306L, fan.GetPanelRect (0).right, L"no arrow when everything is open");
    }


    TEST_METHOD (Hover_ClassWithOneDrive_HasNoFan)
    {
        std::vector<DeskDrivePlacement>  plan  = { OnBar (DeskDriveModel::ProFile, 2, 0, 0) };
        DriveBarFan                      fan;



        fan.SetClasses (DriveBarFan::Group (plan));
        fan.Layout (0, 100, SIZE { 200, 40 }, 20, 4, 6, 10);

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


    TEST_METHOD (PutFirst_MovesTheRecentDriveToTheBottomOfItsClass)
    {
        std::vector<DeskDrivePlacement>      plan;
        std::vector<std::vector<DeskDrive>>  classes;



        plan.push_back (OnBar (DeskDriveModel::DiskII,       6, 0, 0));
        plan.push_back (OnBar (DeskDriveModel::Apple35Drive, 4, 0, 1));
        plan.push_back (OnBar (DeskDriveModel::UniDisk35,    5, 0, 2));
        plan.push_back (OnBar (DeskDriveModel::HardDisk20SC, 7, 6, 0));
        plan.push_back (OnBar (DeskDriveModel::ProFile,      2, 0, 1));

        classes = DriveBarFan::Group (plan);
        DriveBarFan::PutFirst (classes, { DeskDrive { DeskDriveModel::UniDisk35, 5, 0 }, DeskDrive { DeskDriveModel::ProFile, 2, 0 } });

        Assert::IsTrue (classes[0][0].model == DeskDriveModel::UniDisk35, L"the last floppy clicked is shown while the stack is closed");
        Assert::IsTrue (classes[0][1].model == DeskDriveModel::DiskII);
        Assert::IsTrue (classes[0][2].model == DeskDriveModel::Apple35Drive);
        Assert::IsTrue (classes[1][0].model == DeskDriveModel::ProFile);
        Assert::IsTrue (classes[1][1].model == DeskDriveModel::HardDisk20SC);
    }


    TEST_METHOD (PutFirst_DriveNotOnTheBar_ChangesNothing)
    {
        std::vector<DeskDrivePlacement>      plan;
        std::vector<std::vector<DeskDrive>>  classes;



        plan.push_back (OnBar (DeskDriveModel::DiskII,       6, 0, 0));
        plan.push_back (OnBar (DeskDriveModel::Apple35Drive, 4, 0, 1));

        classes = DriveBarFan::Group (plan);
        DriveBarFan::PutFirst (classes, { DeskDrive { DeskDriveModel::UniDisk35, 5, 0 } });

        Assert::IsTrue (classes[0][0].model == DeskDriveModel::DiskII);
        Assert::IsTrue (classes[0][1].model == DeskDriveModel::Apple35Drive);
    }


    TEST_METHOD (GetZoomAfterWheel_StepsByANotchAndStopsAtTheLimits)
    {
        Assert::AreEqual (1.1f, DriveBarFan::GetZoomAfterWheel (1.0f, 1.0f), 0.001f);
        Assert::AreEqual (0.9f, DriveBarFan::GetZoomAfterWheel (1.0f, -1.0f), 0.001f);
        Assert::AreEqual (DriveBarFan::kMaxZoom, DriveBarFan::GetZoomAfterWheel (DriveBarFan::kMaxZoom, 3.0f), 0.001f);
        Assert::AreEqual (DriveBarFan::kMinZoom, DriveBarFan::GetZoomAfterWheel (DriveBarFan::kMinZoom, -3.0f), 0.001f);
        Assert::AreEqual (1.0f, DriveBarFan::GetZoomAfterWheel (0.9f, 1.0f), 0.001f, L"100% is one of the stops");
        Assert::AreEqual (1.0f, DriveBarFan::GetZoomAfterWheel (1.04f, 0.0f), 0.001f, L"a zoom off the steps lands back on one");
    }
};
