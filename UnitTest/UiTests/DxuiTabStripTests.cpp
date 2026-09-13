#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Widgets/DxuiTabStrip.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TabStripTests
//
//  Tab strip: selection, hit testing across tab widths, and Left/Right
//  navigation.
//
//  Tabs are sized to their LABELS, so hit testing is not a division -- the
//  tests use tabs of deliberately different widths, where an implementation
//  assuming uniform tabs selects the wrong one everywhere except the first.
//
//  Only the horizontal arrows are bound, and that is pinned: a tab strip is
//  always laid out horizontally, so Up and Down belong to whatever the tab is
//  displaying.
//
//  Selection wraps, and moving it commits -- switching tabs is the point, so
//  there is no separate activation.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (TabStripTests)
{
public:

    DxuiTabStrip::Tab MakeTab (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiTabStrip::Tab  tab;
        tab.rect  = { l, t, r, b };
        tab.label = label;
        return tab;
    }


    std::vector<DxuiTabStrip::Tab>  MakeThreeTabs()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;
        tabs.push_back (MakeTab (  0, 0,  80, 24, L"Machine"));
        tabs.push_back (MakeTab ( 80, 0, 160, 24, L"Hardware"));
        tabs.push_back (MakeTab (160, 0, 240, 24, L"Display"));
        return tabs;
    }

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
        int           last = -1;
        ts.SetTabs (MakeThreeTabs());
        ts.SetOnChange ([&] (int idx) { last = idx; });

        Assert::IsTrue (ts.OnLButtonDown (100, 10));
        Assert::IsTrue (ts.OnLButtonUp   (100, 10));
        Assert::AreEqual (1, ts.GetSelected());
        Assert::AreEqual (1, last);
    }

    TEST_METHOD (Click_OutsideAfterPress_NoChange)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetSelected (0);

        Assert::IsTrue  (ts.OnLButtonDown (100, 10));
        Assert::IsFalse (ts.OnLButtonUp   (500, 500));
        Assert::AreEqual (0, ts.GetSelected());
    }

    TEST_METHOD (KeyRight_Wraps)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetFocused (true);
        ts.SetSelected (2);

        Assert::IsTrue (ts.OnKey (VK_RIGHT));
        Assert::AreEqual (0, ts.GetSelected());
    }

    TEST_METHOD (KeyLeft_Wraps)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());
        ts.SetFocused (true);
        ts.SetSelected (0);

        Assert::IsTrue (ts.OnKey (VK_LEFT));
        Assert::AreEqual (2, ts.GetSelected());
    }

    TEST_METHOD (Key_UnfocusedNoOp)
    {
        DxuiTabStrip  ts;
        ts.SetTabs (MakeThreeTabs());

        Assert::IsFalse (ts.OnKey (VK_RIGHT));
    }

    TEST_METHOD (Paint_LongLabelIsCutOffNotWrapped)
    {
        DxuiTabStrip                    ts;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        MockDxuiPainter                 painter;
        std::vector<DxuiTabStrip::Tab>  tabs;
        std::wstring                    drawn;

        tabs.push_back (MakeTab (0, 0, 60, 24, L"A very long tab label"));
        ts.SetTabs (std::move (tabs));

        ts.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString)
            {
                drawn = call.text;
            }
        }

        Assert::IsFalse (drawn.empty(), L"The tab draws its label");
        Assert::IsTrue  (drawn.size() < wcslen (L"A very long tab label"), L"A label wider than its tab is shortened");
        Assert::AreEqual (s_kchEllipsis, drawn.back(), L"and ends in an ellipsis");
    }

    static std::vector<DxuiTabStrip::Tab>  MakeTenTabs()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;

        for (int i = 0; i < 10; i++)
        {
            DxuiTabStrip::Tab  tab;

            tab.rect  = { i * 80, 0, (i + 1) * 80, 24 };
            tab.label = std::to_wstring (i);
            tabs.push_back (tab);
        }

        return tabs;
    }


    static void  LayOut (DxuiTabStrip & ts, LONG width)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        ts.Layout (RECT { 0, 0, width, 24 }, scaler);
    }


    TEST_METHOD (Drag_MovesTheTabAndReportsEachMove)
    {
        DxuiTabStrip  ts;
        int           from = -1;
        int           to   = -1;

        ts.SetTabs   (MakeThreeTabs());
        ts.SetOnMove ([&] (int f, int t) { from = f; to = t; });

        Assert::IsTrue   (ts.OnLButtonDown (10, 10));
        Assert::IsTrue   (ts.OnMouseMove   (100, 10), L"A press carried past the threshold is a drag");
        Assert::AreEqual (0, from);
        Assert::AreEqual (1, to);
        Assert::IsTrue   (ts.OnMouseMove   (200, 10));
        Assert::IsTrue   (ts.IsInteracting());
        Assert::IsTrue   (ts.OnLButtonUp   (200, 10));
        Assert::IsFalse  (ts.IsInteracting());

        Assert::AreEqual (std::wstring (L"Hardware"), ts.GetTabs()[0].label);
        Assert::AreEqual (std::wstring (L"Display"),  ts.GetTabs()[1].label);
        Assert::AreEqual (std::wstring (L"Machine"),  ts.GetTabs()[2].label, L"The dragged tab lands at the end");
        Assert::AreEqual (1, from);
        Assert::AreEqual (2, to);
        Assert::AreEqual (2, ts.GetSelected(), L"and stays selected");
        Assert::AreEqual (160L, ts.GetTabs()[2].rect.left, L"The row is packed again");
    }


    TEST_METHOD (Drag_SelectsTheTabItCarried)
    {
        DxuiTabStrip  ts;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetSelected (1);

        ts.OnLButtonDown (10, 10);
        ts.OnMouseMove   (200, 10);
        ts.OnLButtonUp   (200, 10);

        Assert::AreEqual (std::wstring (L"Hardware"), ts.GetTabs()[0].label);
        Assert::AreEqual (2, ts.GetSelected(), L"Releasing a drag selects the tab it carried");
    }


    TEST_METHOD (SmallMove_IsStillAClick)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeThreeTabs());

        ts.OnLButtonDown (100, 10);
        Assert::IsFalse (ts.OnMouseMove (102, 10), L"A move inside the threshold is not a drag");
        Assert::IsTrue  (ts.OnLButtonUp (102, 10));

        Assert::AreEqual (1, ts.GetSelected());
        Assert::AreEqual (std::wstring (L"Machine"), ts.GetTabs()[0].label, L"and nothing moved");
    }


    TEST_METHOD (Overflow_WheelScrollsAndHitTestFollows)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeTenTabs());
        LayOut (ts, 240);

        Assert::IsTrue   (ts.HasScrollArrows());
        Assert::AreEqual (-1, ts.HitTest (10, 10),  L"The left arrow is no tab");
        Assert::AreEqual (-1, ts.HitTest (230, 10), L"nor is the right one");
        Assert::AreEqual (0,  ts.HitTest (40, 10),  L"The first tab starts after the left arrow");

        Assert::IsTrue   (ts.OnWheel (-1.0f));
        Assert::AreEqual (60, ts.GetScrollPx());
        Assert::AreEqual (1,  ts.HitTest (58, 10), L"Scrolled, the second tab is under the start of the tabs");

        Assert::IsTrue   (ts.OnWheel (-100.0f));
        Assert::AreEqual (616, ts.GetScrollPx(), L"Scrolling stops with the last tab against the right arrow");
        Assert::AreEqual (9,   ts.HitTest (200, 10));
        Assert::IsFalse  (ts.OnWheel (-1.0f));
    }


    TEST_METHOD (Overflow_SelectingATabScrollsItIntoView)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeTenTabs());
        LayOut (ts, 240);

        ts.SetSelected (9);
        Assert::AreEqual (616, ts.GetScrollPx());

        ts.SetSelected (0);
        Assert::AreEqual (0, ts.GetScrollPx());
    }


    TEST_METHOD (Overflow_DragHeldPastTheEndReachesTheLastPlace)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeTenTabs());
        LayOut (ts, 240);

        ts.OnLButtonDown (40, 10);

        for (int i = 0; i < 100; i++)
        {
            ts.OnMouseMove (400, 10);
        }

        ts.OnLButtonUp (400, 10);

        Assert::AreEqual (std::wstring (L"0"), ts.GetTabs()[9].label, L"A drag held past the right end carries its tab to the last place");
        Assert::AreEqual (9,   ts.GetSelected());
        Assert::AreEqual (616, ts.GetScrollPx());
    }


    TEST_METHOD (TabsThatFit_ShowNoArrows)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeThreeTabs());
        LayOut (ts, 300);

        Assert::IsFalse  (ts.HasScrollArrows());
        Assert::AreEqual (0, ts.HitTest (10, 10), L"With no arrows the first tab starts at the strip's edge");
    }


    TEST_METHOD (Arrows_ScrollOneTabAtATime)
    {
        DxuiTabStrip  ts;

        ts.SetTabs (MakeTenTabs());
        LayOut (ts, 240);

        Assert::IsTrue   (ts.OnLButtonDown (10, 10));
        Assert::IsTrue   (ts.OnLButtonUp   (10, 10));
        Assert::AreEqual (0, ts.GetScrollPx(), L"At the start the left arrow has nowhere to go");

        ts.OnLButtonDown (230, 10);
        ts.OnLButtonUp   (230, 10);
        Assert::AreEqual (56, ts.GetScrollPx(), L"The right arrow brings the third tab, the first cut off, fully into view");

        ts.OnLButtonDown (230, 10);
        ts.OnLButtonUp   (230, 10);
        Assert::AreEqual (136, ts.GetScrollPx(), L"then the fourth");

        ts.OnLButtonDown (10, 10);
        ts.OnLButtonUp   (10, 10);
        Assert::AreEqual (80, ts.GetScrollPx(), L"The left arrow brings the second tab back to the start");

        Assert::AreEqual (0, ts.GetSelected(), L"Scrolling selects nothing");
    }
};
