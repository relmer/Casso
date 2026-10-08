#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewViewsTests
//
//  Explorer's views other than Details: items laid out in rows, or in
//  columns for List, found under the pointer and moved through with the
//  arrow keys as they lie on screen.
//
//  The list is 600 by 400 pixels at 96 DPI, so each view's cell size from
//  GetItemMetrics gives an exact grid to check against.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewViewsTests)
{
public:

    using View = DxuiListView::View;

    static constexpr int  s_kWidth  = 600;
    static constexpr int  s_kHeight = 400;
    static constexpr int  s_kRows   = 60;
    static constexpr int  s_kBarPx  = 10;



    struct Fixture
    {
        DxuiListView  list;

        explicit Fixture (View view)
        {
            DxuiDpiScaler                                 scaler;
            std::vector<std::vector<DxuiListView::Cell>>  rows;
            int                                           i = 0;

            scaler.SetDpi (96);

            for (i = 0; i < s_kRows; i++)
            {
                rows.push_back ({ DxuiListView::Cell { L"item", false }, DxuiListView::Cell { L"TXT", false } });
            }

            list.SetColumns     ({ DxuiListView::Column { L"Name", 200 }, DxuiListView::Column { L"Type", 100 } });
            list.SetShowHeader  (true);
            list.SetMultiSelect (true);
            list.SetRows        (std::move (rows));
            list.Layout         (RECT { 0, 0, s_kWidth, s_kHeight }, scaler);
            list.SetView        (view);
            list.SetKeyboardColumnNav (true);
            list.OnFocusChanged       (true);
            list.SetSelectedRow       (0);
        }

        void  Key (WPARAM vk)
        {
            DxuiKeyEvent  ev;

            ev.kind = DxuiKeyEventKind::Down;
            ev.vk   = vk;

            list.OnKey (ev);
        }
    };



    //  Items to a row for a view that runs along rows.
    static int  PerRow (View view)
    {
        DxuiListView::ItemMetrics  m = DxuiListView::GetItemMetrics (view);

        return (m.cellWDip > 0) ? (std::max) (1, (s_kWidth - s_kBarPx - Left (view)) / m.cellWDip) : 1;
    }



    //  Where the first item starts: the item views set theirs in.
    static int  Left (View view)
    {
        return (int) DxuiListView::GetItemsLeftDip (view);
    }



    //  How many items share the first row: the run whose tops match the first's.
    static int  CountFirstRow (const DxuiListView & list)
    {
        RECT  first = {};
        RECT  next  = {};
        int   count = 1;

        list.GetItemRectPx (0, first);

        while (count < s_kRows && list.GetItemRectPx (count, next) && next.top == first.top)
        {
            count++;
        }

        return count;
    }



    TEST_METHOD (EachRowView_FindsTheItemUnderThePoint)
    {
        for (View view : { View::ExtraLargeIcons, View::LargeIcons, View::MediumIcons, View::SmallIcons, View::Tiles, View::Content })
        {
            Fixture  f (view);
            int      perRow = CountFirstRow (f.list);
            RECT     first  = {};
            RECT     below  = {};
            RECT     beside = {};

            Assert::IsTrue (f.list.GetItemRectPx (0, first) && f.list.GetItemRectPx (perRow, below));

            Assert::AreEqual (0, f.list.HitTestRow (first.left + 2, first.top + 2), L"The first item is at the top left");
            Assert::AreEqual (perRow, f.list.HitTestRow ((below.left + below.right) / 2, (below.top + below.bottom) / 2), L"and the second row under it");

            if (perRow > 1)
            {
                Assert::IsTrue   (f.list.GetItemRectPx (1, beside));
                Assert::AreEqual (1, f.list.HitTestRow ((beside.left + beside.right) / 2, (beside.top + beside.bottom) / 2), L"and the second beside it");
            }
        }
    }



    TEST_METHOD (List_RunsItemsDownColumns)
    {
        Fixture                    f (View::List);
        DxuiListView::ItemMetrics  m      = DxuiListView::GetItemMetrics (View::List);
        int                        perCol = (s_kHeight - s_kBarPx) / m.cellHDip;

        Assert::AreEqual (1,      f.list.HitTestRow (m.cellWDip / 2, m.cellHDip + m.cellHDip / 2), L"Under the first item is the second");
        Assert::AreEqual (perCol, f.list.HitTestRow (m.cellWDip + m.cellWDip / 2, m.cellHDip / 2),  L"and the next column starts beside it");
    }



    TEST_METHOD (Arrows_MoveThroughTheGridAsItLies)
    {
        Fixture  f (View::MediumIcons);
        int      perRow = PerRow (View::MediumIcons);

        f.Key (VK_RIGHT);
        Assert::AreEqual (1, f.list.GetSelectedRow(), L"Right moves along the row");

        f.Key (VK_DOWN);
        Assert::AreEqual (1 + perRow, f.list.GetSelectedRow(), L"Down moves to the item below");

        f.Key (VK_LEFT);
        f.Key (VK_UP);
        Assert::AreEqual (0, f.list.GetSelectedRow());

        f.Key (VK_UP);
        Assert::AreEqual (0, f.list.GetSelectedRow(), L"Nothing wraps past the top");
    }



    TEST_METHOD (ListArrows_MoveDownAndAcrossColumns)
    {
        Fixture                    f (View::List);
        DxuiListView::ItemMetrics  m      = DxuiListView::GetItemMetrics (View::List);
        int                        perCol = (s_kHeight - s_kBarPx) / m.cellHDip;

        f.Key (VK_DOWN);
        Assert::AreEqual (1, f.list.GetSelectedRow());

        f.Key (VK_RIGHT);
        Assert::AreEqual (1 + perCol, f.list.GetSelectedRow(), L"Right moves to the next column");
    }



    TEST_METHOD (MovingPastTheBottom_ScrollsARowOfItems)
    {
        Fixture                    f (View::LargeIcons);
        DxuiListView::ItemMetrics  m       = DxuiListView::GetItemMetrics (View::LargeIcons);
        int                        perRow  = PerRow (View::LargeIcons);
        int                        visible = s_kHeight / m.cellHDip;
        int                        i       = 0;

        for (i = 0; i < visible; i++)
        {
            f.Key (VK_DOWN);
        }

        //  An item view scrolls by rows of items, which may differ in height,
        //  so its top is a row rather than an item.
        Assert::IsTrue   (perRow > 1);
        Assert::AreEqual (1, f.list.GetTopRow(), L"The top is now the second row of items");
    }



    //  A name that wraps past the one line a cell has room for makes its row
    //  taller once it has been drawn, as Explorer's does, and the next row
    //  moves down to make room.
    TEST_METHOD (MediumIcons_ARowGrowsForALongName)
    {
        Fixture                                       f (View::MediumIcons);
        int                                           perRow = 0;
        RECT                                          first  = {};
        RECT                                          second = {};
        RECT                                          grown  = {};
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        MockDxuiPainter                               painter;
        MockDxuiTextRenderer                          text;
        MockDxuiTheme                                 theme;
        int                                           i      = 0;

        for (i = 0; i < s_kRows; i++)
        {
            rows.push_back ({ DxuiListView::Cell { (i == 0) ? L"a name long enough to wrap to four lines" : L"item", false } });
        }

        f.list.SetRows (std::move (rows));

        perRow = CountFirstRow (f.list);
        Assert::IsTrue   (f.list.GetItemRectPx (0, first) && f.list.GetItemRectPx (perRow, second));
        Assert::AreEqual (perRow, f.list.HitTestRow (second.left + 2, second.top + 2), L"Before it is drawn, the name takes one line");

        f.list.Paint (painter, text, theme);

        Assert::IsTrue   (f.list.GetItemRectPx (perRow, grown));
        Assert::AreEqual (0,      f.list.HitTestRow ((first.left + first.right) / 2, second.top + 2), L"The long name's cell runs on down");
        Assert::IsTrue   (grown.top > second.top, L"and the second row starts lower");
        Assert::AreEqual (perRow, f.list.HitTestRow (grown.left + 2, grown.top + 2));
    }



    TEST_METHOD (Details_ComesBackWithItsHeader)
    {
        Fixture  f (View::Tiles);

        Assert::AreEqual (0, f.list.GetHeaderHeightPx(), L"Only Details has a header");

        f.list.SetView (View::Details);
        Assert::IsTrue (f.list.GetHeaderHeightPx() > 0);
    }

    void  Drag (DxuiListView & list, POINT from, POINT to, bool ctrl = false)
    {
        DxuiMouseEvent  ev;

        ev.button      = DxuiMouseButton::Left;
        ev.ctrl        = ctrl;
        ev.kind        = DxuiMouseEventKind::Down;
        ev.positionDip = from;
        list.OnMouse (ev);

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = to;
        list.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);
    }


    TEST_METHOD (ARubberBand_SelectsEveryItemItTouches)
    {
        Fixture  f (View::MediumIcons);
        int      perRow = CountFirstRow (f.list);
        RECT     first  = {};
        RECT     last   = {};
        RECT     below  = {};

        Assert::IsTrue (f.list.GetItemRectPx (0, first) && f.list.GetItemRectPx (perRow - 1, last) && f.list.GetItemRectPx (perRow, below));

        //  From the empty space right of the first row, down and left across
        //  two rows of items.
        Drag (f.list, POINT { last.right + 2, first.top + 5 }, POINT { first.left + 10, below.top + 5 });

        Assert::AreEqual (2 * perRow, (int) f.list.GetSelectedRows().size());
        Assert::IsTrue   (f.list.IsRowSelected (0) && f.list.IsRowSelected (2 * perRow - 1));
        Assert::IsFalse  (f.list.IsRowSelected (2 * perRow), L"The third row is untouched");
    }


    TEST_METHOD (ACtrlRubberBand_AddsToTheSelection)
    {
        Fixture                    f (View::MediumIcons);
        DxuiListView::ItemMetrics  m      = DxuiListView::GetItemMetrics (View::MediumIcons);
        int                        perRow = PerRow (View::MediumIcons);
        int                        empty  = Left (View::MediumIcons) + perRow * m.cellWDip + 5;
        int                        third  = 2 * perRow;   // first item of the third row

        f.list.SetSelectedRows ({ third }, third);

        Drag (f.list, POINT { empty, 5 }, POINT { Left (View::MediumIcons) + 10, 10 }, true);

        Assert::AreEqual (perRow + 1, (int) f.list.GetSelectedRows().size(), L"The first row, and the item kept");
        Assert::IsTrue   (f.list.IsRowSelected (third));
    }


    TEST_METHOD (CtrlAndShiftClicks_WorkInAnItemView)
    {
        Fixture                    f (View::LargeIcons);
        DxuiListView::ItemMetrics  m = DxuiListView::GetItemMetrics (View::LargeIcons);
        DxuiMouseEvent             ev;

        ev.button = DxuiMouseButton::Left;

        auto  click = [&] (int item, bool ctrl, bool shift)
        {
            RECT  cell = {};

            f.list.GetItemRectPx (item, cell);

            ev.ctrl        = ctrl;
            ev.shift       = shift;
            ev.positionDip = POINT { (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 };
            ev.kind        = DxuiMouseEventKind::Down;
            f.list.OnMouse (ev);
            ev.kind        = DxuiMouseEventKind::Up;
            f.list.OnMouse (ev);
        };

        (void) m;

        click (1, false, false);
        click (3, true,  false);
        Assert::AreEqual (2, (int) f.list.GetSelectedRows().size(), L"Ctrl adds an item apart from the first");

        click (6, false, true);
        Assert::AreEqual (4, (int) f.list.GetSelectedRows().size(), L"Shift selects the run from the item last clicked, 3 to 6, as Explorer does");
    }
};
