#include "Pch.h"

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

        return (m.cellWDip > 0) ? (std::max) (1, (s_kWidth - s_kBarPx) / m.cellWDip) : 1;
    }



    TEST_METHOD (EachRowView_FindsTheItemUnderThePoint)
    {
        for (View view : { View::ExtraLargeIcons, View::LargeIcons, View::MediumIcons, View::SmallIcons, View::Tiles, View::Content })
        {
            Fixture                    f (view);
            DxuiListView::ItemMetrics  m      = DxuiListView::GetItemMetrics (view);
            int                        perRow = PerRow (view);
            int                        cellW  = (m.cellWDip > 0) ? m.cellWDip : s_kWidth - s_kBarPx;

            Assert::AreEqual (0, f.list.HitTestRow (2, 2), L"The first item is at the top left");

            //  The middle of the second row's first cell.
            Assert::AreEqual (perRow, f.list.HitTestRow (cellW / 2, m.cellHDip + m.cellHDip / 2));

            if (perRow > 1)
            {
                Assert::AreEqual (1, f.list.HitTestRow (cellW + cellW / 2, m.cellHDip / 2), L"and the second beside it");
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

        Assert::AreEqual (perRow, f.list.GetTopRow(), L"The top is now the second row of items");
        Assert::AreEqual (0, f.list.GetTopRow() % perRow);
    }



    TEST_METHOD (Details_ComesBackWithItsHeader)
    {
        Fixture  f (View::Tiles);

        Assert::AreEqual (0, f.list.GetHeaderHeightPx(), L"Only Details has a header");

        f.list.SetView (View::Details);
        Assert::IsTrue (f.list.GetHeaderHeightPx() > 0);
    }
};
