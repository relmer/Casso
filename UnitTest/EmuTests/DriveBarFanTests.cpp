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

    //  A DRIVE'S FIRST LINE IS ITS NAME: a Disk II numbered as DOS numbers it,
    //  an HD20SC with its SCSI ID, and a 3.5-inch drive with its ProDOS slot
    //  and drive only when the class has two of its kind.
    TEST_METHOD (Title_NamesEachDrive)
    {
        Assert::AreEqual (std::wstring (L"Drive 1 - Disk ]["),    DriveBarFan::GetTitle ({ DeskDriveModel::DiskII,       6, 0 }, false));
        Assert::AreEqual (std::wstring (L"Drive 2 - Disk IIc"),   DriveBarFan::GetTitle ({ DeskDriveModel::Disk2c,       6, 1 }, false));
        Assert::AreEqual (std::wstring (L"UniDisk 3.5"),          DriveBarFan::GetTitle ({ DeskDriveModel::UniDisk35,    5, 0 }, false));
        Assert::AreEqual (std::wstring (L"UniDisk 3.5 - S5,D2"),  DriveBarFan::GetTitle ({ DeskDriveModel::UniDisk35,    5, 1 }, true));
        Assert::AreEqual (std::wstring (L"Built-in 3.5 - S5,D1"), DriveBarFan::GetTitle ({ DeskDriveModel::Apple35Plus,  0, 0 }, true), L"the //c port is slot 5 to ProDOS");
        Assert::AreEqual (std::wstring (L"UniDisk 3.5 - unit 3"), DriveBarFan::GetTitle ({ DeskDriveModel::UniDisk35,    5, 2 }, true));
        Assert::AreEqual (std::wstring (L"HD20SC - ID 6"),        DriveBarFan::GetTitle ({ DeskDriveModel::HardDisk20SC, 7, 6 }, true), L"the SCSI ID is the number");
    }


    //  A DRIVE'S SECOND LINE IS ITS FORM FACTOR AND CAPACITY, with a smaller
    //  disk's size after a 3.5-inch drive's, and a hard disk's image size.
    TEST_METHOD (Detail_FormFactorAndCapacity)
    {
        Assert::AreEqual (std::wstring (L"5.25\" (140K)"),          DriveBarFan::GetDetail (DeskDriveModel::DiskII,       0,     0));
        Assert::AreEqual (std::wstring (L"3.5\" (800K)"),           DriveBarFan::GetDetail (DeskDriveModel::UniDisk35,    0,     0), L"empty");
        Assert::AreEqual (std::wstring (L"3.5\" (800K)"),           DriveBarFan::GetDetail (DeskDriveModel::Apple35Drive, 1600,  0), L"an 800K disk");
        Assert::AreEqual (std::wstring (L"3.5\" (800K), 400K disk"), DriveBarFan::GetDetail (DeskDriveModel::UniDisk35,    800,   0));
        Assert::AreEqual (std::wstring (L"Hard disk (32 MB)"),       DriveBarFan::GetDetail (DeskDriveModel::HardDisk20SC, 65535, 0));
        Assert::AreEqual (std::wstring (L"Hard disk"),               DriveBarFan::GetDetail (DeskDriveModel::HardDisk20SC, 0,     0), L"no image");
        Assert::AreEqual (std::wstring (L"Hard disk (5 MB)"),        DriveBarFan::GetDetail (DeskDriveModel::ProFile,      0,     9728));
        Assert::AreEqual (std::wstring (L"Hard disk (10 MB)"),       DriveBarFan::GetDetail (DeskDriveModel::ProFile,      19456, 19456));
        Assert::AreEqual (std::wstring (L"140K"),                    DriveBarFan::FormatSize (280));
        Assert::AreEqual (std::wstring (L"1 MB"),                    DriveBarFan::FormatSize (2048));
    }
};
