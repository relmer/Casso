#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

#include "Widgets/DxuiScrollPanel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollPanelTests
//
//  A list of rows in a viewport four rows tall. Each row is 24 px high on a
//  30 px step, inset 3 px from the viewport's top, so the viewport runs from
//  100 to 220 and six rows need 180 px: two rows' worth of scrolling.
//
//  Covered: the range and the scroll position, the wheel (a row per notch,
//  left for the page at either end), input and painting clipped to the
//  viewport inside a clip already in force, and the keyboard: Tab visits
//  every row in order, those scrolled out of view included, before the
//  controls beside and below the list, and scrolls the one it reaches into
//  view.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiScrollPanelTests)
{
public:

    static constexpr int  kLeftPx     = 10;
    static constexpr int  kRightPx    = 130;
    static constexpr int  kTopPx      = 100;
    static constexpr int  kInsetPx    = 3;
    static constexpr int  kRowHPx     = 24;
    static constexpr int  kStepPx     = 30;
    static constexpr int  kShownRows  = 4;
    static constexpr int  kBottomPx   = kTopPx + kInsetPx + (kShownRows - 1) * kStepPx + kRowHPx + kInsetPx;
    static constexpr int  kRowRightPx = kRightPx - DxuiScrollPanel::kScrollbarWidthDip;


    //  Adds `count` rows to the panel, places them, and lays the panel out.
    static std::vector<MockDxuiControl *> AddRows (DxuiScrollPanel & panel, size_t count)
    {
        std::vector<MockDxuiControl *>  rows;
        size_t                          i    = 0;



        for (i = 0; i < count; i++)
        {
            rows.push_back (&panel.Add<MockDxuiControl>());
        }

        PlaceAndLayOut (panel, rows);
        return rows;
    }


    //  Places each row on its step and lays the panel out at 96 DPI.
    static void PlaceAndLayOut (DxuiScrollPanel & panel, const std::vector<MockDxuiControl *> & rows)
    {
        DxuiDpiScaler  scaler;
        size_t         i      = 0;



        for (i = 0; i < rows.size(); i++)
        {
            int  top = kTopPx + kInsetPx + (int) i * kStepPx;

            panel.PlaceChild (*rows[i], RECT { kLeftPx, top, kRowRightPx, top + kRowHPx });
        }

        panel.SetLineStepPx (kStepPx);
        panel.Layout        (RECT { kLeftPx, kTopPx, kRightPx, kBottomPx }, scaler);
    }


    static DxuiMouseEvent MakeWheel (int x, int y, float notches)
    {
        DxuiMouseEvent  ev;



        ev.kind        = DxuiMouseEventKind::Wheel;
        ev.positionDip = { x, y };
        ev.wheelDelta  = notches;
        return ev;
    }


    static DxuiMouseEvent MakeMouse (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;



        ev.kind        = kind;
        ev.positionDip = { x, y };
        ev.button      = DxuiMouseButton::Left;
        return ev;
    }


    TEST_METHOD (Layout_RowsThatFitDoNotScroll)
    {
        DxuiScrollPanel                  panel;
        std::vector<MockDxuiControl *>   rows  = AddRows (panel, 3);



        Assert::IsFalse  (panel.IsScrollable());
        Assert::AreEqual (0, panel.GetScrollPosPx());
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx), rows[0]->GetBounds().top, L"the first row where it was placed");
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx + 2 * kStepPx), rows[2]->GetBounds().top);
        Assert::AreEqual ((LONG) kBottomPx, panel.GetBounds().bottom, L"the panel's bounds are its viewport");
    }


    //  The content ends as far below the last row as the first sits below the
    //  top: 3 + 5 * 30 + 24 + 3.
    TEST_METHOD (Layout_RowsPastTheViewportScroll)
    {
        DxuiScrollPanel  panel;



        AddRows (panel, 6);

        Assert::IsTrue   (panel.IsScrollable());
        Assert::AreEqual (180, panel.GetContentHeightPx());
        Assert::AreEqual (2 * kStepPx, panel.GetMaxScrollPosPx());
    }


    TEST_METHOD (ScrollPos_MovesTheRowsAndIsClamped)
    {
        DxuiScrollPanel                  panel;
        std::vector<MockDxuiControl *>   rows  = AddRows (panel, 6);



        panel.SetScrollPosPx (kStepPx);
        Assert::AreEqual (kStepPx, panel.GetScrollPosPx());
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx - kStepPx), rows[0]->GetBounds().top, L"moved up a row");
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx + 4 * kStepPx), rows[5]->GetBounds().top);

        panel.SetScrollPosPx (1000);
        Assert::AreEqual (2 * kStepPx, panel.GetScrollPosPx(), L"no further than the last row");

        panel.SetScrollPosPx (-5);
        Assert::AreEqual (0, panel.GetScrollPosPx(), L"no further than the first");
    }


    //  Laid out again, the panel keeps its place; once the rows fit, it goes
    //  back to the top.
    TEST_METHOD (Relayout_KeepsThePositionUntilTheRowsFit)
    {
        DxuiScrollPanel                  panel;
        std::vector<MockDxuiControl *>   rows  = AddRows (panel, 6);



        panel.SetScrollPosPx (2 * kStepPx);
        PlaceAndLayOut (panel, rows);

        Assert::AreEqual (2 * kStepPx, panel.GetScrollPosPx());
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx - 2 * kStepPx), rows[0]->GetBounds().top);

        rows[4]->SetVisible (false);
        rows[5]->SetVisible (false);
        PlaceAndLayOut (panel, rows);

        Assert::IsFalse  (panel.IsScrollable());
        Assert::AreEqual (0, panel.GetScrollPosPx());
        Assert::AreEqual ((LONG) (kTopPx + kInsetPx), rows[0]->GetBounds().top);
    }


    //  A notch is a row. At either end the turn is not taken, so the page
    //  around the panel scrolls instead, as a browser does.
    TEST_METHOD (Wheel_ScrollsARowPerNotchAndPassesOnAtTheEnds)
    {
        DxuiScrollPanel  panel;
        int              x     = kLeftPx + 20;
        int              y     = kTopPx + 40;



        AddRows (panel, 6);

        Assert::IsFalse  (panel.OnMouse (MakeWheel (x, y, 1.0f)),  L"up at the top is the page's");
        Assert::IsTrue   (panel.OnMouse (MakeWheel (x, y, -1.0f)), L"down scrolls");
        Assert::AreEqual (kStepPx, panel.GetScrollPosPx());
        Assert::IsTrue   (panel.OnMouse (MakeWheel (x, y, -1.0f)));
        Assert::AreEqual (2 * kStepPx, panel.GetScrollPosPx());
        Assert::IsFalse  (panel.OnMouse (MakeWheel (x, y, -1.0f)), L"down at the bottom is the page's");
        Assert::AreEqual (2 * kStepPx, panel.GetScrollPosPx());
        Assert::IsTrue   (panel.OnMouse (MakeWheel (x, y, 1.0f)),  L"and up scrolls back");
        Assert::AreEqual (kStepPx, panel.GetScrollPosPx());
        Assert::IsFalse  (panel.OnMouse (MakeWheel (x, kBottomPx + 20, -1.0f)), L"outside the viewport it is not the panel's");
        Assert::AreEqual (kStepPx, panel.GetScrollPosPx());
    }


    TEST_METHOD (Wheel_OverRowsThatFitIsThePages)
    {
        DxuiScrollPanel  panel;



        AddRows (panel, 4);

        Assert::IsFalse (panel.OnMouse (MakeWheel (kLeftPx + 20, kTopPx + 40, -1.0f)));
    }


    //  A row scrolled out of view lies under the viewport's bottom edge. A
    //  press there must not reach it, but a move still does.
    TEST_METHOD (Press_OutsideTheViewportReachesNoRow)
    {
        DxuiScrollPanel                  panel;
        std::vector<MockDxuiControl *>   rows   = AddRows (panel, 6);
        RECT                             hidden = {};
        POINT                            center = {};



        rows[5]->consumeMouse = true;
        hidden                = rows[5]->GetBounds();
        center                = { (hidden.left + hidden.right) / 2, (hidden.top + hidden.bottom) / 2 };

        Assert::IsTrue   (center.y > kBottomPx, L"the last row is below the viewport");
        Assert::IsFalse  (panel.OnMouse (MakeMouse (DxuiMouseEventKind::Down, center.x, center.y)));
        Assert::AreEqual (0, rows[5]->mouseCount, L"the press did not reach it");
        Assert::IsTrue   (panel.IsPointClipped (center), L"and focus cannot land there");
        Assert::IsFalse  (panel.IsPointClipped (POINT { kLeftPx + 20, kTopPx + 40 }));

        Assert::IsTrue   (panel.OnMouse (MakeMouse (DxuiMouseEventKind::Move, center.x, center.y)), L"a move still reaches it");
    }


    TEST_METHOD (Scrollbar_APressOnItsLowerEndScrolls)
    {
        DxuiScrollPanel  panel;



        AddRows (panel, 6);

        Assert::IsTrue (panel.OnMouse (MakeMouse (DxuiMouseEventKind::Down, kRightPx - 3, kBottomPx - 3)));
        panel.OnMouse (MakeMouse (DxuiMouseEventKind::Up, kRightPx - 3, kBottomPx - 3));

        Assert::IsTrue (panel.GetScrollPosPx() > 0);
    }


    //  Inside a scrolled page's clip, the panel draws within the overlap of
    //  the two, and leaves the page's clip as it found it.
    TEST_METHOD (Paint_ClipsToTheViewportWithinTheClipAround)
    {
        DxuiScrollPanel                  panel;
        std::vector<MockDxuiControl *>   rows  = AddRows (panel, 6);
        MockDxuiPainter                  painter;
        MockDxuiTextRenderer             text;
        MockDxuiTheme                    theme;
        RECT                             page  = { 0, 150, 500, 500 };
        RECT                             after = {};



        painter.SetClipRect (&page);
        panel.Paint (painter, text, theme);

        Assert::IsTrue ((size_t) 0 < painter.Calls().size(), L"the scrollbar is drawn");
        Assert::AreEqual (1, rows[0]->paintCount, L"and the rows");

        for (const RecordedPaintCall & call : painter.Calls())
        {
            Assert::IsTrue   (call.isClipped);
            Assert::AreEqual ((LONG) kLeftPx,   call.clip.left);
            Assert::AreEqual ((LONG) 150,       call.clip.top, L"the page's top, below the viewport's");
            Assert::AreEqual ((LONG) kRightPx,  call.clip.right);
            Assert::AreEqual ((LONG) kBottomPx, call.clip.bottom);
        }

        Assert::IsTrue   (painter.GetClipRect (after), L"the page's clip is back");
        Assert::AreEqual (page.top,    after.top);
        Assert::AreEqual (page.bottom, after.bottom);

        Assert::IsTrue   (text.Calls().front().kind == RecordedTextKind::PushClipRect, L"text is clipped too");
        Assert::AreEqual ((float) kTopPx, text.Calls().front().y);
        Assert::IsTrue   (text.Calls().back().kind  == RecordedTextKind::PopClipRect);
    }


    TEST_METHOD (Paint_WithNoClipAroundLeavesNone)
    {
        DxuiScrollPanel        panel;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        MockDxuiTheme          theme;
        RECT                   after   = {};



        AddRows (panel, 6);
        panel.Paint (painter, text, theme);

        Assert::IsTrue  (painter.Calls().front().isClipped);
        Assert::IsFalse (painter.GetClipRect (after));
    }


    //  Rows 5 and 6 lie under the viewport, level with the control below the
    //  list. Tab still takes the rows in order, then the control beside the
    //  first row, then the one below.
    TEST_METHOD (Tab_VisitsEveryRowInOrderBeforeWhatIsBesideAndBelow)
    {
        DxuiPanel                        root;
        MockDxuiControl                & above  = root.Add<MockDxuiControl>();
        DxuiScrollPanel                & panel  = root.Add<DxuiScrollPanel>();
        MockDxuiControl                & beside = root.Add<MockDxuiControl>();
        MockDxuiControl                & below  = root.Add<MockDxuiControl>();
        std::vector<MockDxuiControl *>   rows   = AddRows (panel, 6);
        DxuiFocusManager                 focus;
        size_t                           i      = 0;



        above.SetBounds  (RECT { kLeftPx,       50,             kRightPx,      74 });
        beside.SetBounds (RECT { kRightPx + 6,  kTopPx + 3,     kRightPx + 40, kTopPx + 27 });
        below.SetBounds  (RECT { kLeftPx,       kBottomPx + 20, kRightPx,      kBottomPx + 44 });

        focus.SetRowEpsilonDip (16.0f);
        focus.Attach (&root);

        Assert::AreEqual ((size_t) 9, focus.GetTabOrderCount());
        Assert::AreEqual (static_cast<void *> (&above), static_cast<void *> (focus.GetTabOrderAt (0)));

        for (i = 0; i < rows.size(); i++)
        {
            Assert::AreEqual (static_cast<void *> (rows[i]), static_cast<void *> (focus.GetTabOrderAt (1 + i)));
        }

        Assert::AreEqual (static_cast<void *> (&beside), static_cast<void *> (focus.GetTabOrderAt (7)));
        Assert::AreEqual (static_cast<void *> (&below),  static_cast<void *> (focus.GetTabOrderAt (8)));
    }


    //  Tab onto a row below the viewport scrolls it in; Shift+Tab back to the
    //  first scrolls back up.
    TEST_METHOD (Tab_ToARowOutOfViewScrollsItIn)
    {
        DxuiPanel                        root;
        DxuiScrollPanel                & panel = root.Add<DxuiScrollPanel>();
        std::vector<MockDxuiControl *>   rows  = AddRows (panel, 6);
        DxuiFocusManager                 focus;
        size_t                           i     = 0;



        focus.SetRowEpsilonDip (16.0f);
        focus.Attach (&root);

        for (i = 0; i < rows.size(); i++)
        {
            focus.HandleKey (DxuiFocusKey::Tab);
        }

        Assert::AreEqual (static_cast<void *> (rows[5]), static_cast<void *> (focus.GetFocusedControl()));
        Assert::AreEqual (2 * kStepPx, panel.GetScrollPosPx());
        Assert::IsTrue   (rows[5]->GetBounds().bottom <= kBottomPx, L"the last row is in view");

        for (i = 1; i < rows.size(); i++)
        {
            focus.HandleKey (DxuiFocusKey::ShiftTab);
        }

        Assert::AreEqual (static_cast<void *> (rows[0]), static_cast<void *> (focus.GetFocusedControl()));
        Assert::IsTrue   (rows[0]->GetBounds().top >= kTopPx, L"the first row is in view again");
    }


    //  A click gives focus without scrolling, even to a row partly out of
    //  view: the list does not move under the pointer.
    TEST_METHOD (Click_FocusesWithoutScrolling)
    {
        constexpr int                    kHalfRowPx = kStepPx / 2;
        DxuiPanel                        root;
        DxuiScrollPanel                & panel      = root.Add<DxuiScrollPanel>();
        std::vector<MockDxuiControl *>   rows       = AddRows (panel, 6);
        DxuiFocusManager                 focus;



        focus.Attach (&root);
        panel.SetScrollPosPx (kHalfRowPx);

        Assert::IsTrue   (rows[4]->GetBounds().bottom > kBottomPx, L"the fifth row is partly out of view");
        Assert::IsTrue   (focus.FocusAtPoint (POINT { kLeftPx + 20, kBottomPx - 2 }));
        Assert::AreEqual (static_cast<void *> (rows[4]), static_cast<void *> (focus.GetFocusedControl()));
        Assert::AreEqual (kHalfRowPx, panel.GetScrollPosPx());
    }
};
