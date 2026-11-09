#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewColumnOrderTests
//
//  Headers dragged into a new order, as Explorer's are: a drag moves the
//  pressed column to where it is dropped, a press released where it started
//  sorts instead, and a stored order that does not name every column once is
//  ignored. Three 100 px columns at 96 dpi, so every x is plain arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewColumnOrderTests)
{
public:

    static constexpr int  s_kColumnPx = 100;
    static constexpr int  s_kHeaderY  = 10;

    static void  ConfigureList (DxuiListView & list)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"A", s_kColumnPx });
        cols.push_back (DxuiListView::Column { L"B", s_kColumnPx });
        cols.push_back (DxuiListView::Column { L"C", s_kColumnPx });

        data.push_back ({ DxuiListView::Cell { L"a" }, DxuiListView::Cell { L"b" }, DxuiListView::Cell { L"c" } });

        list.SetColumns    (std::move (cols));
        list.SetRows       (std::move (data));
        list.SetShowHeader (true);
        list.Layout        (RECT { 0, 0, 600, 300 }, scaler);
    }

    static DxuiMouseEvent  MakeMouse (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        return ev;
    }


    TEST_METHOD (DragPastAnotherHeader_MovesTheColumn)
    {
        DxuiListView         list;
        std::vector<size_t>  reported;

        ConfigureList (list);
        list.SetOnColumnsReordered ([&reported] (const std::vector<size_t> & order) { reported = order; });

        //  A pressed, carried past the middle of C, dropped.
        list.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 50,  s_kHeaderY));
        list.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 150, s_kHeaderY));
        list.OnMouse (MakeMouse (DxuiMouseEventKind::Move, 280, s_kHeaderY));
        list.OnMouse (MakeMouse (DxuiMouseEventKind::Up,   280, s_kHeaderY));

        Assert::AreEqual ((size_t) 3, list.GetColumnOrder().size());
        Assert::AreEqual ((size_t) 1, list.GetColumnOrder()[0], L"B first");
        Assert::AreEqual ((size_t) 2, list.GetColumnOrder()[1], L"then C");
        Assert::AreEqual ((size_t) 0, list.GetColumnOrder()[2], L"then A, where it was dropped");
        Assert::IsTrue   (reported == list.GetColumnOrder(), L"and the new order is reported");
    }


    TEST_METHOD (ReleaseWhereItStarted_SortsInsteadOfMoving)
    {
        DxuiListView  list;
        int           sorted = -1;

        ConfigureList (list);
        list.SetOnSortColumn ([&sorted] (int column) { sorted = column; });

        list.OnMouse (MakeMouse (DxuiMouseEventKind::Down, 150, s_kHeaderY));
        Assert::AreEqual (-1, sorted, L"nothing sorts on the press");

        list.OnMouse (MakeMouse (DxuiMouseEventKind::Up, 152, s_kHeaderY));

        Assert::AreEqual (1, sorted, L"the release sorts by B");
        Assert::AreEqual ((size_t) 0, list.GetColumnOrder()[0], L"and the order is unchanged");
    }


    TEST_METHOD (StoredOrder_IgnoredUnlessItNamesEveryColumnOnce)
    {
        DxuiListView  list;

        ConfigureList (list);

        list.SetColumnOrder ({ 2, 2, 0 });
        Assert::AreEqual ((size_t) 0, list.GetColumnOrder()[0], L"a repeated column is ignored");

        list.SetColumnOrder ({ 2, 0 });
        Assert::AreEqual ((size_t) 0, list.GetColumnOrder()[0], L"so is a short one");

        list.SetColumnOrder ({ 2, 0, 1 });
        Assert::AreEqual ((size_t) 2, list.GetColumnOrder()[0], L"a whole one is taken");
    }
};
