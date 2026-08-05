#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetHitTests
//
//  Which REGION of a drive widget a point falls in: the body, the eject door,
//  or neither.
//
//  The two regions do different things -- the body browses for a disk, the door
//  ejects -- so a boundary that is one pixel out swaps a mount for an eject.
//  That is why the region matters more than the hit.
//
//  Points on each boundary are tested explicitly, since the door sits inside
//  the widget's outer rect and the two rects share an edge.
//
//  A HIDDEN widget must report a miss everywhere. Its rects are collapsed to
//  nothing on a machine with no controller, and a hit test that ignored that
//  would make an invisible drive clickable.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetHitTests)
{
public:

    TEST_METHOD (HitTest_Returns_Expected_Regions)
    {
        DriveWidget    drive;
        RECT           body   = {};
        RECT           eject  = {};
        DxuiDpiScaler  scaler;
        RECT           anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        body  = drive.BodyRect();
        eject = drive.EjectRect();

        Assert::IsTrue (drive.HitTest ((body.left + body.right) / 2,
                                       body.top + 5) == DriveWidgetRegion::Body);
        Assert::IsTrue (drive.HitTest (eject.right + 1,
                                       (body.top + body.bottom) / 2) == DriveWidgetRegion::Body);
        Assert::IsTrue (drive.HitTest ((eject.left + eject.right) / 2,
                                       (eject.top + eject.bottom) / 2) == DriveWidgetRegion::Eject);
        Assert::IsTrue (drive.HitTest (body.left - 1, body.top - 1) == DriveWidgetRegion::None);
    }


    TEST_METHOD (Led_Uses_Active_State_When_Motor_Is_On)
    {
        DriveWidget       drive;
        DriveWidgetState  state;
        DxuiDpiScaler     scaler;
        RECT              anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        state.mountedImagePath = L"boot.dsk";
        state.motorOn.store (true, std::memory_order_relaxed);
        state.diskActive.store (false, std::memory_order_relaxed);

        drive.SyncFromState (state);

        Assert::IsTrue (drive.Led() == LedState::Active);
    }


    TEST_METHOD (Led_Uses_Active_State_When_Nibble_Counters_Move)
    {
        DriveWidget       drive;
        DriveWidgetState  state;
        DxuiDpiScaler     scaler;
        RECT              anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        state.mountedImagePath = L"boot.dsk";
        state.motorOn.store (false, std::memory_order_relaxed);
        state.diskActive.store (true, std::memory_order_relaxed);

        drive.SyncFromState (state);

        Assert::IsTrue (drive.Led() == LedState::Active);
    }
};
