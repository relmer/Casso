#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewMultiSelectTests
//
//  Multiple selection on DxuiListView: a plain click replaces the selection,
//  Ctrl toggles one row, Shift extends from the anchor by click or arrow, and
//  Ctrl+A takes every row. Also the sort and activate callbacks a consumer of
//  a multi-select list relies on. A single-select list keeps its one row in
//  the selected set, so a consumer can always read GetSelectedRows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewMultiSelectTests)
{
public:

    static void  ConfigureList (DxuiListView & list, int rows)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        int                                           i = 0;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", 80 });
        cols.push_back (DxuiListView::Column { L"Size", 60 });

        for (i = 0; i < rows; i++)
        {
            data.push_back ({ DxuiListView::Cell { L"row", false }, DxuiListView::Cell { L"1", false } });
        }

        list.SetColumns       (std::move (cols));
        list.SetRows          (std::move (data));
        list.SetMultiSelect   (true);
        list.SetKeyboardColumnNav (true);
        list.Layout           (RECT { 0, 0, 400, 300 }, scaler);
    }


    static DxuiKeyEvent  MakeKey (WPARAM vk, bool shift = false, bool ctrl = false)
    {
        DxuiKeyEvent  ev;

        ev.kind  = DxuiKeyEventKind::Down;
        ev.vk    = vk;
        ev.shift = shift;
        ev.ctrl  = ctrl;
        return ev;
    }


    static std::vector<int>  Rows (std::initializer_list<int> rows)
    {
        return std::vector<int> (rows);
    }


    TEST_METHOD (PlainClick_ReplacesSelection)
    {
        DxuiListView  list;

        ConfigureList (list, 10);

        list.ClickRow (2, false, false);
        list.ClickRow (5, true,  false);
        list.ClickRow (7, false, false);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 7 }));
        Assert::AreEqual (7, list.GetAnchorRow());
    }


    TEST_METHOD (CtrlClick_TogglesOneRow)
    {
        DxuiListView  list;

        ConfigureList (list, 10);

        list.ClickRow (2, false, false);
        list.ClickRow (5, true,  false);
        list.ClickRow (8, true,  false);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 2, 5, 8 }));

        list.ClickRow (5, true, false);

        Assert::IsTrue  (list.GetSelectedRows() == Rows ({ 2, 8 }));
        Assert::IsFalse (list.IsRowSelected (5));
    }


    TEST_METHOD (ShiftClick_ExtendsFromAnchorInEitherDirection)
    {
        DxuiListView  list;

        ConfigureList (list, 10);

        list.ClickRow (4, false, false);
        list.ClickRow (7, false, true);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 4, 5, 6, 7 }));

        list.ClickRow (2, false, true);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 2, 3, 4 }));
        Assert::AreEqual (4, list.GetAnchorRow());
        Assert::AreEqual (2, list.GetSelectedRow());
    }


    TEST_METHOD (ShiftArrow_ExtendsFromAnchor)
    {
        DxuiListView  list;

        ConfigureList (list, 10);

        list.ClickRow (3, false, false);

        Assert::IsTrue (list.OnKey (MakeKey (VK_TAB)));
        Assert::IsTrue (list.OnKey (MakeKey (VK_DOWN, true)));
        Assert::IsTrue (list.OnKey (MakeKey (VK_DOWN, true)));

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 3, 4, 5 }));

        Assert::IsTrue (list.OnKey (MakeKey (VK_DOWN)));

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 6 }));
    }


    TEST_METHOD (CtrlA_SelectsEveryRow)
    {
        DxuiListView  list;

        ConfigureList (list, 4);

        list.ClickRow (1, false, false);

        Assert::IsTrue (list.OnKey (MakeKey (VK_TAB)));
        Assert::IsTrue (list.OnKey (MakeKey ('A', false, true)));

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 0, 1, 2, 3 }));
    }


    TEST_METHOD (SetSelectedRows_SortsAndDropsOutOfRange)
    {
        DxuiListView  list;

        ConfigureList (list, 5);

        list.SetSelectedRows ({ 4, 1, 9, 1, -2 }, 1);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 1, 4 }));
        Assert::AreEqual (1, list.GetAnchorRow());
    }


    TEST_METHOD (ShrinkingRows_PrunesSelection)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  data;

        ConfigureList (list, 10);

        list.SetSelectedRows ({ 1, 6, 9 }, 1);

        data.assign (5, { DxuiListView::Cell { L"row", false }, DxuiListView::Cell { L"1", false } });
        list.SetRows (std::move (data));

        for (int row : list.GetSelectedRows())
        {
            Assert::IsTrue (row < 5);
        }
    }


    TEST_METHOD (SingleSelect_KeepsOneRowInTheSet)
    {
        DxuiListView  list;

        ConfigureList (list, 10);
        list.SetMultiSelect (false);

        list.ClickRow (2, false, false);
        list.ClickRow (6, true,  true);

        Assert::IsTrue (list.GetSelectedRows() == Rows ({ 6 }));
    }


    TEST_METHOD (Enter_ReportsActivateForTheKeyboardRow)
    {
        DxuiListView  list;
        int           activated = -1;

        ConfigureList (list, 10);
        list.SetOnActivateRow ([&activated] (int row) { activated = row; });

        list.ClickRow (3, false, false);

        Assert::IsTrue (list.OnKey (MakeKey (VK_TAB)));
        Assert::IsTrue (list.OnKey (MakeKey (VK_RETURN)));

        Assert::AreEqual (3, activated);
    }


    TEST_METHOD (HeaderEnter_ReportsSortColumnWithoutReorderingRows)
    {
        DxuiListView  list;
        int           sorted = -1;

        ConfigureList (list, 3);
        list.SetOnSortColumn ([&sorted] (int column) { sorted = column; });

        list.ClickRow (2, false, false);

        Assert::IsTrue (list.OnKey (MakeKey (VK_TAB)));
        Assert::IsTrue (list.OnKey (MakeKey (VK_TAB)));
        Assert::IsTrue (list.OnKey (MakeKey (VK_RETURN)));

        Assert::AreEqual (0, sorted);
        Assert::AreEqual (2, list.GetSelectedRow());
    }


    TEST_METHOD (CtrlClick_ReportsSelectionChanged)
    {
        DxuiListView  list;
        int           reported = -1;
        int           calls    = 0;

        ConfigureList (list, 10);
        list.SetOnSelectionChanged ([&] (int row) { reported = row; calls++; });

        list.ClickRow (4, true, false);

        Assert::AreEqual (4, reported);
        Assert::AreEqual (1, calls);
    }
};
