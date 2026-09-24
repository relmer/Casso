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

    TEST_METHOD (NewTabButton_FollowsTheLastTab)
    {
        DxuiTabStrip  ts;
        int           opened = 0;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetOnNewTab ([&]() { opened++; });
        LayOut (ts, 400);

        Assert::IsFalse  (ts.HasScrollArrows());
        Assert::AreEqual (-1, ts.HitTest (250, 10), L"The + button is no tab");

        Assert::IsTrue   (ts.OnLButtonDown (250, 10));
        Assert::IsTrue   (ts.OnLButtonUp   (250, 10));
        Assert::AreEqual (1, opened, L"The + button just past the last tab opens a new one");

        ts.OnLButtonDown (350, 10);
        ts.OnLButtonUp   (350, 10);
        Assert::AreEqual (1, opened, L"The strip past the + button is empty");
    }


    TEST_METHOD (NewTabButton_HoldsTheRightEndWhenTabsOverflow)
    {
        DxuiTabStrip  ts;
        int           opened = 0;

        ts.SetTabs     (MakeTenTabs());
        ts.SetOnNewTab ([&]() { opened++; });
        LayOut (ts, 240);

        Assert::IsTrue   (ts.HasScrollArrows());

        ts.OnLButtonDown (230, 10);
        ts.OnLButtonUp   (230, 10);
        Assert::AreEqual (1, opened, L"The + button sits at the strip's right end");
        Assert::AreEqual (0, ts.GetScrollPx(), L"and is not the right arrow");

        ts.OnLButtonDown (190, 10);
        ts.OnLButtonUp   (190, 10);
        Assert::AreEqual (8, ts.GetScrollPx(), L"The right arrow sits just before it");

        Assert::IsTrue   (ts.OnWheel (-100.0f));
        Assert::AreEqual (648, ts.GetScrollPx(), L"Scrolled to the end, the last tab stops at the right arrow");
        Assert::AreEqual (9,   ts.HitTest (170, 10));
    }


    //  Explorer's close button sits toward each tab's right end. Clicking it
    //  closes that tab and selects nothing; the rest of the tab still selects.
    TEST_METHOD (CloseButton_ClosesItsTabWithoutSelectingIt)
    {
        DxuiTabStrip  ts;
        int           closed = -1;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetOnClose  ([&] (int index) { closed = index; });
        ts.SetSelected (1);
        LayOut (ts, 300);

        Assert::IsTrue   (ts.OnLButtonDown (138, 12));
        Assert::IsTrue   (ts.OnLButtonUp   (138, 12));
        Assert::AreEqual (1, closed, L"The second tab's close button closes the second tab");
        Assert::AreEqual (1, ts.GetSelected(), L"and selects nothing");

        closed = -1;
        ts.OnLButtonDown (90, 12);
        ts.OnLButtonUp   (90, 12);
        Assert::AreEqual (-1, closed, L"The rest of a tab closes nothing");
        Assert::AreEqual (1,  ts.GetSelected(), L"and selects it");

        ts.OnLButtonDown (20, 12);
        ts.OnLButtonUp   (20, 12);
        Assert::AreEqual (0, ts.GetSelected());
    }

    //  Visual Studio's document tabs: the close button is on the selected tab
    //  and the one under the pointer, and a press where it would be on any
    //  other tab selects that tab. The third tab's button is centered 12 px
    //  inside its right end, at 228.
    TEST_METHOD (DocumentStyle_ClosesOnlyTheSelectedOrHoveredTab)
    {
        DxuiTabStrip  ts;
        int           closed = -1;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetStyle    (DxuiTabStrip::Style::Document);
        ts.SetOnClose  ([&] (int index) { closed = index; });
        ts.SetSelected (0);
        LayOut (ts, 300);

        ts.OnLButtonDown (228, 12);
        ts.OnLButtonUp   (228, 12);
        Assert::AreEqual (-1, closed,         L"no button on a tab neither selected nor hovered");
        Assert::AreEqual (2,  ts.GetSelected(), L"the press selects it");

        ts.SetMouseHover (228, 12);
        ts.OnLButtonDown (228, 12);
        ts.OnLButtonUp   (228, 12);
        Assert::AreEqual (2, closed, L"selected, and under the pointer, it has one");
    }

    //  A tool window closes from its title bar, so its tabs have no button.
    TEST_METHOD (ToolWindowStyle_ShowsNoCloseButton)
    {
        DxuiTabStrip  ts;
        int           closed = -1;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetStyle    (DxuiTabStrip::Style::ToolWindow);
        ts.SetOnClose  ([&] (int index) { closed = index; });
        ts.SetSelected (2);
        LayOut (ts, 300);

        ts.SetMouseHover (228, 12);
        ts.OnLButtonDown (228, 12);
        ts.OnLButtonUp   (228, 12);
        Assert::AreEqual (-1, closed);
    }

    //  A tab dragged past the strip's top or bottom goes to the host, which
    //  the strip lets have it: no move along the strip, no selection.
    TEST_METHOD (DragOffTheStrip_HandsTheTabToTheHost)
    {
        DxuiTabStrip  ts;
        int           handed = -1;
        POINT         at     = {};
        int           moves  = 0;

        ts.SetTabs      (MakeThreeTabs());
        ts.SetOnMove    ([&] (int, int) { moves++; });
        ts.SetOnDragOut ([&] (int index, POINT point) { handed = index; at = point; });
        LayOut (ts, 300);

        ts.OnLButtonDown (100, 12);
        ts.OnMouseMove   (100, 20);
        Assert::AreEqual (-1, handed, L"still on the strip");

        ts.OnMouseMove   (100, 60);
        Assert::AreEqual (1,  handed, L"below it, the host takes the tab");
        Assert::AreEqual (60L, at.y);
        Assert::IsFalse  (ts.IsInteracting(), L"and the strip lets go");
        Assert::AreEqual (0, moves);
    }

    //  A leading mark is drawn ahead of the label in its own face and color,
    //  and a tab's tip is the host's to show.
    TEST_METHOD (CompactTab_DrawsItsMarkAndGivesItsTip)
    {
        DxuiTabStrip                    ts;
        std::vector<DxuiTabStrip::Tab>  tabs   = MakeThreeTabs();
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        RECT                            tab    = {};
        bool                            marked = false;

        tabs[1].mark     = L"M";
        tabs[1].markFace = L"Mark Face";
        tabs[1].markArgb = 0xFFFFD700;
        tabs[1].tip      = L"This one follows the PC";

        ts.SetTabs  (tabs);
        ts.SetStyle (DxuiTabStrip::Style::Document);
        LayOut (ts, 300);
        ts.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            marked = marked || (call.text == L"M" && call.argb == 0xFFFFD700 && call.x < 100.0f);
        }

        Assert::IsTrue   (marked, L"the mark, ahead of the second tab's label");
        Assert::AreEqual (std::wstring (L"This one follows the PC"), ts.GetTipAt (100, 12, tab));
        Assert::AreEqual (80L, tab.left);
        Assert::IsTrue   (ts.GetTipAt (20, 12, tab).empty());
    }

    //  A tab measured for its mark and close button is wider than one
    //  without, so a host sizing its tabs this way never cuts a label short.
    TEST_METHOD (MeasureTab_MakesRoomForTheMarkAndTheCloseButton)
    {
        DxuiDpiScaler      scaler;
        DxuiTabStrip::Tab  plain;
        DxuiTabStrip::Tab  marked;

        scaler.SetDpi (96);
        plain.label  = L"include-macro.a65";
        marked       = plain;
        marked.mark  = L"M";

        Assert::IsTrue (DxuiTabStrip::MeasureTabPx (nullptr, marked, DxuiTabStrip::Style::Document, false, scaler) >
                        DxuiTabStrip::MeasureTabPx (nullptr, plain,  DxuiTabStrip::Style::Document, false, scaler));
        Assert::IsTrue (DxuiTabStrip::MeasureTabPx (nullptr, plain,  DxuiTabStrip::Style::Document, true,  scaler) >
                        DxuiTabStrip::MeasureTabPx (nullptr, plain,  DxuiTabStrip::Style::Document, false, scaler));
        Assert::AreEqual (DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::ToolWindow, true,  scaler),
                          DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::ToolWindow, false, scaler),
                          L"a tool window's tabs have no close button to make room for");
    }
};
