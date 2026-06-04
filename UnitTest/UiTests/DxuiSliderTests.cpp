#include "Pch.h"

#include "Widgets/DxuiSlider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SliderTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }


    DxuiSlider  MakeUnitSlider ()
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
}


TEST_CLASS (SliderTests)
{
public:

    TEST_METHOD (SetValue_QuantizesAndClamps)
    {
        DxuiSlider  s = MakeUnitSlider();

        s.SetValue (0.347f);
        Assert::IsTrue (NearlyEqual (0.3f, s.Value()));

        s.SetValue (-1.0f);
        Assert::IsTrue (NearlyEqual (0.0f, s.Value()));

        s.SetValue (10.0f);
        Assert::IsTrue (NearlyEqual (1.0f, s.Value()));
    }

    TEST_METHOD (SetRange_SwapsReversed)
    {
        DxuiSlider  s;
        s.SetRange (1.0f, 0.0f);
        Assert::IsTrue (NearlyEqual (0.0f, s.Min()));
        Assert::IsTrue (NearlyEqual (1.0f, s.Max()));
    }

    TEST_METHOD (MouseDown_JumpsAndDrags)
    {
        DxuiSlider  s = MakeUnitSlider();
        float   last = -1.0f;
        s.SetOnChange ([&] (float v) { last = v; });

        Assert::IsTrue (s.OnLButtonDown (50, 8));
        Assert::IsTrue (s.Dragging());
        Assert::IsTrue (NearlyEqual (0.5f, s.Value()));
        Assert::IsTrue (NearlyEqual (0.5f, last));
    }

    TEST_METHOD (MouseMove_OnlyAffectsWhileDragging)
    {
        DxuiSlider  s = MakeUnitSlider();

        Assert::IsFalse (s.OnMouseMove (30, 8));
        Assert::IsTrue  (NearlyEqual (0.0f, s.Value()));

        Assert::IsTrue (s.OnLButtonDown (10, 8));
        Assert::IsTrue (s.OnMouseMove   (80, 8));
        Assert::IsTrue (NearlyEqual (0.8f, s.Value()));

        Assert::IsTrue (s.OnLButtonUp (80, 8));
        Assert::IsFalse (s.Dragging());
    }

    TEST_METHOD (Key_FocusedSteps)
    {
        DxuiSlider  s = MakeUnitSlider();
        s.SetFocused (true);
        s.SetValue (0.5f);

        Assert::IsTrue (s.OnKey (VK_LEFT));
        Assert::IsTrue (NearlyEqual (0.4f, s.Value()));

        Assert::IsTrue (s.OnKey (VK_RIGHT));
        Assert::IsTrue (NearlyEqual (0.5f, s.Value()));

        Assert::IsTrue (s.OnKey (VK_PRIOR));
        Assert::IsTrue (NearlyEqual (1.0f, s.Value()));   // clamped

        Assert::IsTrue (s.OnKey (VK_HOME));
        Assert::IsTrue (NearlyEqual (0.0f, s.Value()));

        Assert::IsTrue (s.OnKey (VK_END));
        Assert::IsTrue (NearlyEqual (1.0f, s.Value()));
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiSlider  s = MakeUnitSlider();
        Assert::IsFalse (s.OnKey (VK_RIGHT));
        Assert::IsTrue  (NearlyEqual (0.0f, s.Value()));
    }

    TEST_METHOD (Disabled_RejectsMouse)
    {
        DxuiSlider  s = MakeUnitSlider();
        s.SetEnabled (false);
        Assert::IsFalse (s.OnLButtonDown (50, 8));
    }

    TEST_METHOD (OnChange_NotCalledForNoOpKey)
    {
        DxuiSlider  s = MakeUnitSlider();
        int     callCount = 0;
        s.SetFocused (true);
        s.SetOnChange ([&] (float) { callCount++; });

        s.SetValue (0.0f);
        Assert::IsTrue (s.OnKey (VK_LEFT));
        Assert::AreEqual (0, callCount,
            L"Stepping below min must not fire OnChange once value is already at min.");
    }
};
