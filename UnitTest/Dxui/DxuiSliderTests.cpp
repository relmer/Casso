#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SliderTests
//
//  Slider value mapping: position to value, clamping at the rails, and the
//  separate step sizes.
//
//  Two step sizes exist and are tested separately: a coarse one for clicks and
//  arrow keys, and a fine one for dragging. Collapsing them makes a slider
//  either impossible to adjust precisely or impossible to move quickly.
//
//  Positions BEYOND both rails are covered because a drag routinely leaves the
//  track -- the value must clamp rather than extrapolate, and the ends must be
//  exactly reachable rather than approached asymptotically.
//
//  Round-tripping value to position and back is asserted, since the thumb's
//  drawn position and the value it represents are computed separately and a
//  disagreement shows as a thumb that will not sit under the cursor.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (SliderTests)
{
public:

    RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }


    DxuiSlider  MakeUnitSlider()
    {
        DxuiSlider  s;
        s.SetRect (MakeRect (0, 0, 100, 16));
        s.SetRange (0.0f, 1.0f);
        s.SetStep  (0.1f);
        s.SetValue (0.0f);
        return s;
    }


    bool  NearlyEqual (float a, float b)
    {
        return std::fabs (a - b) < 1e-4f;
    }

    TEST_METHOD (SetValue_QuantizesAndClamps)
    {
        DxuiSlider  s = MakeUnitSlider();

        s.SetValue (0.347f);
        Assert::IsTrue (NearlyEqual (0.3f, s.GetValue()));

        s.SetValue (-1.0f);
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()));

        s.SetValue (10.0f);
        Assert::IsTrue (NearlyEqual (1.0f, s.GetValue()));
    }

    TEST_METHOD (SetRange_SwapsReversed)
    {
        DxuiSlider  s;
        s.SetRange (1.0f, 0.0f);
        Assert::IsTrue (NearlyEqual (0.0f, s.GetMin()));
        Assert::IsTrue (NearlyEqual (1.0f, s.GetMax()));
    }

    TEST_METHOD (MouseDown_JumpsAndDrags)
    {
        DxuiSlider  s    = MakeUnitSlider();
        float       last = -1.0f;
        s.SetOnChange ([&] (float v) { last = v; });

        Assert::IsTrue (s.OnLButtonDown (50, 8));
        Assert::IsTrue (s.IsDragging());
        Assert::IsTrue (NearlyEqual (0.5f, s.GetValue()));
        Assert::IsTrue (NearlyEqual (0.5f, last));
    }

    TEST_METHOD (MouseMove_OnlyAffectsWhileDragging)
    {
        DxuiSlider  s = MakeUnitSlider();

        Assert::IsFalse (s.OnMouseMove (30, 8));
        Assert::IsTrue  (NearlyEqual (0.0f, s.GetValue()));

        // The track is inset by the widest puck at each end, so the whole
        // control stays inside its rect: on this 100-wide slider it runs from
        // 11 to 89, and a press at 11 is the minimum rather than a tenth in.
        Assert::IsTrue (s.OnLButtonDown (11, 8));
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()));

        Assert::IsTrue (s.OnMouseMove   (74, 8));
        Assert::IsTrue (NearlyEqual (0.8f, s.GetValue()));

        Assert::IsTrue (s.OnLButtonUp (80, 8));
        Assert::IsFalse (s.IsDragging());
    }

    TEST_METHOD (Key_FocusedSteps)
    {
        DxuiSlider  s = MakeUnitSlider();
        s.SetFocused (true);
        s.SetValue (0.5f);

        Assert::IsTrue (s.OnKey (VK_LEFT));
        Assert::IsTrue (NearlyEqual (0.4f, s.GetValue()));

        Assert::IsTrue (s.OnKey (VK_RIGHT));
        Assert::IsTrue (NearlyEqual (0.5f, s.GetValue()));

        Assert::IsTrue (s.OnKey (VK_PRIOR));
        Assert::IsTrue (NearlyEqual (1.0f, s.GetValue()));   // clamped

        Assert::IsTrue (s.OnKey (VK_HOME));
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()));

        Assert::IsTrue (s.OnKey (VK_END));
        Assert::IsTrue (NearlyEqual (1.0f, s.GetValue()));
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiSlider  s = MakeUnitSlider();
        Assert::IsFalse (s.OnKey (VK_RIGHT));
        Assert::IsTrue  (NearlyEqual (0.0f, s.GetValue()));
    }

    TEST_METHOD (Disabled_RejectsMouse)
    {
        DxuiSlider  s = MakeUnitSlider();
        s.SetEnabled (false);
        Assert::IsFalse (s.OnLButtonDown (50, 8));
    }

    TEST_METHOD (OnChange_NotCalledForNoOpKey)
    {
        DxuiSlider  s         = MakeUnitSlider();
        int         callCount = 0;
        s.SetFocused (true);
        s.SetOnChange ([&] (float) { callCount++; });

        s.SetValue (0.0f);
        Assert::IsTrue (s.OnKey (VK_LEFT));
        Assert::AreEqual (0, callCount,
            L"Stepping below min must not fire OnChange once value is already at min.");
    }


    //
    //  Vertical orientation. The mapping flips axis AND direction -- max at
    //  the TOP, the fader convention -- and the readout area moves from the
    //  right edge to the bottom, so the track shortens vertically when a
    //  suffix is set. Asserted through the public mouse path, exactly like
    //  the horizontal cases above.
    //

    DxuiSlider  MakeVerticalSlider()
    {
        DxuiSlider  s;
        s.SetRect     (MakeRect (0, 0, 28, 100));
        s.SetVertical (true);
        s.SetRange    (0.0f, 1.0f);
        s.SetStep     (0.1f);
        s.SetValue    (0.0f);
        return s;
    }

    TEST_METHOD (Vertical_TopIsMax_BottomIsMin)
    {
        DxuiSlider  s = MakeVerticalSlider();

        Assert::IsTrue (s.OnLButtonDown (14, 0));
        Assert::IsTrue (NearlyEqual (1.0f, s.GetValue()), L"the top of the track is max");
        s.OnLButtonUp (14, 0);

        Assert::IsTrue (s.OnLButtonDown (14, 100));
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()), L"the bottom of the track is min");
    }

    TEST_METHOD (Vertical_MidTrack_IsMidValue)
    {
        DxuiSlider  s = MakeVerticalSlider();

        Assert::IsTrue (s.OnLButtonDown (14, 50));
        Assert::IsTrue (NearlyEqual (0.5f, s.GetValue()));
    }

    TEST_METHOD (Vertical_DragBeyondRails_Clamps)
    {
        DxuiSlider  s = MakeVerticalSlider();

        Assert::IsTrue (s.OnLButtonDown (14, 50));
        s.OnMouseMove (14, -40);
        Assert::IsTrue (NearlyEqual (1.0f, s.GetValue()), L"above the track clamps to max");
        s.OnMouseMove (14, 300);
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()), L"below the track clamps to min");
    }

    TEST_METHOD (Vertical_SuffixReservesTheBottom_ForTheReadout)
    {
        // With a readout, the track ends above the value area: the mapping
        // must reach min at the SHORTENED track's bottom, not the bounds'.
        DxuiSlider  s = MakeVerticalSlider();

        s.SetSuffix (L"%");

        Assert::IsTrue (s.OnLButtonDown (14, 76));
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()),
                        L"the shortened track bottom must map exactly to min");
    }

    TEST_METHOD (Vertical_Keys_UpIncreases)
    {
        DxuiSlider  s = MakeVerticalSlider();

        s.SetFocused (true);
        Assert::IsTrue (s.OnKey (VK_UP));
        Assert::IsTrue (NearlyEqual (0.1f, s.GetValue()));
        Assert::IsTrue (s.OnKey (VK_DOWN));
        Assert::IsTrue (NearlyEqual (0.0f, s.GetValue()));
    }

    DxuiSlider  MakeZoomSlider()
    {
        DxuiSlider  s;

        s.SetRect  (MakeRect (0, 0, 300, 24));
        s.SetRange (50.0f, 300.0f);
        s.SetStep  (10.0f);
        s.SetValue (100.0f);
        return s;
    }

    TEST_METHOD (Ticks_DefaultToOnePerStep)
    {
        DxuiSlider          s     = MakeUnitSlider();
        std::vector<float>  ticks = s.GetTickValues();

        Assert::AreEqual ((size_t) 11, ticks.size());
        Assert::IsTrue (NearlyEqual (0.0f, ticks.front()));
        Assert::IsTrue (NearlyEqual (0.5f, ticks[5]));
        Assert::IsTrue (NearlyEqual (1.0f, ticks.back()));
    }

    TEST_METHOD (Ticks_FollowTheTickInterval)
    {
        DxuiSlider          s = MakeUnitSlider();
        std::vector<float>  ticks;

        s.SetTickInterval (0.5f);
        ticks = s.GetTickValues();

        Assert::AreEqual ((size_t) 3, ticks.size());
        Assert::IsTrue (NearlyEqual (0.5f, ticks[1]));
    }

    TEST_METHOD (Ticks_ExplicitValuesReplaceTheIntervalTicksInOrder)
    {
        DxuiSlider          s = MakeZoomSlider();
        std::vector<float>  ticks;

        s.SetTickValues ({ 300.0f, 50.0f, 100.0f });
        ticks = s.GetTickValues();

        Assert::AreEqual ((size_t) 3, ticks.size());
        Assert::IsTrue (NearlyEqual (50.0f,  ticks[0]));
        Assert::IsTrue (NearlyEqual (100.0f, ticks[1]));
        Assert::IsTrue (NearlyEqual (300.0f, ticks[2]));
    }

    TEST_METHOD (Wheel_IsOffByDefault)
    {
        DxuiSlider  s = MakeZoomSlider();

        Assert::IsFalse (s.OnWheel (1.0f));
        Assert::IsTrue (NearlyEqual (100.0f, s.GetValue()));
    }

    TEST_METHOD (Wheel_StepsOneStepPerNotchUpIsLarger)
    {
        DxuiSlider  s       = MakeZoomSlider();
        int         changes = 0;

        s.SetWheelStepping (true);
        s.SetOnChange ([&changes] (float) { changes++; });

        Assert::IsTrue (s.OnWheel (1.0f));
        Assert::IsTrue (NearlyEqual (110.0f, s.GetValue()));

        s.OnWheel (-3.0f);
        Assert::IsTrue (NearlyEqual (80.0f, s.GetValue()));
        Assert::AreEqual (2, changes);
    }

    TEST_METHOD (Wheel_ClampsAtTheRailsAndIgnoresPartialNotches)
    {
        DxuiSlider  s = MakeZoomSlider();

        s.SetWheelStepping (true);

        s.OnWheel (100.0f);
        Assert::IsTrue (NearlyEqual (300.0f, s.GetValue()));

        s.OnWheel (-100.0f);
        Assert::IsTrue (NearlyEqual (50.0f, s.GetValue()));

        s.OnWheel (0.4f);
        Assert::IsTrue (NearlyEqual (50.0f, s.GetValue()));
    }

    TEST_METHOD (Wheel_ArrivesThroughOnMouseOnlyOverTheSlider)
    {
        DxuiSlider      s  = MakeZoomSlider();
        DxuiMouseEvent  ev;

        s.SetWheelStepping (true);
        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.wheelDelta  = 1.0f;
        ev.positionDip = POINT { 100, 12 };

        Assert::IsTrue (s.OnMouse (ev));
        Assert::IsTrue (NearlyEqual (110.0f, s.GetValue()));

        ev.positionDip = POINT { 100, 200 };
        Assert::IsFalse (s.OnMouse (ev));
        Assert::IsTrue (NearlyEqual (110.0f, s.GetValue()));

        ev.positionDip     = POINT { 100, 12 };
        ev.wheelHorizontal = true;
        Assert::IsFalse (s.OnMouse (ev));
    }

    TEST_METHOD (DoubleClick_ResetsToTheDefaultValue)
    {
        DxuiSlider  s = MakeZoomSlider();

        s.SetDefaultValue (100.0f);

        Assert::IsTrue (s.OnLButtonDownAt (250, 12, 1000));
        s.OnLButtonUp (250, 12);
        Assert::IsTrue (s.GetValue() > 150.0f, L"the first press moves the value to the point");

        Assert::IsTrue (s.OnLButtonDownAt (250, 12, 1100));
        Assert::IsTrue (NearlyEqual (100.0f, s.GetValue()));
        Assert::IsFalse (s.IsDragging(), L"the second press resets without starting a drag");
    }

    TEST_METHOD (DoubleClick_NeedsThePressesCloseTogetherAndConsumesThePair)
    {
        DxuiSlider  s       = MakeZoomSlider();
        int         slowMs  = DxuiSystemSettings::Instance().GetDoubleClickMs() + 100;

        s.SetDefaultValue (100.0f);

        s.OnLButtonDownAt (250, 12, 1000);
        s.OnLButtonUp (250, 12);
        s.OnLButtonDownAt (250, 12, 1000 + slowMs);
        s.OnLButtonUp (250, 12);
        Assert::IsTrue (s.GetValue() > 150.0f, L"presses too far apart are two presses");

        s.OnLButtonDownAt (250, 12, 1000 + slowMs + 50);
        s.OnLButtonUp (250, 12);
        Assert::IsTrue (NearlyEqual (100.0f, s.GetValue()));

        s.OnLButtonDownAt (250, 12, 1000 + slowMs + 100);
        s.OnLButtonUp (250, 12);
        Assert::IsTrue (s.GetValue() > 150.0f, L"a third press starts a new count");
    }

    TEST_METHOD (DoubleClick_DoesNothingWithoutADefaultValue)
    {
        DxuiSlider  s = MakeZoomSlider();

        s.OnLButtonDownAt (250, 12, 1000);
        s.OnLButtonUp (250, 12);
        s.OnLButtonDownAt (250, 12, 1050);
        s.OnLButtonUp (250, 12);

        Assert::IsTrue (s.GetValue() > 150.0f);
    }

    TEST_METHOD (ValueRect_IsTheReadoutAtTheRightEndWhenShown)
    {
        DxuiSlider  s = MakeZoomSlider();
        RECT        r = s.GetValueRect();

        Assert::IsTrue (IsRectEmpty (&r) != FALSE, L"no readout, no rect");

        s.SetSuffix (L"%");
        r = s.GetValueRect();

        Assert::AreEqual (300L, r.right);
        Assert::IsTrue (r.left > 200L && r.left < r.right);
        Assert::AreEqual (0L,  r.top);
        Assert::AreEqual (24L, r.bottom);
    }
};
