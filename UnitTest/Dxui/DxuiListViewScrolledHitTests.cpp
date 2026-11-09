#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewScrolledHitTests
//
//  Clicking a row in a SCROLLED list. The reported defect is that picking the
//  tenth row on screen after scrolling down selects the tenth row of the whole
//  list instead, so these state the absolute index a click at a screen
//  position must produce, by hit test and by a real press and release.
//
//  Metrics at 96 dpi, matching the file browser's list: rows 30 tall under a
//  32 header and its 2 gap, so the tenth visible row spans y 304..334 and its
//  middle is 319.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewScrolledHitTests)
{
public:

    static constexpr int  kRows       = 300;
    static constexpr int  kTopRow     = 200;
    static constexpr int  kVisibleIdx = 9;
    static constexpr int  kRowMidY    = 34 + kVisibleIdx * 30 + 15;
    static constexpr int  kRowX       = 100;



    static void ConfigureList (DxuiListView & list)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        int                                           i = 0;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", 200 });
        cols.push_back (DxuiListView::Column { L"Size", 100 });

        for (i = 0; i < kRows; i++)
        {
            data.push_back ({ DxuiListView::Cell { std::format (L"row{}", i), false },
                              DxuiListView::Cell { L"1", false } });
        }

        //  As the browser configures it: a header, multiple selection, and a
        //  body tall enough for twenty rows.
        list.SetColumns     (std::move (cols));
        list.SetShowHeader  (true);
        list.SetMultiSelect (true);
        list.SetRows        (std::move (data));
        list.Layout         (RECT { 0, 0, 400, 634 }, scaler);
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



    TEST_METHOD (TheListHoldsTwentyRowsAndScrolls)
    {
        DxuiListView  list;

        ConfigureList (list);

        //  The premise every other test here rests on.
        Assert::AreEqual (20,  list.GetVisibleRowCapacity(), L"Twenty rows fit the body");
        Assert::AreEqual (280, list.GetMaxTopRow(),          L"and 300 rows can scroll to 280");

        list.SetTopRow (kTopRow);

        Assert::AreEqual (kTopRow, list.GetTopRow());
    }



    TEST_METHOD (HitTestOnAScrolledList_ReportsTheAbsoluteRow)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetTopRow (kTopRow);

        Assert::AreEqual (kTopRow + kVisibleIdx,
                          list.HitTestRow (kRowX, kRowMidY),
                          L"The tenth row on screen is row 209 of the list");
    }



    TEST_METHOD (ClickingAScrolledList_SelectsTheRowUnderThePointer)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetTopRow (kTopRow);

        Click (list, kRowX, kRowMidY);

        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual (kTopRow + kVisibleIdx, list.GetSelectedRows()[0],
                          L"and the click selects that row, not the tenth of the whole list");
        Assert::AreEqual (kTopRow, list.GetTopRow(), L"The list does not jump back to the top");
    }



    TEST_METHOD (ScrollingByWheelThenClicking_SelectsTheRowUnderThePointer)
    {
        DxuiListView    list;
        DxuiMouseEvent  wheel;
        int             expected = 0;

        ConfigureList (list);

        //  Scroll the way a user does, rather than by setting the top row.
        wheel.kind        = DxuiMouseEventKind::Wheel;
        wheel.button      = DxuiMouseButton::None;
        wheel.positionDip = POINT { kRowX, kRowMidY };
        wheel.wheelDelta  = -10.0f;

        list.OnMouse (wheel);

        expected = list.GetTopRow() + kVisibleIdx;

        Assert::IsTrue (list.GetTopRow() > 0, L"The wheel scrolled the list");

        Click (list, kRowX, kRowMidY);

        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual (expected, list.GetSelectedRows()[0]);
    }
};
