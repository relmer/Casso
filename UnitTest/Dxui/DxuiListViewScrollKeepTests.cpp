#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewScrollKeepTests
//
//  Where a scrolled list is looking, across the layout passes a host runs
//  while the list is untouched.
//
//  The reported defect: scroll a long list down, click a row, and the list
//  jumps back to the top so the click lands on a row near the top instead of
//  the one under the pointer. Selecting a file in the browser re-lays out the
//  panes, and a layout pass must not move the list.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewScrollKeepTests)
{
public:

    static constexpr int  kRows   = 300;
    static constexpr int  kTopRow = 200;



    static void ConfigureList (DxuiListView & list, const RECT & rect)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        int                                           i = 0;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", 200 });

        for (i = 0; i < kRows; i++)
        {
            data.push_back ({ DxuiListView::Cell { std::format (L"row{}", i), false } });
        }

        list.SetColumns    (std::move (cols));
        list.SetShowHeader (true);
        list.SetRows       (std::move (data));
        list.Layout        (rect, scaler);
    }



    static void LayoutTo (DxuiListView & list, const RECT & rect)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        list.Layout (rect, scaler);
    }



    TEST_METHOD (ARepeatedLayoutOfTheSameSize_LeavesTheScrollAlone)
    {
        DxuiListView  list;

        ConfigureList (list, RECT { 0, 0, 400, 634 });
        list.SetTopRow (kTopRow);

        //  What selecting a file does: the host re-lays out its panes.
        LayoutTo (list, RECT { 0, 0, 400, 634 });

        Assert::AreEqual (kTopRow, list.GetTopRow(), L"A layout that changes nothing does not scroll the list");
    }



    TEST_METHOD (ALayoutWhileTheRectIsEmpty_DoesNotThrowAwayTheScroll)
    {
        DxuiListView  list;

        ConfigureList (list, RECT { 0, 0, 400, 634 });
        list.SetTopRow (kTopRow);

        //  A pass before the host knows its size: no height, so no capacity.
        LayoutTo (list, RECT { 0, 0, 0, 0 });
        LayoutTo (list, RECT { 0, 0, 400, 634 });

        Assert::AreEqual (kTopRow, list.GetTopRow(), L"A sizeless pass is not a scroll to the top");
    }



    TEST_METHOD (AShorterListKeepsAsMuchOfTheScrollAsItCan)
    {
        DxuiListView  list;

        ConfigureList (list, RECT { 0, 0, 400, 634 });
        list.SetTopRow (kTopRow);

        //  Genuinely shorter: 300 rows in a 10-row body can only reach 290, so
        //  200 is still reachable and must be kept.
        LayoutTo (list, RECT { 0, 0, 400, 334 });

        Assert::AreEqual (kTopRow, list.GetTopRow(), L"A shorter body that can still show row 200 keeps it");

        //  Back to the taller rect, still where the user left it.
        LayoutTo (list, RECT { 0, 0, 400, 634 });

        Assert::AreEqual (kTopRow, list.GetTopRow());
    }



    TEST_METHOD (AListScrolledToTheEndStaysThere)
    {
        DxuiListView  list;
        int           maxTop = 0;

        ConfigureList (list, RECT { 0, 0, 400, 634 });

        maxTop = list.GetMaxTopRow();
        list.SetTopRow (maxTop);

        //  Sticky tail: a taller body shows more rows, and the end is still
        //  the end.
        LayoutTo (list, RECT { 0, 0, 400, 934 });

        Assert::AreEqual (list.GetMaxTopRow(), list.GetTopRow(), L"The end of the list stays the end");
    }
};
