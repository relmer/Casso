#include "Pch.h"

#include "Widgets/DxuiRadio.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RadioGroupTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    DxuiRadioOption MakeOpt (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiRadioOption  o;
        o.rect  = { l, t, r, b };
        o.label = label;
        return o;
    }


    std::vector<DxuiRadioOption> MakeTwoOptions ()
    {
        std::vector<DxuiRadioOption>  opts;
        opts.push_back (MakeOpt (  0, 0,  90, 20, L"A"));
        opts.push_back (MakeOpt (100, 0, 190, 20, L"B"));
        return opts;
    }
}


TEST_CLASS (RadioGroupTests)
{
public:

    TEST_METHOD (HitTest_ReturnsIndex)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::AreEqual (0, g.HitTest ( 10, 10));
        Assert::AreEqual (1, g.HitTest (120, 10));
        Assert::AreEqual (-1, g.HitTest (500, 500));
    }

    TEST_METHOD (Mouse_ClickSelects_AndFiresOnChange)
    {
        DxuiRadioGroup  g;
        int         lastIdx = -42;
        g.SetOptions (MakeTwoOptions());
        g.SetOnChange ([&] (int idx) { lastIdx = idx; });

        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsTrue (g.OnLButtonUp   (10, 10));
        Assert::AreEqual (0, g.Selected());
        Assert::AreEqual (0, lastIdx);

        Assert::IsTrue (g.OnLButtonDown (120, 10));
        Assert::IsTrue (g.OnLButtonUp   (120, 10));
        Assert::AreEqual (1, g.Selected());
        Assert::AreEqual (1, lastIdx);
    }

    TEST_METHOD (Mouse_DragOffCancels)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsFalse (g.OnLButtonUp (500, 500));
        Assert::AreEqual (-1, g.Selected());
    }

    TEST_METHOD (Key_RightFromUnselected_SelectsFirst)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.Selected());
    }

    TEST_METHOD (Key_LeftWraps)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_LEFT));
        Assert::AreEqual (1, g.Selected());
    }

    TEST_METHOD (Key_RightWraps)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (1);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.Selected());
    }

    TEST_METHOD (Key_DownEquivalentToRight)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_DOWN));
        Assert::AreEqual (1, g.Selected());
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsFalse (g.OnKey (VK_RIGHT));
        Assert::AreEqual (-1, g.Selected());
    }

    TEST_METHOD (Disabled_RejectsMouseAndHit)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetEnabled (false);

        Assert::AreEqual (-1, g.HitTest (10, 10));
        Assert::IsFalse (g.OnLButtonDown (10, 10));
    }
};
