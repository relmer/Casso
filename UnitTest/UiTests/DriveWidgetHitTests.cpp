#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





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
