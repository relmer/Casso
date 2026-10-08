#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewStretchColumnTests
//
//  A stretch column fills the width the other columns leave, but is never
//  narrower than its declared width. Previously, in a pane narrower than the
//  fixed columns it got zero width while the horizontal scroll range still
//  included its declared width, so CassoExplorer's file list opened without its Name
//  column.
//
//  96 DPI, so a DIP is a pixel.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewStretchColumnTests)
{
public:

    static RECT  MakeRect (LONG width)
    {
        RECT  out = {};

        out.right  = width;
        out.bottom = 300;
        return out;
    }


    static void  LayOut (DxuiListView & list, int nameWidthDip, LONG paneWidth)
    {
        DxuiDpiScaler                      scaler;
        std::vector<DxuiListView::Column>  cols;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", nameWidthDip, true  });
        cols.push_back (DxuiListView::Column { L"Type", 70,           false });
        cols.push_back (DxuiListView::Column { L"Size", 92,           false });

        list.SetColumns    (std::move (cols));
        list.SetShowHeader (false);
        list.Layout        (MakeRect (paneWidth), scaler);
    }


    TEST_METHOD (NarrowPane_StretchColumnKeepsItsDeclaredWidth)
    {
        DxuiListView  list;


        LayOut (list, 200, 150);

        Assert::AreEqual (200, list.GetColumnEffectiveWidthPx (0),
            L"A pane too narrow for the fixed columns still draws the stretch column at its declared width");
    }


    TEST_METHOD (WidePane_StretchColumnStillFillsTheRest)
    {
        DxuiListView  list;
        int           total = 0;


        LayOut (list, 200, 1000);

        total = list.GetColumnEffectiveWidthPx (0)
              + list.GetColumnEffectiveWidthPx (1)
              + list.GetColumnEffectiveWidthPx (2);

        Assert::IsTrue   (list.GetColumnEffectiveWidthPx (0) > 200,
            L"With room to spare the stretch column grows past its declared width");
        Assert::AreEqual (1000, total,
            L"and the columns fill the pane exactly");
    }


    TEST_METHOD (StretchColumnDeclaredAtZero_StillShrinksToFit)
    {
        DxuiListView  list;


        LayOut (list, 0, 150);

        Assert::AreEqual (0, list.GetColumnEffectiveWidthPx (0),
            L"A stretch column with no declared width has no floor, as before");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewCopyTests
//
//  The text Copy puts on the clipboard, and when Copy is offered at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewCopyTests)
{
public:

    TEST_METHOD (SelectionText_IsTheSelectedRowsInOrder_CellsTabSeparated)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        bool                                          enabled = false;

        rows.push_back ({ DxuiListView::Cell { L"HELLO", false }, DxuiListView::Cell { L"BAS", false } });
        rows.push_back ({ DxuiListView::Cell { L"NOTES", false }, DxuiListView::Cell { L"TXT", false } });
        rows.push_back ({ DxuiListView::Cell { L"ODD",   false }, DxuiListView::Cell { L"BIN", false } });

        list.SetMultiSelect (true);
        list.SetRows (std::move (rows));

        Assert::IsFalse  (list.QueryCommand (DxuiStandardCommand::Copy, enabled),
            L"Copy is offered only by a list with a window to own the clipboard");

        list.SetOwnerWindow ((HWND) 1);
        list.SelectAllRows();

        Assert::IsTrue   (list.QueryCommand (DxuiStandardCommand::Copy, enabled));
        Assert::IsTrue   (enabled);
        Assert::AreEqual (std::wstring (L"HELLO\tBAS\r\nNOTES\tTXT\r\nODD\tBIN\r\n"), list.GetSelectionText());
    }


    TEST_METHOD (Drag_SelectsTheRowsItCrosses)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        DxuiDpiScaler                                 scaler;
        DxuiMouseEvent                                ev;
        int                                           rowH = 0;

        for (int i = 0; i < 10; i++)
        {
            rows.push_back ({ DxuiListView::Cell { std::to_wstring (i), false } });
        }

        scaler.SetDpi (96);
        list.SetColumns     ({ DxuiListView::Column { L"", 0, true } });
        list.SetShowHeader  (false);
        list.SetMultiSelect (true);
        list.SetRows        (std::move (rows));
        list.Layout         (RECT { 0, 0, 300, 600 }, scaler);

        rowH           = list.GetRowHeightDip();
        ev.button      = DxuiMouseButton::Left;
        ev.kind        = DxuiMouseEventKind::Down;
        ev.positionDip = POINT { 10, rowH / 2 };
        list.OnMouse (ev);

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = POINT { 10, rowH * 3 + rowH / 2 };
        list.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);

        Assert::AreEqual ((size_t) 4, list.GetSelectedRows().size(), L"A drag from the first row to the fourth selects all four");
        Assert::IsFalse  (list.IsInteracting(), L"and releasing the button ends the drag");
    }


    TEST_METHOD (EndDragSelect_EndsTheDragWithoutARelease)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        DxuiDpiScaler                                 scaler;
        DxuiMouseEvent                                ev;
        int                                           rowH = 0;

        for (int i = 0; i < 10; i++)
        {
            rows.push_back ({ DxuiListView::Cell { std::to_wstring (i), false } });
        }

        scaler.SetDpi (96);
        list.SetColumns     ({ DxuiListView::Column { L"", 0, true } });
        list.SetShowHeader  (false);
        list.SetMultiSelect (true);
        list.SetRows        (std::move (rows));
        list.Layout         (RECT { 0, 0, 300, 600 }, scaler);

        rowH           = list.GetRowHeightDip();
        ev.button      = DxuiMouseButton::Left;
        ev.kind        = DxuiMouseEventKind::Down;
        ev.positionDip = POINT { 10, rowH / 2 };
        list.OnMouse (ev);

        Assert::IsTrue (list.IsInteracting(), L"A press on a row starts a drag that extends the selection");

        list.EndDragSelect();

        Assert::IsFalse  (list.IsInteracting(), L"which ends when the host takes the drag over");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size(), L"leaving the pressed row selected");

        ev.kind        = DxuiMouseEventKind::Move;
        ev.button      = DxuiMouseButton::None;
        ev.positionDip = POINT { 10, rowH * 3 + rowH / 2 };
        list.OnMouse (ev);

        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size(), L"so a later move selects nothing more");
    }


    TEST_METHOD (FitColumnToContent_WidensAFixedWidthColumnToItsLongestValue)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        DxuiDpiScaler                                 scaler;
        MockDxuiPainter                               painter;
        MockDxuiTextRenderer                          text;
        MockDxuiTheme                                 theme;

        rows.push_back ({ DxuiListView::Cell { L"short", false } });
        rows.push_back ({ DxuiListView::Cell { L"a much longer name", false } });

        text.SetCannedMetrics (L"short",              SIZE { 30, 16 });
        text.SetCannedMetrics (L"a much longer name", SIZE { 240, 16 });

        scaler.SetDpi (96);
        list.SetColumns    ({ DxuiListView::Column { L"", 50, false } });
        list.SetShowHeader (false);
        list.SetRows       (std::move (rows));
        list.Layout        (RECT { 0, 0, 600, 300 }, scaler);

        list.FitColumnToContent (0);
        list.Paint (painter, text, theme);

        Assert::IsTrue (list.GetColumnEffectiveWidthPx (0) >= 240,
                        L"A column declared at a fixed width fits its longest value, as Explorer's does");
        Assert::IsTrue (list.GetColumnEffectiveWidthPx (0) < 280, L"and no wider than that and its padding");
    }


    TEST_METHOD (SetDpi_ScalesAWidthTheUserGaveAColumn)
    {
        DxuiListView  list;

        list.SetDpi     (96);
        list.SetColumns ({ DxuiListView::Column { L"Name", 250, false }, DxuiListView::Column { L"Size", 80, false } });
        list.SetColumnOverrideWidthPx (0, 110);

        list.SetDpi (144);

        Assert::AreEqual (165, list.GetColumnOverrideWidthPx (0), L"A width set at 96 dpi is half again as wide at 144");
        Assert::AreEqual (-1,  list.GetColumnOverrideWidthPx (1), L"and a column with none still has none");
    }
};