#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewBrowserShapeTests
//
//  A list configured the way the file browser configures its own: a stretch
//  Name column carrying an icon, five auto-fit columns after it, precise
//  auto-fit, and horizontal scrolling.
//
//  The plain list already keeps its place and reports the row under the
//  pointer, so anything that moves a scrolled click onto the wrong row lives
//  in that combination rather than in the click. Content wider than the pane
//  raises the horizontal bar, which takes a row's worth of height out of the
//  body, and the row capacity that comes back is what both the scroll limit
//  and the hit test are measured against.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewBrowserShapeTests)
{
public:

    static constexpr int  kRows   = 300;
    static constexpr int  kTopRow = 200;
    static constexpr int  kRowX   = 100;



    static void ConfigureLikeTheBrowser (DxuiListView & list)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        int                                           i = 0;

        scaler.SetDpi (96);

        //  CassqueBrowser::GetColumns.
        cols.push_back (DxuiListView::Column { L"Name",     200, true,  DxuiTextHAlign::Left  });
        cols.push_back (DxuiListView::Column { L"Type",     0,   false, DxuiTextHAlign::Left  });
        cols.push_back (DxuiListView::Column { L"Size",     0,   false, DxuiTextHAlign::Right });
        cols.push_back (DxuiListView::Column { L"Address",  0,   false, DxuiTextHAlign::Left  });
        cols.push_back (DxuiListView::Column { L"Locked",   0,   false, DxuiTextHAlign::Left  });
        cols.push_back (DxuiListView::Column { L"Modified", 0,   false, DxuiTextHAlign::Left  });

        for (i = 0; i < kRows; i++)
        {
            //  Long names, as a folder of real files has.
            data.push_back ({ DxuiListView::Cell { std::format (L"Disk2ControllerEventTests{}.cpp", i), false },
                              DxuiListView::Cell { L"CPP",                false },
                              DxuiListView::Cell { L"17.2 KB",            false },
                              DxuiListView::Cell { L"",                   false },
                              DxuiListView::Cell { L"",                   false },
                              DxuiListView::Cell { L"2026-09-10 23:35",   false } });
        }

        list.SetColumns                 (std::move (cols));
        list.SetShowHeader              (true);
        list.SetPreciseAutoFit          (true);
        list.SetHorizontalScrollEnabled (true);
        list.SetMultiSelect             (true);
        list.SetAlwaysShowSelection     (true);
        list.SetKeyboardColumnNav       (true);
        list.SetActivateOnDoubleClick   (true);
        list.SetRows                    (std::move (data));
        list.Layout                     (RECT { 0, 0, 700, 634 }, scaler);
        list.UpdateAutoFitFromRows();
    }



    static int RowMidY (const DxuiListView & list, int visibleIndex)
    {
        //  Header 32 and its 2 gap, then rows of 30, at 96 dpi.
        UNREFERENCED_PARAMETER (list);

        return 34 + visibleIndex * 30 + 15;
    }



    static void Click (DxuiListView & list, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        ev.kind = DxuiMouseEventKind::Down;
        list.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);
    }



    TEST_METHOD (TheBodyHoldsRowsAndTheListCanScroll)
    {
        DxuiListView  list;

        ConfigureLikeTheBrowser (list);

        //  Column widths come from measured text, and the test renderer
        //  measures nothing, so the horizontal bar cannot be raised here. Only
        //  the vertical geometry is worth asserting headlessly.
        Assert::IsTrue (list.GetVisibleRowCapacity() > 0, L"The body holds rows");
        Assert::IsTrue (list.GetMaxTopRow() > 0,          L"and 300 rows can scroll");
    }



    TEST_METHOD (ScrolledClick_SelectsTheRowUnderThePointer)
    {
        DxuiListView  list;
        int           visible = 9;

        ConfigureLikeTheBrowser (list);
        list.SetTopRow (kTopRow);

        Assert::AreEqual (kTopRow, list.GetTopRow(), L"The list is scrolled");

        Click (list, kRowX, RowMidY (list, visible));

        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual (kTopRow + visible, list.GetSelectedRows()[0],
                          L"The tenth row on screen is row 209, not row 9");
        Assert::AreEqual (kTopRow, list.GetTopRow(), L"and the list has not jumped to the top");
    }



    TEST_METHOD (ScrolledClick_ThenAnotherLayout_KeepsBothScrollAndSelection)
    {
        DxuiListView   list;
        DxuiDpiScaler  scaler;
        int            visible = 9;

        scaler.SetDpi (96);

        ConfigureLikeTheBrowser (list);
        list.SetTopRow (kTopRow);
        Click (list, kRowX, RowMidY (list, visible));

        //  Selecting a file re-lays out the browser's panes.
        list.Layout (RECT { 0, 0, 700, 634 }, scaler);

        Assert::AreEqual (kTopRow + visible, list.GetSelectedRows()[0]);
        Assert::AreEqual (kTopRow, list.GetTopRow());
    }
};
