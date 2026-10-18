#include "Pch.h"

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

    TEST_METHOD (Move_IsHandledOnlyWhenTheHoveredTabChanges)
    {
        // A handled move is what makes the window repaint at once; an unhandled
        // one waits for the dialog's half-second tick, which is how the hover
        // highlight came to trail the pointer.
        DxuiTabStrip    ts;
        DxuiMouseEvent  ev;

        ts.SetTabs (MakeThreeTabs());
        ev.kind = DxuiMouseEventKind::Move;

        ev.positionDip = { 10, 10 };
        Assert::IsTrue  (ts.OnMouse (ev), L"entering a tab repaints");

        ev.positionDip = { 20, 12 };
        Assert::IsFalse (ts.OnMouse (ev), L"moving within it does not");

        ev.positionDip = { 100, 10 };
        Assert::IsTrue  (ts.OnMouse (ev), L"crossing to the next tab repaints");

        ev.positionDip = { 500, 500 };
        Assert::IsTrue  (ts.OnMouse (ev), L"and so does leaving the strip");
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
    //  other tab selects that tab. The third tab's button is the 24-px square
    //  ending a pixel inside its right end, [215, 239).
    TEST_METHOD (DocumentTabs_CloseOnlyTheSelectedOrHoveredTab)
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

    //  The pin, just left of the close button, is on the same tabs, and a
    //  press on it pins that tab's pane without selecting the tab.
    TEST_METHOD (DocumentTabs_PinTheSelectedOrHoveredTab)
    {
        DxuiTabStrip  ts;
        int           pinned = -1;
        int           closed = -1;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetStyle    (DxuiTabStrip::Style::Document);
        ts.SetOnPin    ([&] (int index) { pinned = index; });
        ts.SetOnClose  ([&] (int index) { closed = index; });
        ts.SetSelected (0);
        LayOut (ts, 300);

        ts.OnLButtonDown (43, 12);
        ts.OnLButtonUp   (43, 12);
        Assert::AreEqual (0,  pinned, L"the selected tab's pin, [31, 55)");
        Assert::AreEqual (-1, closed, L"and not its close button");

        pinned = -1;
        ts.SetMouseHover (120, 12);
        ts.OnLButtonDown (120, 12);
        ts.OnLButtonUp   (120, 12);
        Assert::AreEqual (1, pinned,          L"the hovered tab's pin");
        Assert::AreEqual (0, ts.GetSelected(), L"which selects nothing");
    }

    //  Visual Studio's tool-window tabs show neither a pin nor a close
    //  button, selected or under the pointer, even with both handlers set:
    //  nothing is drawn, nothing is reported under a point, and a press where
    //  a document tab's buttons would be selects the tab.
    TEST_METHOD (ToolWindowTabs_ShowNoPinOrClose)
    {
        DxuiDpiScaler         scaler;
        DxuiTabStrip          ts;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   pinned = -1;
        int                   closed = -1;
        int                   index  = -1;
        RECT                  rect   = {};
        size_t                glyphs = 0;
        size_t                labels = 0;



        scaler.SetDpi    (96);
        ts.SetTabs       (MakeThreeTabs());
        ts.SetStyle      (DxuiTabStrip::Style::ToolWindow);
        ts.SetOnPin      ([&] (int i) { pinned = i; });
        ts.SetOnClose    ([&] (int i) { closed = i; });
        ts.Layout        (RECT { 0, 0, 300, 24 }, scaler);
        ts.SetSelected   (0);
        ts.SetMouseHover (148, 12);
        ts.Paint         (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            glyphs += (call.text == s_kpszMdl2Pinned || call.text == s_kpszMdl2Cancel) ? 1 : 0;
            labels += (call.text == L"Machine" || call.text == L"Hardware" || call.text == L"Display") ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 3, labels, L"every label is drawn");
        Assert::AreEqual ((size_t) 0, glyphs, L"and no pin or close glyph");
        Assert::IsTrue   (ts.GetTabButtonAt (148, 12, index, rect) == DxuiTabStrip::TabButton::None, L"none under the hovered tab's end");
        Assert::IsTrue   (ts.GetTabButtonAt (68,  12, index, rect) == DxuiTabStrip::TabButton::None, L"nor the selected tab's");

        rect = ts.GetTabButtonRect (0, DxuiTabStrip::TabButton::Close);
        Assert::IsTrue   (rect.right <= rect.left, L"a tool window's tab has no button square");

        ts.OnLButtonDown (148, 12);
        ts.OnLButtonUp   (148, 12);
        Assert::AreEqual (-1, pinned,          L"nothing pinned");
        Assert::AreEqual (-1, closed,          L"nothing closed");
        Assert::AreEqual (1,  ts.GetSelected(), L"the press selects the tab");
    }

    //  The tab whose shown pin or close button is under a point, and which
    //  of the two, for the host's tip.
    TEST_METHOD (CompactStyles_ReportTheButtonUnderAPoint)
    {
        DxuiTabStrip  ts;
        int           index = -1;
        RECT          rect  = {};

        ts.SetTabs     (MakeThreeTabs());
        ts.SetStyle    (DxuiTabStrip::Style::Document);
        ts.SetOnPin    ([] (int) {});
        ts.SetOnClose  ([] (int) {});
        ts.SetSelected (1);
        LayOut (ts, 300);

        Assert::IsTrue   (ts.GetTabButtonAt (120, 12, index, rect) == DxuiTabStrip::TabButton::Pin);
        Assert::AreEqual (1, index);
        Assert::AreEqual (111L, rect.left);
        Assert::IsTrue   (ts.GetTabButtonAt (150, 12, index, rect) == DxuiTabStrip::TabButton::Close);
        Assert::AreEqual (159L, rect.right);
        Assert::IsTrue   (ts.GetTabButtonAt (65, 12, index, rect) == DxuiTabStrip::TabButton::None, L"an unselected, unhovered tab shows none");
        Assert::IsTrue   (ts.GetTabButtonAt (90, 12, index, rect) == DxuiTabStrip::TabButton::None, L"the label is no button");
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

    //  A tab measured for its mark is wider than one without, so a host
    //  sizing its tabs this way never cuts a label short. A document tab
    //  keeps room for its pin and close button whether or not it has them,
    //  so no tab changes width as they come and go; a tool window's tab has
    //  neither, and is narrower by their two squares.
    TEST_METHOD (MeasureTab_MakesRoomForTheMarkAndTheButtons)
    {
        DxuiDpiScaler      scaler;
        DxuiTabStrip::Tab  plain;
        DxuiTabStrip::Tab  marked;

        scaler.SetDpi (96);
        plain.label  = L"include-macro.a65";
        marked       = plain;
        marked.mark  = L"M";

        Assert::IsTrue   (DxuiTabStrip::MeasureTabPx (nullptr, marked, DxuiTabStrip::Style::Document, false, scaler) >
                          DxuiTabStrip::MeasureTabPx (nullptr, plain,  DxuiTabStrip::Style::Document, false, scaler));
        Assert::AreEqual (DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::Document,   true,  scaler),
                          DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::Document,   false, scaler),
                          L"the same width with or without a close handler");
        Assert::AreEqual (DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::Document,   true,  scaler) - 48,
                          DxuiTabStrip::MeasureTabPx (nullptr, plain, DxuiTabStrip::Style::ToolWindow, true,  scaler),
                          L"a tool window's tab, two 24-px squares narrower");
    }

    //  Visual Studio's width: the text inset, the label rounded, 6.5 DIP,
    //  two 24-DIP squares on a document tab, and a line. For an 81-px label,
    //  146, 161 and 179 px at 100%, 125% and 150%: VS's 78 px of the rest at
    //  125% and 95 at 150%, with the label 2 px and 3 px further in, where a
    //  title starts. A tool window's tab, with no squares, is 98, 101 and 107
    //  px: VS's bottom tab for the same label is 104 px at 150%, its label
    //  the same 3 px nearer the tab's start.
    TEST_METHOD (MeasureTab_TakesVisualStudiosWidth)
    {
        constexpr UINT  kDpis[]       = { 96, 120, 144 };
        constexpr int   kWidths[]     = { 146, 161, 179 };
        constexpr int   kToolWidths[] = { 98, 101, 107 };
        constexpr LONG  kLabelPx      = 81;



        for (size_t i = 0; i < std::size (kDpis); i++)
        {
            DxuiDpiScaler         scaler;
            DxuiTabStrip::Tab     tab;
            MockDxuiTextRenderer  text;
            std::wstring          at = std::format (L"{} dpi", kDpis[i]);

            tab.label = L"Memory 3";

            scaler.SetDpi         (kDpis[i]);
            text.SetCannedMetrics (tab.label, SIZE { kLabelPx, 16 });

            Assert::AreEqual (kWidths[i],     DxuiTabStrip::MeasureTabPx (&text, tab, DxuiTabStrip::Style::Document,   true,  scaler), (L"a document tab, " + at).c_str());
            Assert::AreEqual (kToolWidths[i], DxuiTabStrip::MeasureTabPx (&text, tab, DxuiTabStrip::Style::ToolWindow, false, scaler), (L"a tool window's tab, " + at).c_str());
        }
    }

    //  A document tab's pin and close squares at 100%, 125% and 150%: 24, 30
    //  and 36 px across, the close square ending a line in from the tab's
    //  right end and the pin's beside it, between the tab's outline and the
    //  line along the band; the close ink then ends 12 px in at 150%, 36 px
    //  from the pin's.
    TEST_METHOD (TabButtons_TakeVisualStudiosSquares)
    {
        constexpr UINT  kDpis[]  = { 96, 120, 144 };
        constexpr long  kSizes[] = { 24, 30, 36 };
        constexpr long  kLines[] = { 1, 1, 2 };
        constexpr long  kRight   = 400;
        constexpr long  kBand    = 38;



        for (size_t i = 0; i < std::size (kDpis); i++)
        {
            DxuiDpiScaler                   scaler;
            DxuiTabStrip                    ts;
            std::vector<DxuiTabStrip::Tab>  tabs;
            RECT                            pin   = {};
            RECT                            close = {};
            std::wstring                    at    = std::format (L"{} dpi", kDpis[i]);

            scaler.SetDpi  (kDpis[i]);
            tabs.push_back (MakeTab (200, 0, kRight, kBand, L"Memory 3"));
            ts.SetTabs     (tabs);
            ts.SetStyle    (DxuiTabStrip::Style::Document);
            ts.Layout      (RECT { 0, 0, 600, kBand }, scaler);

            pin   = ts.GetTabButtonRect (0, DxuiTabStrip::TabButton::Pin);
            close = ts.GetTabButtonRect (0, DxuiTabStrip::TabButton::Close);

            Assert::AreEqual (kRight - kLines[i], close.right,              (L"the close square ends a line in, " + at).c_str());
            Assert::AreEqual (kSizes[i],          close.right - close.left, (L"its width, " + at).c_str());
            Assert::AreEqual (close.left,         pin.right,                (L"the pin beside it, " + at).c_str());
            Assert::AreEqual (kSizes[i],          pin.right - pin.left,     (L"its width, " + at).c_str());
            Assert::AreEqual (kLines[i],          close.top,                (L"below the tab's outline, " + at).c_str());
            Assert::AreEqual (kBand,              close.bottom,             (L"to the line, " + at).c_str());
        }
    }

    //  The selected tab and the hovered tab draw their pin and close glyphs,
    //  sized so their ink is 16 px across at 150%: the pin at 10.67 DIP,
    //  13.34 and 16 px at 125% and 150%, and the close at 14 DIP, 17.5 and
    //  21 px. A tab neither selected nor hovered draws none.
    TEST_METHOD (TabButtons_ShowOnTheSelectedAndHoveredTabs)
    {
        constexpr UINT   kDpis[]       = { 96, 120, 144 };
        constexpr float  kPinPx[]      = { 10.67f, 13.34f, 16.0f };
        constexpr float  kClosePx[]    = { 14.0f, 17.5f, 21.0f };
        constexpr float  kToleranceDip = 0.01f;



        for (size_t i = 0; i < std::size (kDpis); i++)
        {
            DxuiDpiScaler         scaler;
            DxuiTabStrip          ts;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            size_t                pins   = 0;
            size_t                closes = 0;
            std::wstring          at     = std::format (L"{} dpi", kDpis[i]);

            scaler.SetDpi    (kDpis[i]);
            ts.SetTabs       (MakeThreeTabs());
            ts.SetStyle      (DxuiTabStrip::Style::Document);
            ts.SetOnPin      ([] (int) {});
            ts.SetOnClose    ([] (int) {});
            ts.Layout        (RECT { 0, 0, 300, 24 }, scaler);
            ts.SetSelected   (0);
            ts.SetMouseHover (100, 12);
            ts.Paint         (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == s_kpszMdl2Pinned)
                {
                    pins++;
                    Assert::AreEqual (kPinPx[i], call.fontSizeDip, kToleranceDip, (L"the pin's size, " + at).c_str());
                }
                else if (call.text == s_kpszMdl2Cancel)
                {
                    closes++;
                    Assert::AreEqual (kClosePx[i], call.fontSizeDip, kToleranceDip, (L"the close button's size, " + at).c_str());
                }
            }

            Assert::AreEqual ((size_t) 2, pins,   (L"a pin on the selected tab and the hovered one, " + at).c_str());
            Assert::AreEqual ((size_t) 2, closes, (L"and a close button, " + at).c_str());
        }
    }

    //  The tab under the pointer has its label and its glyphs a step nearer
    //  the foreground than any other tab's, the selected tab of a pane not in
    //  use included: Visual Studio's #D9D9D9 and #D3D3D3 beside #D7D7D7 and
    //  #D1D1D1.
    TEST_METHOD (CompactTab_TheHoveredTabsInksAreAStepBrighter)
    {
        DxuiTabStrip                    ts;
        std::vector<DxuiTabStrip::Tab>  tabs;
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        uint32_t                        label   = DxuiTabStrip::GetLabelInk (theme, false);
        uint32_t                        glyph   = DxuiTabStrip::GetGlyphInk (theme, false);
        uint32_t                        hovered = DxuiTabStrip::GetHoveredLabelInk (theme);
        uint32_t                        hoverGl = DxuiTabStrip::GetHoveredGlyphInk (theme);
        size_t                          labels  = 0;
        size_t                          glyphs  = 0;



        Assert::AreNotEqual (label, hovered, L"the hovered label's ink differs");
        Assert::AreNotEqual (glyph, hoverGl, L"and so does its glyphs'");

        //  Tabs wide enough for their whole labels beside their buttons.
        tabs.push_back (MakeTab (  0, 0, 160, 24, L"Machine"));
        tabs.push_back (MakeTab (160, 0, 320, 24, L"Hardware"));
        tabs.push_back (MakeTab (320, 0, 480, 24, L"Display"));

        ts.SetTabs       (tabs);
        ts.SetStyle      (DxuiTabStrip::Style::Document);
        ts.SetOnPin      ([] (int) {});
        ts.SetOnClose    ([] (int) {});
        ts.SetSelected   (0);
        LayOut (ts, 500);
        ts.SetMouseHover (200, 12);
        ts.Paint         (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            bool  onHovered = call.x >= 160.0f && call.x < 320.0f;
            bool  isGlyph   = call.text == s_kpszMdl2Pinned || call.text == s_kpszMdl2Cancel;
            bool  isLabel   = call.text == L"Machine" || call.text == L"Hardware" || call.text == L"Display";

            if (isLabel)
            {
                labels++;
                Assert::AreEqual (onHovered ? hovered : label, call.argb, (L"the label " + call.text).c_str());
            }
            else if (isGlyph)
            {
                glyphs++;
                Assert::AreEqual (onHovered ? hoverGl : glyph, call.argb, L"a pin or close glyph");
            }
        }

        Assert::AreEqual ((size_t) 3, labels, L"every label");
        Assert::AreEqual ((size_t) 4, glyphs, L"the pin and close of the selected and the hovered tabs");
    }

    //  A compact tab sized by MeasureTabPx shows its whole label at any
    //  scale. The label is placed in whole pixels, so the tab is measured in
    //  them too: at 102 dpi, where 8 DIP is 8.5 px, padding measured as 17
    //  px left a label room 1 px short of itself, and it was cut short.
    TEST_METHOD (CompactTab_ShowsTheLabelItWasMeasuredFor)
    {
        constexpr UINT  kDpis[]  = { 96, 102, 105, 106, 120, 144 };
        constexpr long  kTop     = 0;
        constexpr long  kWidthPx = 600;



        for (UINT dpi : kDpis)
        {
            DxuiDpiScaler                   scaler;
            DxuiTabStrip                    ts;
            DxuiTabStrip::Tab               tab;
            std::vector<DxuiTabStrip::Tab>  tabs;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            MockDxuiTheme                   theme;
            long                            width = 0;
            bool                            whole = false;
            std::wstring                    at    = std::format (L"{} dpi", dpi);

            scaler.SetDpi (dpi);
            tab.label = L"Registers";
            width     = DxuiTabStrip::MeasureTabPx (&text, tab, DxuiTabStrip::Style::Document, false, scaler);
            tab.rect  = RECT { 0, kTop, width, kTop + scaler.ToPx (DxuiTabGroup::kStripDip) };
            tabs.push_back (tab);

            ts.SetStyle    (DxuiTabStrip::Style::Document);
            ts.SetTabs     (tabs);
            ts.Layout      (RECT { 0, kTop, kWidthPx, tab.rect.bottom }, scaler);
            ts.SetSelected (0);
            ts.Paint       (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                whole = whole || (call.kind == RecordedTextKind::DrawString && call.text == L"Registers");
            }

            Assert::IsTrue (whole, (L"the label is drawn whole, " + at).c_str());
        }
    }
};
