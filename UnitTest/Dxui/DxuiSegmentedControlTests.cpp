#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControlTests
//
//  Selection by click and key, and the pill's slide: from where the pill is
//  to the new segment over kSlideMs, starting at the first Tick after the
//  selection, and a jump when animations are off or the host selects.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiSegmentedControlTests)
{
public:

    static constexpr float  kTolerance = 0.001f;

    //  At 96 DPI, 204 pixels less a 2-pixel inset each side: two 100-pixel
    //  segments starting at x = 2.
    static void MakeControl (DxuiSegmentedControl & control, bool isAnimated)
    {
        DxuiDpiScaler  scaler;



        control.SetSegments          ({ L"Structure", L"Timing" });
        control.SetAnimationsEnabled (isAnimated);
        control.Layout               ({ 0, 0, 204, 30 }, scaler);
    }


    static void Click (DxuiSegmentedControl & control, int x)
    {
        control.OnLButtonDown (x, 15);
        control.OnLButtonUp   (x, 15);
    }


    TEST_METHOD (NewSegmentsSelectTheFirst)
    {
        DxuiSegmentedControl  control;



        Assert::AreEqual (-1, control.GetSelected());

        MakeControl (control, true);

        Assert::AreEqual (0, control.GetSelected());
        Assert::AreEqual (0.0f, control.GetPillPosition(), kTolerance);
        Assert::IsFalse  (control.WantsTick());
    }


    TEST_METHOD (HitTestSplitsTheCapsuleEvenly)
    {
        DxuiSegmentedControl  control;



        MakeControl (control, true);

        Assert::AreEqual (0,  control.HitTest (50,  15));
        Assert::AreEqual (0,  control.HitTest (101, 15));
        Assert::AreEqual (1,  control.HitTest (102, 15));
        Assert::AreEqual (1,  control.HitTest (203, 15));
        Assert::AreEqual (-1, control.HitTest (204, 15), L"past the right edge");
        Assert::AreEqual (-1, control.HitTest (50,  30), L"below the bottom edge");

        control.SetEnabled (false);
        Assert::AreEqual (-1, control.HitTest (50, 15), L"a disabled control is transparent to the mouse");
    }


    TEST_METHOD (AClickSelectsAndSlidesThePill)
    {
        DxuiSegmentedControl  control;
        int                   changed = -1;
        float                 middle  = 0.0f;



        MakeControl (control, true);
        control.SetOnChange ([&changed] (int index) { changed = index; });

        Click (control, 150);

        Assert::AreEqual (1, changed);
        Assert::AreEqual (1, control.GetSelected());
        Assert::IsTrue   (control.WantsTick());
        Assert::AreEqual (0.0f, control.GetPillPosition(), kTolerance, L"the pill has not moved before a tick");

        control.Tick (1000);
        Assert::AreEqual (0.0f, control.GetPillPosition(), kTolerance, L"the first tick starts the slide");

        control.Tick (1000 + DxuiSegmentedControl::kSlideMs / 2);
        middle = control.GetPillPosition();
        Assert::IsTrue (middle > 0.0f && middle < 1.0f, L"halfway, the pill is between the segments");

        control.Tick (1000 + DxuiSegmentedControl::kSlideMs);
        Assert::AreEqual (1.0f, control.GetPillPosition(), kTolerance);
        Assert::IsFalse  (control.WantsTick(), L"WantsTick is false once the slide ends");
    }


    TEST_METHOD (ASecondClickTurnsThePillAroundWhereItIs)
    {
        DxuiSegmentedControl  control;
        float                 turned = 0.0f;



        MakeControl (control, true);

        Click (control, 150);
        control.Tick (1000);
        control.Tick (1000 + DxuiSegmentedControl::kSlideMs / 2);
        turned = control.GetPillPosition();

        Click (control, 50);
        control.Tick (2000);

        Assert::AreEqual (turned, control.GetPillPosition(), kTolerance, L"the return starts where the pill was");

        control.Tick (2000 + DxuiSegmentedControl::kSlideMs);
        Assert::AreEqual (0.0f, control.GetPillPosition(), kTolerance);
    }


    TEST_METHOD (WithAnimationsOffThePillJumps)
    {
        DxuiSegmentedControl  control;



        MakeControl (control, false);

        Click (control, 150);

        Assert::AreEqual (1.0f, control.GetPillPosition(), kTolerance);
        Assert::IsFalse  (control.WantsTick());
    }


    TEST_METHOD (TheHostsSelectionJumpsWithoutAChange)
    {
        DxuiSegmentedControl  control;
        int                   changes = 0;



        MakeControl (control, true);
        control.SetOnChange ([&changes] (int) { changes++; });

        control.SetSelected (1);
        Assert::AreEqual (1.0f, control.GetPillPosition(), kTolerance);
        Assert::IsFalse  (control.WantsTick());

        control.SetSelected (0, true);
        Assert::IsTrue (control.WantsTick(), L"an animated selection slides");

        control.SetSelected (7);
        Assert::AreEqual (1, control.GetSelected(), L"clamped to the last segment");
        Assert::AreEqual (0, changes, L"the host's selections don't raise a change");
    }


    TEST_METHOD (ArrowsMoveTheSelectionAndStopAtTheEnds)
    {
        DxuiSegmentedControl  control;



        MakeControl (control, false);

        Assert::IsFalse (control.OnKey (static_cast<WPARAM> (VK_RIGHT)), L"keys have no effect while the control is unfocused");

        control.SetFocused (true);

        Assert::IsTrue   (control.OnKey (static_cast<WPARAM> (VK_RIGHT)));
        Assert::AreEqual (1, control.GetSelected());
        Assert::IsTrue   (control.OnKey (static_cast<WPARAM> (VK_RIGHT)));
        Assert::AreEqual (1, control.GetSelected(), L"Right stops at the last segment");
        Assert::IsTrue   (control.OnKey (static_cast<WPARAM> (VK_HOME)));
        Assert::AreEqual (0, control.GetSelected());
        Assert::IsTrue   (control.OnKey (static_cast<WPARAM> (VK_END)));
        Assert::AreEqual (1, control.GetSelected());
        Assert::IsFalse  (control.OnKey (static_cast<WPARAM> (VK_SPACE)));
    }
};
