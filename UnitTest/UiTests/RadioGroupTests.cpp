#include "Pch.h"

#include "Ui/Widgets/Radio.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RadioGroupTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    RadioOption MakeOpt (int l, int t, int r, int b, const wchar_t * label)
    {
        RadioOption  o;
        o.rect  = { l, t, r, b };
        o.label = label;
        return o;
    }


    std::vector<RadioOption> MakeTwoOptions ()
    {
        std::vector<RadioOption>  opts;
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
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::AreEqual (0, g.HitTest ( 10, 10));
        Assert::AreEqual (1, g.HitTest (120, 10));
        Assert::AreEqual (-1, g.HitTest (500, 500));
    }

    TEST_METHOD (Mouse_ClickSelects_AndFiresOnChange)
    {
        RadioGroup  g;
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
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsFalse (g.OnLButtonUp (500, 500));
        Assert::AreEqual (-1, g.Selected());
    }

    TEST_METHOD (Key_RightFromUnselected_SelectsFirst)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.Selected());
    }

    TEST_METHOD (Key_LeftWraps)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_LEFT));
        Assert::AreEqual (1, g.Selected());
    }

    TEST_METHOD (Key_RightWraps)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (1);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.Selected());
    }

    TEST_METHOD (Key_DownEquivalentToRight)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_DOWN));
        Assert::AreEqual (1, g.Selected());
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsFalse (g.OnKey (VK_RIGHT));
        Assert::AreEqual (-1, g.Selected());
    }

    TEST_METHOD (Disabled_RejectsMouseAndHit)
    {
        RadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetEnabled (false);

        Assert::AreEqual (-1, g.HitTest (10, 10));
        Assert::IsFalse (g.OnLButtonDown (10, 10));
    }
};
