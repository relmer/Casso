#include "Pch.h"

#include "Widgets/DxuiTabStrip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TabStripTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    DxuiTabStrip::Tab MakeTab (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiTabStrip::Tab  tab;
        tab.rect  = { l, t, r, b };
        tab.label = label;
        return tab;
    }


    std::vector<DxuiTabStrip::Tab>  MakeThreeTabs ()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;
        tabs.push_back (MakeTab (  0, 0,  80, 24, L"Machine"));
        tabs.push_back (MakeTab ( 80, 0, 160, 24, L"Hardware"));
        tabs.push_back (MakeTab (160, 0, 240, 24, L"Display"));
        return tabs;
    }
}


TEST_CLASS (TabStripTests)
{
public:

    TEST_METHOD (HitTest_ReturnsIndex)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());

        Assert::AreEqual (0, ts.HitTest ( 10, 10));
        Assert::AreEqual (1, ts.HitTest (100, 10));
        Assert::AreEqual (2, ts.HitTest (200, 10));
        Assert::AreEqual (-1, ts.HitTest (500, 500));
    }

    TEST_METHOD (Click_SelectsAndFiresOnChange)
    {
        DxuiTabStrip  ts;
        int       last = -1;
        ts.SetTabs (MakeThreeTabs());
        ts.SetOnChange ([&] (int idx) { last = idx; });

        Assert::IsTrue (ts.OnLButtonDown (100, 10));
        Assert::IsTrue (ts.OnLButtonUp   (100, 10));
        Assert::AreEqual (1, ts.Selected());
        Assert::AreEqual (1, last);
    }

    TEST_METHOD (Click_OutsideAfterPress_NoChange)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetSelected (0);

        Assert::IsTrue  (ts.OnLButtonDown (100, 10));
        Assert::IsFalse (ts.OnLButtonUp   (500, 500));
        Assert::AreEqual (0, ts.Selected());
    }

    TEST_METHOD (KeyRight_Wraps)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetFocused (true);
        ts.SetSelected (2);

        Assert::IsTrue (ts.OnKey (VK_RIGHT));
        Assert::AreEqual (0, ts.Selected());
    }

    TEST_METHOD (KeyLeft_Wraps)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetFocused (true);
        ts.SetSelected (0);

        Assert::IsTrue (ts.OnKey (VK_LEFT));
        Assert::AreEqual (2, ts.Selected());
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());

        Assert::IsFalse (ts.OnKey (VK_RIGHT));
    }
};
