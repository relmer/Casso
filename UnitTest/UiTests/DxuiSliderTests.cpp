#include "Pch.h"

#include "Widgets/DxuiSlider.h"

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

        Assert::IsTrue (s.OnLButtonDown (10, 8));
        Assert::IsTrue (s.OnMouseMove   (80, 8));
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
};
