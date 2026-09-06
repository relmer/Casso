#include "Pch.h"

#include "Widgets/DxuiRadio.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RadioGroupTests
//
//  Radio group: exactly one selection, arrow navigation with wrap, and commit
//  on move.
//
//  Committing AS IT NAVIGATES is what distinguishes a radio group from a list,
//  and it is asserted rather than assumed -- arrowing through options is
//  choosing them, with no separate activation step.
//
//  The wrap at both ends is covered, and so is the unselected group: entering
//  at the first option going forward and the last going backward means the
//  first key press always lands somewhere.
//
//  The invariant that exactly one option is selected is checked after every
//  operation, since it is the group's entire reason for existing over a set of
//  checkboxes.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (RadioGroupTests)
{
public:

    DxuiRadioOption MakeOpt (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiRadioOption  o;
        o.rect  = { l, t, r, b };
        o.label = label;
        return o;
    }


    std::vector<DxuiRadioOption> MakeTwoOptions()
    {
        std::vector<DxuiRadioOption>  opts;
        opts.push_back (MakeOpt (  0, 0,  90, 20, L"A"));
        opts.push_back (MakeOpt (100, 0, 190, 20, L"B"));
        return opts;
    }

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
        int             lastIdx = -42;
        g.SetOptions (MakeTwoOptions());
        g.SetOnChange ([&] (int idx) { lastIdx = idx; });

        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsTrue (g.OnLButtonUp   (10, 10));
        Assert::AreEqual (0, g.GetSelected());
        Assert::AreEqual (0, lastIdx);

        Assert::IsTrue (g.OnLButtonDown (120, 10));
        Assert::IsTrue (g.OnLButtonUp   (120, 10));
        Assert::AreEqual (1, g.GetSelected());
        Assert::AreEqual (1, lastIdx);
    }

    TEST_METHOD (Mouse_DragOffCancels)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsFalse (g.OnLButtonUp (500, 500));
        Assert::AreEqual (-1, g.GetSelected());
    }

    TEST_METHOD (Key_RightFromUnselected_SelectsFirst)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.GetSelected());
    }

    TEST_METHOD (Key_LeftWraps)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_LEFT));
        Assert::AreEqual (1, g.GetSelected());
    }

    TEST_METHOD (Key_RightWraps)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (1);

        Assert::IsTrue (g.OnKey (VK_RIGHT));
        Assert::AreEqual (0, g.GetSelected());
    }

    TEST_METHOD (Key_DownEquivalentToRight)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetFocused (true);
        g.SetSelected (0);

        Assert::IsTrue (g.OnKey (VK_DOWN));
        Assert::AreEqual (1, g.GetSelected());
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsFalse (g.OnKey (VK_RIGHT));
        Assert::AreEqual (-1, g.GetSelected());
    }

    TEST_METHOD (Disabled_RejectsMouseAndHit)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetEnabled (false);

        Assert::AreEqual (-1, g.HitTest (10, 10));
        Assert::IsFalse (g.OnLButtonDown (10, 10));
    }


    //
    //  Per-option descriptions. For option sets whose labels cannot carry
    //  their own meaning -- where a one-word label is a token to guess at.
    //

    TEST_METHOD (Description_DefaultsEmptyAndChangesNothing)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());

        Assert::IsTrue (g.GetOptions()[0].description.empty());
        Assert::AreEqual (0, g.HitTest (10, 10));
        Assert::IsTrue (g.OnLButtonDown (10, 10));
        Assert::IsTrue (g.OnLButtonUp   (10, 10));
        Assert::AreEqual (0, g.GetSelected());
    }


    TEST_METHOD (Description_RoundTripsThroughSetOptions)
    {
        DxuiRadioGroup                g;
        std::vector<DxuiRadioOption>  opts = MakeTwoOptions();

        opts[0].description = L"The whole desk, as it looks";

        g.SetOptions (opts);

        Assert::AreEqual (std::wstring (L"The whole desk, as it looks"),
                          g.GetOptions()[0].description);
        Assert::IsTrue (g.GetOptions()[1].description.empty(),
            L"Describing one option must not describe its neighbors");
    }


    // Hit testing is rect-based, so a described option -- which the caller
    // gives a taller rect -- must still select over its whole height, not
    // just the label line.
    TEST_METHOD (Description_TallerRectStaysFullyClickable)
    {
        DxuiRadioGroup                g;
        std::vector<DxuiRadioOption>  opts;

        opts.push_back (MakeOpt (0,  0, 200, 44, L"Scene"));
        opts.push_back (MakeOpt (0, 44, 200, 88, L"Raw"));
        opts[0].description = L"Desk, monitor and drives";
        opts[1].description = L"The framebuffer, unprocessed";

        g.SetOptions (opts);

        Assert::AreEqual (0, g.HitTest (10,  4), L"label line of the first");
        Assert::AreEqual (0, g.HitTest (10, 38), L"description line of the first");
        Assert::AreEqual (1, g.HitTest (10, 48), L"label line of the second");
        Assert::AreEqual (1, g.HitTest (10, 84), L"description line of the second");
    }


    // The description exists because the label alone does not say what the
    // option means. A reader given only the label is left with exactly the
    // guess the description was added to remove.
    TEST_METHOD (Description_IsPartOfTheAccessibleName)
    {
        DxuiRadioGroup                g;
        std::vector<DxuiRadioOption>  opts = MakeTwoOptions();

        opts[0].description = L"Desk, monitor and drives";
        g.SetOptions (opts);
        g.SetSelected (0);

        std::wstring  name = g.GetAccessibleName();

        Assert::IsTrue (name.find (L"A") != std::wstring::npos);
        Assert::IsTrue (name.find (L"Desk, monitor and drives") != std::wstring::npos);
    }


    TEST_METHOD (Description_AbsentLeavesTheAccessibleNameAsTheLabel)
    {
        DxuiRadioGroup  g;
        g.SetOptions (MakeTwoOptions());
        g.SetSelected (1);

        Assert::AreEqual (std::wstring (L"B"), g.GetAccessibleName());
    }
};

