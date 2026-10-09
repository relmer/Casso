#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

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
};





////////////////////////////////////////////////////////////////////////////////
//
//  TabStripStandardTests
//
//  The Standard style, the default, which every Dxui dialog draws its tabs
//  in: each label centered inside the label pad, an accent underline under
//  the selected tab and nothing else, no scrolling, and a held press that
//  leaves its tab canceled rather than dragged.
//
//  A host sizing its tabs to their labels relies on that pad and on the face
//  and weight the label is drawn in, so all three are pinned here.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TabStripStandardTests)
{
public:

    static DxuiTabStrip::Tab  MakeTab (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiTabStrip::Tab  tab;

        tab.rect  = { l, t, r, b };
        tab.label = label;
        return tab;
    }


    static std::vector<DxuiTabStrip::Tab>  MakeThreeTabs()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;

        tabs.push_back (MakeTab (  0, 0, 100, 36, L"General"));
        tabs.push_back (MakeTab (100, 0, 200, 36, L"Hardware"));
        tabs.push_back (MakeTab (200, 0, 300, 36, L"Display"));
        return tabs;
    }


    static std::vector<DxuiTabStrip::Tab>  MakeTenTabs()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;

        for (int i = 0; i < 10; i++)
        {
            tabs.push_back (MakeTab (i * 80, 0, (i + 1) * 80, 24, std::to_wstring (i).c_str()));
        }

        return tabs;
    }


    static void  LayOut (DxuiTabStrip & ts, LONG width)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        ts.Layout (RECT { 0, 0, width, 36 }, scaler);
    }


    static const RecordedTextCall *  FindLabel (const MockDxuiTextRenderer & text, const wchar_t * label)
    {
        const RecordedTextCall *  found = nullptr;

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == label)
            {
                found = &call;
            }
        }

        return found;
    }


    static const RecordedPaintCall *  FindRoundedFill (const MockDxuiPainter & painter)
    {
        const RecordedPaintCall *  found = nullptr;

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRoundedRect)
            {
                found = &call;
            }
        }

        return found;
    }


    TEST_METHOD (Default_IsStandard)
    {
        DxuiTabStrip  ts;

        Assert::IsTrue (ts.GetStyle() == DxuiTabStripStyle::Standard);
    }


    TEST_METHOD (Paint_LabelIsCenteredInsideThePad)
    {
        DxuiTabStrip              ts;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        MockDxuiPainter           painter;
        const RecordedTextCall  * selected = nullptr;
        const RecordedTextCall  * other    = nullptr;

        ts.SetTabs (MakeThreeTabs());
        LayOut (ts, 400);
        ts.Paint (painter, text, theme);

        selected = FindLabel (text, L"General");
        other    = FindLabel (text, L"Hardware");
        Assert::IsNotNull (selected, L"The selected tab draws its whole label");
        Assert::IsNotNull (other,    L"and so does the next");

        Assert::AreEqual (108.0f, other->x,      L"inset the label pad from the tab's left");
        Assert::AreEqual (4.0f,   other->y);
        Assert::AreEqual (84.0f,  other->width,  L"and from its right");
        Assert::AreEqual (28.0f,  other->height);
        Assert::IsTrue   (other->hAlign == DxuiTextHAlign::Center, L"centered");
        Assert::IsTrue   (other->vAlign == DxuiTextVAlign::Center);
        Assert::IsTrue   (other->wrap);
        Assert::AreEqual (DxuiTabStrip::kLabelFontDip, other->fontSizeDip);
        Assert::AreEqual (std::wstring (DxuiTabStrip::GetLabelFace()), other->fontFamily, L"in the face a host measures in");
        Assert::IsTrue   (other->weight == DxuiFontWeight::Normal);

        Assert::AreEqual (8.0f,  selected->x);
        Assert::AreEqual (84.0f, selected->width);
        Assert::IsTrue   (selected->weight == DxuiFontWeight::Normal, L"A selected label is drawn at normal weight, the weight the host measured at");
    }


    TEST_METHOD (Paint_SelectedTabIsUnderlinedAndTheRestDimmed)
    {
        DxuiTabStrip          ts;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        MockDxuiPainter       painter;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetSelected (1);
        LayOut (ts, 400);
        ts.Paint (painter, text, theme);

        Assert::AreEqual ((size_t) 1, painter.Calls().size(), L"An idle strip paints only the underline: no fill, divider or base line");
        Assert::IsTrue   (painter.Calls()[0].kind == RecordedPaintKind::FillRect);
        Assert::AreEqual (100.0f, painter.Calls()[0].x);
        Assert::AreEqual (33.0f,  painter.Calls()[0].y, L"3 dip tall, flush with the tab's bottom");
        Assert::AreEqual (100.0f, painter.Calls()[0].width);
        Assert::AreEqual (3.0f,   painter.Calls()[0].height);
        Assert::AreEqual (MockDxuiTheme::s_kSelectionBackground, painter.Calls()[0].argb);

        Assert::AreEqual ((size_t) 3, text.Calls().size(), L"Three labels and nothing else: no clip, arrows or + button");
        Assert::AreEqual (MockDxuiTheme::s_kForeground, FindLabel (text, L"Hardware")->argb, L"The selected label is full strength");
        Assert::AreEqual (DxuiColor::Scale (MockDxuiTheme::s_kForeground, 0.62f), FindLabel (text, L"General")->argb, L"and the rest dimmed");
    }


    TEST_METHOD (Paint_HoveredTabGetsARoundedFillDarkerWhilePressed)
    {
        DxuiTabStrip               ts;
        MockDxuiTextRenderer       text;
        MockDxuiTheme              theme;
        MockDxuiPainter            painter;
        const RecordedPaintCall  * fill = nullptr;

        ts.SetTabs (MakeThreeTabs());
        LayOut (ts, 400);

        ts.SetMouseHover (150, 10);
        ts.Paint (painter, text, theme);

        fill = FindRoundedFill (painter);
        Assert::IsNotNull (fill, L"A hovered tab is filled");
        Assert::AreEqual  (100.0f, fill->x);
        Assert::AreEqual  (0.0f,   fill->y);
        Assert::AreEqual  (100.0f, fill->width);
        Assert::AreEqual  (36.0f,  fill->height, L"the whole tab");
        Assert::AreEqual  (DxuiTheme::kCornerRadiusDip, fill->radius);
        Assert::AreEqual  (MockDxuiTheme::s_kHoverBackground, fill->argb);

        painter.Reset();
        ts.OnLButtonDown (150, 10);
        ts.Paint (painter, text, theme);

        fill = FindRoundedFill (painter);
        Assert::IsNotNull (fill);
        Assert::AreEqual  (DxuiColor::Darken (MockDxuiTheme::s_kHoverBackground, 0.82f), fill->argb, L"and darker while pressed");
    }


    TEST_METHOD (Paint_CloseButtonTakesItsRoomFromTheLabel)
    {
        DxuiTabStrip              ts;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        MockDxuiPainter           painter;
        const RecordedTextCall  * label = nullptr;

        ts.SetTabs    (MakeThreeTabs());
        ts.SetOnClose ([] (int) {});
        LayOut (ts, 400);
        ts.Paint (painter, text, theme);

        label = FindLabel (text, L"General");
        Assert::IsNotNull (label);
        Assert::AreEqual  (8.0f,  label->x);
        Assert::AreEqual  (58.0f, label->width, L"The label stops at the close button, 22 dip in from the tab's right edge less half its 24 dip box");
        Assert::IsNotNull (FindLabel (text, s_kpszMdl2ChromeClose), L"which is drawn");
    }


    TEST_METHOD (PressThenMoveOffTheTab_CancelsAndMovesNothing)
    {
        //  Without a move handler the strip cannot reorder the host's tabs, so
        //  a press that leaves its tab is canceled, never dragged.
        DxuiTabStrip  ts;
        int           changes = 0;

        ts.SetTabs     (MakeThreeTabs());
        ts.SetOnChange ([&] (int) { changes++; });

        Assert::IsTrue   (ts.OnLButtonDown (10, 10));
        Assert::IsFalse  (ts.OnMouseMove   (250, 10), L"A move off the pressed tab is no drag");
        Assert::IsFalse  (ts.IsInteracting(), L"and cancels the press");
        Assert::IsFalse  (ts.OnLButtonUp   (250, 10));

        Assert::AreEqual (std::wstring (L"General"), ts.GetTabs()[0].label, L"Nothing moved");
        Assert::AreEqual (std::wstring (L"Display"), ts.GetTabs()[2].label);
        Assert::AreEqual (0, ts.GetSelected());
        Assert::AreEqual (0, changes, L"and nothing was reported");
    }


    TEST_METHOD (Overflow_ShowsNoArrowsAndDoesNotScroll)
    {
        DxuiTabStrip          ts;
        DxuiMouseEvent        wheel;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        MockDxuiPainter       painter;

        ts.SetTabs (MakeTenTabs());
        LayOut (ts, 240);

        Assert::IsFalse  (ts.HasScrollArrows());
        Assert::AreEqual (0, ts.HitTest (10, 10),  L"The first tab starts at the strip's edge");
        Assert::AreEqual (5, ts.HitTest (410, 10), L"and a tab past the strip's end is still a tab");
        Assert::IsFalse  (ts.OnWheel (-1.0f),      L"The wheel scrolls nothing");

        wheel.kind       = DxuiMouseEventKind::Wheel;
        wheel.wheelDelta = -1.0f;
        Assert::IsFalse  (ts.OnMouse (wheel), L"and passes through to the host");

        ts.SetSelected (9);
        Assert::AreEqual (0, ts.GetScrollPx(), L"Selecting a tab past the end scrolls nothing");

        ts.Paint (painter, text, theme);
        Assert::AreEqual ((size_t) 10, text.Calls().size(), L"Ten labels and nothing else: no clip and no arrows");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  TabStripExplorerTests
//
//  The Explorer style, File Explorer's tab row: labels inset for an icon and
//  cut off rather than wrapped, scroll arrows, the wheel and drag scrolling
//  when the tabs overflow, and the handler-driven drag, + button and close
//  buttons the host turns on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TabStripExplorerTests)
{
public:

    static DxuiTabStrip::Tab  MakeTab (int l, int t, int r, int b, const wchar_t * label)
    {
        DxuiTabStrip::Tab  tab;

        tab.rect  = { l, t, r, b };
        tab.label = label;
        return tab;
    }


    static std::vector<DxuiTabStrip::Tab>  MakeThreeTabs()
    {
        std::vector<DxuiTabStrip::Tab>  tabs;

        tabs.push_back (MakeTab (  0, 0,  80, 24, L"Machine"));
        tabs.push_back (MakeTab ( 80, 0, 160, 24, L"Hardware"));
        tabs.push_back (MakeTab (160, 0, 240, 24, L"Display"));
        return tabs;
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


    TEST_METHOD (Paint_LabelStartsPastTheIconRoom)
    {
        DxuiTabStrip              ts;
        MockDxuiTextRenderer      text;
        MockDxuiTheme             theme;
        MockDxuiPainter           painter;
        const RecordedTextCall  * label = nullptr;

        ts.SetStyle (DxuiTabStripStyle::Explorer);
        ts.SetTabs  ({ MakeTab (0, 0, 240, 24, L"Machine") });
        ts.Paint    (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == L"Machine")
            {
                label = &call;
            }
        }

        Assert::IsNotNull (label);
        Assert::AreEqual  (34.0f, label->x, L"Explorer's label starts past the room for its icon");
        Assert::IsTrue    (label->hAlign == DxuiTextHAlign::Left, L"left-aligned");
        Assert::IsTrue    (label->weight == DxuiFontWeight::SemiBold, L"and semibold on the selected tab");
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
        ts.SetStyle (DxuiTabStripStyle::Explorer);
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


    TEST_METHOD (Drag_MovesTheTabAndReportsEachMove)
    {
        DxuiTabStrip  ts;
        int           from = -1;
        int           to   = -1;

        ts.SetStyle  (DxuiTabStripStyle::Explorer);
        ts.SetTabs   (MakeThreeTabs());
        ts.SetOnMove ([&] (int f, int t) { from = f; to = t; });

        Assert::IsTrue   (ts.OnLButtonDown (10, 10));
        Assert::IsTrue   (ts.OnMouseMove   (100, 10), L"A press dragged past the threshold is a drag");
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

        ts.SetStyle    (DxuiTabStripStyle::Explorer);
        ts.SetTabs     (MakeThreeTabs());
        ts.SetOnMove   ([] (int, int) {});
        ts.SetSelected (1);

        ts.OnLButtonDown (10, 10);
        ts.OnMouseMove   (200, 10);
        ts.OnLButtonUp   (200, 10);

        Assert::AreEqual (std::wstring (L"Hardware"), ts.GetTabs()[0].label);
        Assert::AreEqual (2, ts.GetSelected(), L"Releasing a drag selects the tab it moved");
    }


    TEST_METHOD (SmallMove_IsStillAClick)
    {
        DxuiTabStrip  ts;

        ts.SetStyle  (DxuiTabStripStyle::Explorer);
        ts.SetTabs   (MakeThreeTabs());
        ts.SetOnMove ([] (int, int) {});

        ts.OnLButtonDown (100, 10);
        Assert::IsFalse (ts.OnMouseMove (102, 10), L"A move inside the threshold is not a drag");
        Assert::IsTrue  (ts.OnLButtonUp (102, 10));

        Assert::AreEqual (1, ts.GetSelected());
        Assert::AreEqual (std::wstring (L"Machine"), ts.GetTabs()[0].label, L"and nothing moved");
    }


    TEST_METHOD (Overflow_WheelScrollsAndHitTestFollows)
    {
        DxuiTabStrip  ts;

        ts.SetStyle (DxuiTabStripStyle::Explorer);
        ts.SetTabs  (MakeTenTabs());
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

        ts.SetStyle (DxuiTabStripStyle::Explorer);
        ts.SetTabs  (MakeTenTabs());
        LayOut (ts, 240);

        ts.SetSelected (9);
        Assert::AreEqual (616, ts.GetScrollPx());

        ts.SetSelected (0);
        Assert::AreEqual (0, ts.GetScrollPx());
    }


    TEST_METHOD (Overflow_DragHeldPastTheEndReachesTheLastPlace)
    {
        DxuiTabStrip  ts;

        ts.SetStyle  (DxuiTabStripStyle::Explorer);
        ts.SetTabs   (MakeTenTabs());
        ts.SetOnMove ([] (int, int) {});
        LayOut (ts, 240);

        ts.OnLButtonDown (40, 10);

        for (int i = 0; i < 100; i++)
        {
            ts.OnMouseMove (400, 10);
        }

        ts.OnLButtonUp (400, 10);

        Assert::AreEqual (std::wstring (L"0"), ts.GetTabs()[9].label, L"A drag past the right end moves its tab to the last place");
        Assert::AreEqual (9,   ts.GetSelected());
        Assert::AreEqual (616, ts.GetScrollPx());
    }


    TEST_METHOD (TabsThatFit_ShowNoArrows)
    {
        DxuiTabStrip  ts;

        ts.SetStyle (DxuiTabStripStyle::Explorer);
        ts.SetTabs  (MakeThreeTabs());
        LayOut (ts, 300);

        Assert::IsFalse  (ts.HasScrollArrows());
        Assert::AreEqual (0, ts.HitTest (10, 10), L"With no arrows the first tab starts at the strip's edge");
    }


    TEST_METHOD (Arrows_ScrollOneTabAtATime)
    {
        DxuiTabStrip  ts;

        ts.SetStyle (DxuiTabStripStyle::Explorer);
        ts.SetTabs  (MakeTenTabs());
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

        ts.SetStyle    (DxuiTabStripStyle::Explorer);
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

        ts.SetStyle    (DxuiTabStripStyle::Explorer);
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

        ts.SetStyle    (DxuiTabStripStyle::Explorer);
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
};
