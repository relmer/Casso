#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewFocusScrollTests
//
//  What gaining focus does to a scrolled list.
//
//  With keyboard column navigation on, a list that gains focus with nothing
//  selected seeds the keyboard's row so the focus is visible. Seeding row zero
//  scrolls a list that the user had scrolled elsewhere, and a host that gives
//  the list focus on a press then hit-tests the press against the list it has
//  just moved: the row picked is the one that fell under the pointer after the
//  jump, not the one that was there when the button went down.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewFocusScrollTests)
{
public:

    static constexpr int  kRows   = 300;
    static constexpr int  kTopRow = 200;
    static constexpr int  kRowX   = 100;



    static void ConfigureList (DxuiListView & list)
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

        list.SetColumns           (std::move (cols));
        list.SetShowHeader        (true);
        list.SetMultiSelect       (true);
        list.SetKeyboardColumnNav (true);   // as the file browser does
        list.SetRows              (std::move (data));
        list.Layout               (RECT { 0, 0, 400, 634 }, scaler);
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



    TEST_METHOD (GainingFocus_DoesNotScrollAScrolledList)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetTopRow (kTopRow);

        list.OnFocusChanged (true);

        Assert::AreEqual (kTopRow, list.GetTopRow(), L"Focus does not send a scrolled list back to the top");
    }



    TEST_METHOD (GainingFocus_SeedsTheRowInView)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetTopRow (kTopRow);

        list.OnFocusChanged (true);

        //  Something is selected, so the focus is visible, and it is on screen.
        Assert::IsTrue (list.GetSelectedRow() >= list.GetTopRow(), L"The seeded row is not above the view");
        Assert::IsTrue (list.GetSelectedRow() <  list.GetTopRow() + list.GetVisibleRowCapacity(),
                        L"nor below it");
    }



    TEST_METHOD (GainingFocusOnAnUnscrolledList_StillSeedsTheFirstRow)
    {
        DxuiListView  list;

        ConfigureList (list);

        //  Stated rather than assumed: a list filled while it has no size is
        //  left pinned to its tail, which is a separate defect from this one.
        list.SetTopRow (0);

        list.OnFocusChanged (true);

        Assert::AreEqual (0, list.GetTopRow());
        Assert::AreEqual (0, list.GetSelectedRow(), L"An unscrolled list seeds its first row, as before");
    }



    TEST_METHOD (FocusThenClick_SelectsTheRowUnderThePointer)
    {
        DxuiListView  list;
        int           visible = 9;

        ConfigureList (list);
        list.SetTopRow (kTopRow);

        //  The host gives the list focus on the press, then routes the press.
        list.OnFocusChanged (true);
        Click (list, kRowX, 34 + visible * 30 + 15);

        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual (kTopRow + visible, list.GetSelectedRows()[0],
                          L"The click lands on the row that was under the pointer");
    }



    TEST_METHOD (FocusWithASelectionAlready_LeavesItAlone)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetTopRow (kTopRow);
        list.SetSelectedRows ({ kTopRow + 5 }, kTopRow + 5);

        list.OnFocusChanged (true);

        Assert::AreEqual (kTopRow + 5, list.GetSelectedRow(), L"An existing selection is kept");
        Assert::AreEqual (kTopRow,     list.GetTopRow());
    }
};
