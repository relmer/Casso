#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewPagingTests
//
//  Page Up and Page Down as File Explorer's: from short of the view's edge,
//  to the whole line at that edge without scrolling; from the edge, a page
//  on, the line landed on then flush with the edge. A page is the lines the
//  view holds whole, less one, and a group's header is not one of them. Item
//  views keep the focus's place along its line, and List pages sideways.
//
//  Geometry: 96 DPI throughout. Details is 400 px wide with its header
//  hidden and 30 px rows, so 300 px holds ten whole rows and a page is nine;
//  310 px shows 10 px of an eleventh. Medium icons in 600x400 px lay out
//  seven items to a line, the lines 71 px apart below a 6 px margin, so five
//  lines fit and a page is four. List in 600x400 px runs eleven items down
//  each 240 px column, the first column 14 px in and each box 11 px short of
//  the next column.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewPagingTests)
{
public:

    using View = DxuiListView::View;



    //  A 200 px column of rows "r0", "r1" and on, in Details, with the
    //  keyboard on the body.
    static void  ConfigureList (DxuiListView & list, int rows, int widthPx, int heightPx)
    {
        DxuiDpiScaler                                  scaler;
        std::vector<std::vector<DxuiListView::Cell>>   cells;

        scaler.SetDpi (96);

        list.SetColumns     ({ DxuiListView::Column { L"Name", 200 } });
        list.SetShowHeader  (false);
        list.SetMultiSelect (true);
        list.Layout         (RECT { 0, 0, widthPx, heightPx }, scaler);

        for (int i = 0; i < rows; i++)
        {
            cells.push_back ({ DxuiListView::Cell { L"r" + std::to_wstring (i), false } });
        }

        list.SetRows              (std::move (cells));
        list.SetKeyboardColumnNav (true);
        list.OnFocusChanged       (true);
    }



    static void  ConfigureDetails (DxuiListView & list, int rows, int heightPx)
    {
        ConfigureList (list, rows, 400, heightPx);
    }



    static void  ConfigureItems (DxuiListView & list, View view, int rows, int widthPx, int heightPx)
    {
        ConfigureList (list, rows, widthPx, heightPx);
        list.SetView  (view);
    }



    static DxuiKeyEvent  Key (WPARAM vk, bool shift = false, bool ctrl = false)
    {
        DxuiKeyEvent  ev;

        ev.kind  = DxuiKeyEventKind::Down;
        ev.vk    = vk;
        ev.shift = shift;
        ev.ctrl  = ctrl;
        return ev;
    }



    //  How many items share the first line: the run whose tops match the first's.
    static int  CountFirstLine (const DxuiListView & list)
    {
        RECT  first = {};
        RECT  next  = {};
        int   count = 1;

        list.GetItemRectPx (0, first);

        while (count < list.GetRowCount() && list.GetItemRectPx (count, next) && next.top == first.top)
        {
            count++;
        }

        return count;
    }



    static RECT  GetRect (const DxuiListView & list, int item)
    {
        RECT  cell = {};

        Assert::IsTrue (list.GetItemRectPx (item, cell));
        return cell;
    }



    //  The y at which a string was last drawn, or -1000 when it was not.
    static float  GetDrawnY (const MockDxuiTextRenderer & text, const std::wstring & s)
    {
        float  y = -1000.0f;

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text == s)
            {
                y = call.y;
            }
        }

        return y;
    }



    //  The y at which a line of text starting with a string was last drawn,
    //  or -1000 when none was.
    static float  GetDrawnYStarting (const MockDxuiTextRenderer & text, const std::wstring & start)
    {
        float  y = -1000.0f;

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString && call.text.starts_with (start))
            {
                y = call.y;
            }
        }

        return y;
    }



    //  Names long enough to wrap to four lines in Medium icons, each led by a
    //  word of its own: "n0", "n1" and on.
    static void  SetLongNames (DxuiListView & list, int rows)
    {
        std::vector<std::vector<DxuiListView::Cell>>  cells;

        for (int i = 0; i < rows; i++)
        {
            cells.push_back ({ DxuiListView::Cell { L"n" + std::to_wstring (i) + L" a name long enough to wrap to four lines", false } });
        }

        list.SetRows (std::move (cells));
    }



    //  Rows 4 and 9 are inside the view and on its bottom edge; row 18, a page
    //  on from 9, lands flush with the bottom, leaving 9 the top row.
    TEST_METHOD (Details_PageDownGoesToTheBottomRowThenAPageOn)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (4);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (9, list.GetSelectedRow(), L"To the bottom row");
        Assert::AreEqual (0, list.GetTopRow(),      L"without scrolling");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (18, list.GetSelectedRow(), L"A page on from the bottom row");
        Assert::AreEqual ( 9, list.GetTopRow(),      L"flush with the bottom, the old bottom row now the top one");
        Assert::IsTrue   (list.IsRowSelected (18));
        Assert::IsFalse  (list.IsRowSelected (9));
    }



    //  Alternating, the keys go between the top and bottom rows and the view
    //  stays where it is; from the top row, Page Up goes a page back.
    TEST_METHOD (Details_AlternatingKeysStayOnTheEdgeRows)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (4);
        list.OnKey (Key (VK_NEXT));
        list.OnKey (Key (VK_NEXT));

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (9, list.GetSelectedRow(), L"To the top row");
        Assert::AreEqual (9, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (18, list.GetSelectedRow(), L"Back to the bottom row");
        Assert::AreEqual ( 9, list.GetTopRow());

        list.OnKey (Key (VK_PRIOR));
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow(), L"A page back from the top row");
        Assert::AreEqual (0, list.GetTopRow());
    }



    //  310 px shows ten whole rows and 10 px of another. A page down lifts the
    //  rows 20 px so row 18 ends flush with the bottom, row 8 cut off at the top.
    TEST_METHOD (Details_APartRowIsCutAtTheTopAfterAPageDown)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 310);
        list.SetSelectedRow (4);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (9, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (18, list.GetSelectedRow());
        Assert::AreEqual ( 8, list.GetTopRow());
        Assert::AreEqual (18, list.HitTestRow (10, 305), L"Row 18 at the bottom");
        Assert::AreEqual ( 9, list.HitTestRow (10, 15),  L"row 9 whole under");
        Assert::AreEqual ( 8, list.HitTestRow (10, 5),   L"the 10 px left of row 8");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual ( 9, list.GetSelectedRow(),    L"The top whole row");
        Assert::AreEqual (18, list.HitTestRow (10, 305), L"with nothing scrolled");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetTopRow());
    }



    //  After a page down lifts the rows, a click on the bottom row leaves them
    //  as they are, and a click on the row cut off at the top brings it in
    //  whole: 0 to 30 px, where row 9 had 25 px while they were lifted.
    TEST_METHOD (Details_ClicksAfterAPageDown)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 310);
        list.SetSelectedRow (9);
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (8, list.GetTopRow());
        Assert::AreEqual (9, list.HitTestRow (10, 25), L"Lifted, row 9 starts 10 px down");

        list.ClickRow (18, false, false);
        Assert::AreEqual ( 8, list.GetTopRow(),          L"The bottom row was whole already");
        Assert::AreEqual (18, list.HitTestRow (10, 305), L"and stays flush with the bottom");

        list.ClickRow (8, false, false);
        Assert::AreEqual (8, list.GetTopRow());
        Assert::AreEqual (8, list.HitTestRow (10, 5),  L"Row 8 whole at the top");
        Assert::AreEqual (8, list.HitTestRow (10, 25), L"no longer lifted");
        Assert::AreEqual (9, list.HitTestRow (10, 35));
    }



    //  End lifts the rows so the last one shows whole. Up onto the row cut off
    //  at the top brings it in whole, as it does after a page down.
    TEST_METHOD (Details_UpOntoTheRowCutOffAtTheEnd)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 310);
        list.SetSelectedRow (0);

        list.OnKey (Key (VK_END));
        Assert::AreEqual (39, list.GetSelectedRow());
        Assert::AreEqual (29, list.GetTopRow());
        Assert::AreEqual (39, list.HitTestRow (10, 305), L"The last row flush with the bottom");
        Assert::AreEqual (30, list.HitTestRow (10, 25),  L"row 29 cut off above row 30");

        list.SetSelectedRow (30);
        list.OnKey (Key (VK_UP));
        Assert::AreEqual (29, list.GetSelectedRow());
        Assert::AreEqual (29, list.GetTopRow());
        Assert::AreEqual (29, list.HitTestRow (10, 25), L"Row 29 whole at the top");

        list.OnKey (Key (VK_END));
        Assert::AreEqual (39, list.HitTestRow (10, 305), L"End shows the last row whole again");
    }



    //  Eleven rows in 310 px: ten whole and 10 px of the last. The list opens
    //  with row 0 whole; End lifts the rows 20 px for row 10, and Home or a
    //  page back to row 0 shows it whole again.
    TEST_METHOD (Details_OneRowOverTheViewStillShowsTheFirstWhole)
    {
        DxuiListView  list;

        ConfigureDetails (list, 11, 310);
        Assert::AreEqual (0, list.HitTestRow (10, 25), L"Row 0 whole when the list opens");

        list.SetSelectedRow (0);
        list.OnKey (Key (VK_END));
        Assert::AreEqual (10, list.GetSelectedRow());
        Assert::AreEqual (10, list.HitTestRow (10, 305), L"The last row flush with the bottom");
        Assert::AreEqual ( 1, list.HitTestRow (10, 25),  L"row 0 cut off");

        list.OnKey (Key (VK_HOME));
        Assert::AreEqual (0, list.GetSelectedRow());
        Assert::AreEqual (0, list.HitTestRow (10, 25), L"Home shows row 0 whole");

        list.OnKey (Key (VK_END));
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (1, list.GetSelectedRow(), L"The top whole row");
        Assert::AreEqual (1, list.HitTestRow (10, 25), L"with nothing scrolled");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow());
        Assert::AreEqual (0, list.HitTestRow (10, 25), L"A page back shows row 0 whole");
    }



    //  The last row pages nowhere and the first row back nowhere; neither
    //  scrolls past the end.
    TEST_METHOD (Details_ClampsAtBothEnds)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (35);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (39, list.GetSelectedRow(), L"Clamped at the last row");
        Assert::AreEqual (30, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (39, list.GetSelectedRow());
        Assert::AreEqual (30, list.GetTopRow());

        list.SetSelectedRow (0);
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow(), L"Nowhere back from the first row");
        Assert::AreEqual (0, list.GetTopRow());
    }



    //  Out of sight below, Page Up goes to the top row and Page Down pages on
    //  from the focus itself; with no focus at all, either goes to its edge.
    TEST_METHOD (Details_FocusOutOfSightOrNone)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (25);
        list.SetTopRow (0);

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow(), L"To the top row in sight");
        Assert::AreEqual (0, list.GetTopRow());

        list.SetSelectedRow (25);
        list.SetTopRow (0);
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (34, list.GetSelectedRow(), L"A page on from row 25");
        Assert::AreEqual (25, list.GetTopRow());

        list.ClearSelection();
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (34, list.GetSelectedRow(), L"With no focus, to the bottom row");
        Assert::AreEqual (25, list.GetTopRow());
    }



    TEST_METHOD (Details_ShiftExtendsAndCtrlMovesOnlyTheFocus)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (4);

        list.OnKey (Key (VK_NEXT, true));
        Assert::AreEqual ((size_t) 6, list.GetSelectedRows().size(), L"Shift selects rows 4 to 9");
        Assert::IsTrue   (list.IsRowSelected (4) && list.IsRowSelected (9));
        Assert::AreEqual (9, list.GetSelectedRow());
        Assert::AreEqual (4, list.GetAnchorRow());

        list.SetSelectedRow (4);
        list.OnKey (Key (VK_NEXT, false, true));
        Assert::AreEqual (9, list.GetSelectedRow(), L"Ctrl moves the focus");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::IsTrue   (list.IsRowSelected (4),  L"and leaves the selection");

        list.OnKey (Key (VK_NEXT, false, true));
        Assert::AreEqual (18, list.GetSelectedRow(), L"Ctrl pages and scrolls as well");
        Assert::AreEqual ( 9, list.GetTopRow());
        Assert::IsTrue   (list.IsRowSelected (4));
    }



    //  Rows 0-14 in A and 15-29 in B make lines A, 0-14, B, 15-29. From row 8
    //  at the bottom, nine rows on is row 17, B's header not counted.
    TEST_METHOD (GroupedDetails_HeadersAreNotCounted)
    {
        DxuiListView  list;

        ConfigureDetails (list, 30, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });
        list.SetSelectedRow (0);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (8, list.GetSelectedRow(), L"To the bottom row, on line 9");
        Assert::AreEqual (0, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (17, list.GetSelectedRow(), L"Nine rows on, past B's header");
        Assert::AreEqual (10, list.GetTopRow(),      L"its line, 19, flush with the bottom");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual ( 9, list.GetSelectedRow(), L"To the top row");
        Assert::AreEqual (10, list.GetTopRow());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (0, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetTopRow(), L"The first row goes to the very top, A's header with it");
        Assert::AreEqual (-1, list.GetFocusedGroup());
    }



    //  Home and End go to the first and last rows, not to the headers.
    TEST_METHOD (GroupedDetails_HomeAndEndSelectRows)
    {
        DxuiListView  list;

        ConfigureDetails (list, 30, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });
        list.SetSelectedRow (10);

        list.OnKey (Key (VK_END));
        Assert::AreEqual (29, list.GetSelectedRow());
        Assert::AreEqual (-1, list.GetFocusedGroup());
        Assert::AreEqual (list.GetMaxTopRow(), list.GetTopRow());

        list.OnKey (Key (VK_HOME));
        Assert::AreEqual ( 0, list.GetSelectedRow());
        Assert::AreEqual (-1, list.GetFocusedGroup(), L"Not A's header");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual ( 0, list.GetTopRow());
    }



    //  A focused header pages from its own line: B's, on line 16, is inside the
    //  view, so either key goes to the row at its edge.
    TEST_METHOD (GroupedDetails_AFocusedHeaderPagesFromItsLine)
    {
        DxuiListView  list;

        ConfigureDetails (list, 30, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });
        list.SelectGroup (1);
        Assert::AreEqual (1, list.GetFocusedGroup());
        Assert::AreEqual (8, list.GetTopRow(), L"Lines 8 to 17 in sight");

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (-1, list.GetFocusedGroup());
        Assert::AreEqual (15, list.GetSelectedRow(), L"The bottom row");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::AreEqual ( 8, list.GetTopRow());

        list.SelectGroup (1);
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (7, list.GetSelectedRow(), L"The top row");
        Assert::AreEqual (8, list.GetTopRow());
    }



    //  Lines 17 to 26 in sight: rows 15 to 24, B's header on line 16 just out
    //  of sight above. Page Up goes to row 15, the top row, and nothing
    //  scrolls, although selecting a group's first row alone brings its header
    //  into sight.
    TEST_METHOD (GroupedDetails_TheTopRowOfAGroupLeavesItsHeaderOutOfSight)
    {
        DxuiListView  list;

        ConfigureDetails (list, 30, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });
        list.SetTopRow (17);
        list.SetSelectedRow (20);
        Assert::AreEqual (17, list.GetTopRow());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (15, list.GetSelectedRow(), L"The top row");
        Assert::AreEqual (17, list.GetTopRow(),      L"nothing scrolled");
        Assert::AreEqual (-1, list.HitTestGroupHeader (10, 5), L"B's header still out of sight");

        list.SetSelectedRow (20);
        list.SetSelectedRow (15);
        Assert::AreEqual (16, list.GetTopRow(), L"Selected alone, the row brings its header with it");
    }



    //  From item 16, on the third line, Page Down keeps the third place along:
    //  item 30 on the fifth line, then 58, four lines on, flush with the
    //  bottom, which leaves the fifth line the top whole one and the fourth
    //  cut off. From item 44, between them, Page Up goes to the top line.
    TEST_METHOD (MediumIcons_PagesKeepThePlaceAlongTheLine)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 200, 600, 400);
        list.SetSelectedRow (16);
        Assert::AreEqual (7, CountFirstLine (list));

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (30, list.GetSelectedRow(), L"The bottom line");
        Assert::AreEqual ( 0, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (58,  list.GetSelectedRow());
        Assert::AreEqual (400, (int) GetRect (list, 58).bottom, L"Flush with the bottom");
        Assert::AreEqual (45,  (int) GetRect (list, 30).top,    L"the old bottom line the top whole one");
        Assert::IsTrue   (GetRect (list, 23).top < 0,           L"and the line above it cut off");

        list.SetSelectedRow (44);
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (30, list.GetSelectedRow(), L"The top line");
        Assert::AreEqual (45, (int) GetRect (list, 30).top, L"Nothing scrolled");

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (58, list.GetSelectedRow(), L"Back to the bottom line");
        Assert::AreEqual (45, (int) GetRect (list, 30).top);

        list.OnKey (Key (VK_PRIOR));
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (2, list.GetSelectedRow(), L"A page back from the top line");
        Assert::AreEqual (0, list.GetTopRow());
        Assert::AreEqual (6, (int) GetRect (list, 2).top, L"At the very top, the margin showing");
    }



    //  With the view scrolled away below the focus, Page Down goes to the
    //  bottom line in sight without scrolling: lines 10 to 14.
    TEST_METHOD (MediumIcons_FocusAboveTheViewGoesToTheBottomLine)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 200, 600, 400);
        list.SetSelectedRow (2);
        list.SetTopRow (10);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (100, list.GetSelectedRow(), L"Line 14, third along");
        Assert::AreEqual ( 10, list.GetTopRow());
    }



    //  The bottom line holds five items; from the seventh place, Page Down takes
    //  the last of them.
    TEST_METHOD (MediumIcons_AShortLineTakesItsLastItem)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 33, 600, 400);
        list.SetSelectedRow (6);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (32, list.GetSelectedRow());
    }



    TEST_METHOD (MediumIcons_ShiftExtendsAndCtrlMovesOnlyTheFocus)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 200, 600, 400);
        list.SetSelectedRow (16);

        list.OnKey (Key (VK_NEXT, true));
        Assert::AreEqual ((size_t) 15, list.GetSelectedRows().size(), L"Shift selects items 16 to 30");
        Assert::AreEqual (30, list.GetSelectedRow());
        Assert::AreEqual (16, list.GetAnchorRow());

        list.SetSelectedRow (16);
        list.OnKey (Key (VK_NEXT, false, true));
        Assert::AreEqual (30, list.GetSelectedRow(), L"Ctrl moves the focus");
        Assert::AreEqual ((size_t) 1, list.GetSelectedRows().size());
        Assert::IsTrue   (list.IsRowSelected (16));
    }



    //  Every view that scrolls down pages the same way, each by its own line
    //  height: from the second line to the bottom line in sight, then a page
    //  on flush with the bottom, then back to the top line, and a page back
    //  to the first line.
    TEST_METHOD (EveryRowView_PagesTheSameWay)
    {
        for (View view : { View::LargeIcons, View::MediumIcons, View::SmallIcons, View::Tiles, View::Content })
        {
            DxuiListView  list;
            int           perLine = 0;
            int           start   = 0;
            int           lineH   = 0;
            int           page    = 0;
            int           edge    = 0;
            int           landed  = 0;

            ConfigureItems (list, view, 60, 600, 400);

            perLine = CountFirstLine (list);
            start   = perLine + (std::min) (1, perLine - 1);
            lineH   = (int) (GetRect (list, 0).bottom - GetRect (list, 0).top);
            page    = (std::max) (1, 400 / lineH - 1);

            list.SetSelectedRow (start);

            list.OnKey (Key (VK_NEXT));
            edge = list.GetSelectedRow();
            Assert::AreEqual (start % perLine, edge % perLine,  L"The same place along the line");
            Assert::AreEqual (0,               list.GetTopRow(), L"nothing scrolled");
            Assert::IsTrue   (GetRect (list, edge).bottom <= 400 && GetRect (list, edge + perLine).bottom > 400, L"the bottom whole line");

            list.OnKey (Key (VK_NEXT));
            landed = list.GetSelectedRow();
            Assert::AreEqual (start % perLine, landed % perLine);
            Assert::AreEqual (page,            landed / perLine - edge / perLine, L"A page on");
            Assert::AreEqual (400,             (int) GetRect (list, landed).bottom, L"flush with the bottom");
            Assert::IsTrue   (GetRect (list, edge).top >= 0 && GetRect (list, edge - perLine).top < 0, L"the old bottom line now the top whole one");

            list.OnKey (Key (VK_PRIOR));
            Assert::AreEqual (edge, list.GetSelectedRow());

            list.OnKey (Key (VK_PRIOR));
            Assert::AreEqual (start - perLine, list.GetSelectedRow(), L"A page back is the first line");
            Assert::AreEqual (0,               list.GetTopRow());
        }
    }



    //  Extra large's 279 px lines fit one at a time, so each key goes a line.
    TEST_METHOD (ExtraLargeIcons_PagesALineAtATime)
    {
        DxuiListView  list;

        ConfigureItems (list, View::ExtraLargeIcons, 60, 600, 400);
        list.SetSelectedRow (1);
        Assert::AreEqual (2, CountFirstLine (list));

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (3,   list.GetSelectedRow());
        Assert::AreEqual (400, (int) GetRect (list, 3).bottom);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (5,   list.GetSelectedRow());
        Assert::AreEqual (400, (int) GetRect (list, 5).bottom);
    }



    //  Medium icons 300 px tall: A's 20 items on lines 1-3 under its header,
    //  B's 40 on lines 5-10 under its own. Four lines fit, so a page is three,
    //  and from A's last line it reaches B's third, the header not counted.
    TEST_METHOD (GroupedItems_HeadersAreNotCounted)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 60, 600, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 20 } });
        list.SetSelectedRow (1);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (15, list.GetSelectedRow(), L"A's last line");
        Assert::AreEqual ( 0, list.GetTopRow());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (35,  list.GetSelectedRow(), L"B's third line");
        Assert::AreEqual (300, (int) GetRect (list, 35).bottom);
        Assert::IsTrue   (GetRect (list, 15).top < 0, L"A's last line partly out of sight, the header having taken its room");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (21,  list.GetSelectedRow(), L"B's first line, the top whole line of items");
        Assert::AreEqual (300, (int) GetRect (list, 35).bottom, L"nothing scrolled");

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (1, list.GetSelectedRow(), L"Three lines back, the header not counted");
        Assert::AreEqual (0, list.GetTopRow());
    }



    //  Only a key that scrolls starts a slide, and a key that does not scroll
    //  leaves one under way to end when it would have.
    TEST_METHOD (PageSlide_StartsOnlyWhenTheViewScrolls)
    {
        DxuiListView  list;
        int64_t       now = 1000;

        ConfigureItems (list, View::MediumIcons, 200, 600, 400);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (2);

        list.OnKey (Key (VK_NEXT));
        Assert::IsFalse (list.IsGroupSliding(), L"To the bottom line, nothing scrolled");

        list.OnKey (Key (VK_NEXT));
        Assert::IsTrue  (list.IsGroupSliding(), L"A page on scrolls and slides");

        now += 50;
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (30, list.GetSelectedRow());

        now = 1000 + 142;
        Assert::IsTrue  (list.IsGroupSliding(), L"Once more, for the frame where it rests");
        Assert::IsFalse (list.IsGroupSliding(), L"then over, not restarted by the key that scrolled nothing");
    }



    TEST_METHOD (PageSlide_OffUnlessTurnedOn)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (9);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (9,  list.GetTopRow());
        Assert::IsFalse  (list.IsGroupSliding());
    }



    //  A 270 px page moves at one speed over 175 ms, the first frame already
    //  33 ms along, the two frames a paced window takes to show what it draws:
    //  row 9, at rest at the top, is drawn 219 px down at first, 97 px down
    //  79 ms later, and at rest at the end.
    TEST_METHOD (PageSlide_MovesAtASteadySpeedToItsLastFrame)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now = 1000;

        ConfigureDetails (list, 40, 300);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (9);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (9, list.GetTopRow());

        list.Paint (painter, text, theme);
        Assert::AreEqual (219.0f, GetDrawnY (text, L"r9"), 0.5f);
        Assert::AreEqual (-21.0f, GetDrawnY (text, L"r1"), 0.5f, L"The rows it leaves drawn above it");

        now += 79;
        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (97.0f, GetDrawnY (text, L"r9"), 0.5f);

        now += 79;
        Assert::IsTrue  (list.IsGroupSliding(), L"Once more, for the frame where it rests");
        Assert::IsFalse (list.IsGroupSliding());

        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (0.0f,     GetDrawnY (text, L"r9"));
        Assert::AreEqual (-1000.0f, GetDrawnY (text, L"r1"));
    }



    //  A second page at once starts from where the first was drawn, 51 px
    //  down, but never more than the view's 300 px from where it now rests.
    TEST_METHOD (PageSlide_APageMidSlideStaysWithinAViewOfItsRest)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now = 1000;

        ConfigureDetails (list, 40, 300);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (9);

        list.OnKey (Key (VK_NEXT));
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (27, list.GetSelectedRow());
        Assert::AreEqual (18, list.GetTopRow());

        list.Paint (painter, text, theme);
        Assert::AreEqual (243.0f, GetDrawnY (text, L"r18"), 0.5f, L"300 px less the first frame's 57");
    }



    //  A page that scrolls nothing leaves a slide under way to run on: 50 ms
    //  in, row 9 is still drawn 142 px down. Any other scroll ends the slide
    //  and the view jumps, End and Home here, where a slide left running
    //  would carry on from where the page started.
    TEST_METHOD (PageSlide_AnyOtherScrollEndsIt)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now = 1000;

        ConfigureDetails (list, 40, 300);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (9);

        list.OnKey (Key (VK_NEXT));
        now += 50;
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (9, list.GetSelectedRow(), L"To the top row, nothing scrolled");

        list.Paint (painter, text, theme);
        Assert::AreEqual (142.0f, GetDrawnY (text, L"r9"), 0.5f, L"The slide runs on");

        list.OnKey (Key (VK_END));
        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (30,     list.GetTopRow());
        Assert::AreEqual (270.0f, GetDrawnY (text, L"r39"), 0.5f, L"End jumps to the end");

        list.OnKey (Key (VK_PRIOR));
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (21, list.GetTopRow(), L"A page back from row 30");

        now += 50;
        list.OnKey (Key (VK_HOME));
        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (0,    list.GetTopRow());
        Assert::AreEqual (0.0f, GetDrawnY (text, L"r0"), 0.5f, L"Home jumps to the top");
        Assert::IsFalse  (list.IsGroupSliding(), L"and nothing slides");
    }



    //  Five 30 px rows in sight. A click in the track below the thumb pages
    //  rows 10-14 to 15-19, a 150 px slide, so 35 ms into it, a fifth of its
    //  175, the view is drawn with row 11 at the top. Page Up from row 15 then
    //  lands on row 11 just where it was drawn, and nothing moves.
    TEST_METHOD (PageSlide_APageLandingWhereTheViewIsDrawnStaysPut)
    {
        DxuiListView                    list;
        DxuiListView::ScrollbarMetrics  bar;
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        int64_t                         now = 1000;

        ConfigureDetails (list, 40, 150);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (15);
        list.SetTopRow (10);

        bar = list.GetScrollbarGeometry();
        list.GetPageFromTrackClick (bar.trackTop + bar.trackH - 2);
        Assert::AreEqual (15, list.GetTopRow());

        //  35 ms into the slide, whose first frame was already 33 ms along.
        now += 2;
        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (11, list.GetSelectedRow());
        Assert::AreEqual (11, list.GetTopRow());

        list.Paint (painter, text, theme);
        Assert::AreEqual (0.0f, GetDrawnY (text, L"r11"), 0.5f, L"Row 11 stays at the top, where it was drawn");
        Assert::IsFalse  (list.IsGroupSliding());
    }



    //  While a page slides, a point hits what is drawn there. Row 9 rests at
    //  the top but is drawn from 219 px, and row 1 down to 9 px; in Medium
    //  icons, item 30 is drawn below where its cell rests.
    TEST_METHOD (PageSlide_HitTestsFollowTheRowsAsDrawn)
    {
        DxuiListView          details;
        DxuiListView          items;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now  = 1000;
        RECT                  rest = {};
        float                 y    = 0.0f;

        ConfigureDetails (details, 40, 300);
        details.SetPageSlideEnabled (true);
        details.SetClock ([&now] () { return now; });
        details.SetSelectedRow (9);
        details.OnKey (Key (VK_NEXT));

        Assert::AreEqual (9, details.HitTestRow (10, 225), L"Row 9, drawn from 219 px");
        Assert::AreEqual (1, details.HitTestRow (10, 5),   L"Row 1, drawn above it");

        ConfigureItems (items, View::MediumIcons, 200, 600, 400);
        items.SetPageSlideEnabled (true);
        items.SetClock ([&now] () { return now; });
        items.SetSelectedRow (16);
        items.OnKey (Key (VK_NEXT));
        items.OnKey (Key (VK_NEXT));

        items.Paint (painter, text, theme);
        rest = GetRect (items, 30);
        y    = GetDrawnY (text, L"r30");
        Assert::IsTrue   (y > (float) rest.bottom, L"Item 30's name is drawn below where its cell rests");
        Assert::AreEqual (30, items.HitTestRow ((rest.left + rest.right) / 2, (int) y + 1));
    }



    //  Over 500 items, names are measured as they come into sight, so the
    //  four-line names a page brings in make their rows taller on the slide's
    //  first frame, which moves where the view rests. That frame is still
    //  drawn 33 ms of the way along, from where the view was drawn to where it
    //  rests once they are measured, paging down or up.
    TEST_METHOD (PageSlide_NamesMeasuredOnTheFirstFrameMoveItOnlyItsLead)
    {
        for (bool down : { true, false })
        {
            DxuiListView          list;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            int64_t               now     = 1000;
            int                   tracked = down ? 0 : 280;
            std::wstring          name    = L"n" + std::to_wstring (tracked) + L" ";
            float                 drawn   = 0.0f;
            int                   topWas  = 0;
            int                   restTop = 0;

            ConfigureItems (list, View::MediumIcons, 600, 600, 400);
            SetLongNames   (list, 600);
            list.SetPageSlideEnabled (true);
            list.SetClock ([&now] () { return now; });
            list.SetSelectedRow (down ? 1 : 296);
            list.SetTopRow (down ? 0 : 40);

            list.Paint (painter, text, theme);
            drawn  = GetDrawnYStarting (text, name);
            topWas = GetRect (list, tracked).top;
            Assert::IsTrue (drawn > -1000.0f, L"The tracked item is drawn before the page");

            list.OnKey (Key (down ? VK_NEXT : VK_PRIOR));
            list.OnKey (Key (down ? VK_NEXT : VK_PRIOR));
            Assert::AreNotEqual (topWas, (int) GetRect (list, tracked).top, L"The second key scrolled");

            text.Reset();
            list.Paint (painter, text, theme);
            restTop = GetRect (list, tracked).top;

            Assert::AreEqual (drawn - (float) (topWas - restTop) * 33.0f / 175.0f, GetDrawnYStarting (text, name), 1.5f,
                              down ? L"Paging down" : L"Paging up");
        }
    }



    //  A page is reported once the view rests where the page leaves it and its
    //  slide has begun, so a host that draws from the report, as one filling a
    //  preview does, draws the slide's first frame: row 9 at 219 px, not flush
    //  with the top where the page ends.
    TEST_METHOD (PageSlide_TheMoveIsReportedOnceTheSlideHasBegun)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now      = 1000;
        int                   reported = -1;
        int                   topThen  = -1;
        float                 drawn    = -1000.0f;

        ConfigureDetails (list, 40, 300);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (9);
        list.SetOnSelectionChanged ([&] (int row)
        {
            reported = row;
            topThen  = list.GetTopRow();

            text.Reset();
            list.Paint (painter, text, theme);
            drawn = GetDrawnY (text, L"r9");
        });

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (18,     reported);
        Assert::AreEqual (9,      topThen, L"The view already where the page leaves it");
        Assert::AreEqual (219.0f, drawn,   0.5f, L"Drawn where the slide starts, not where it ends");
    }



    //  With the header showing, the rows are clipped to the body under it: its
    //  fill covers a row's, but text is drawn after every fill, so a row a page
    //  slides up out of the body would write over the column titles. Paging on
    //  from row 7, the bottom whole row under the 34 px header, the slide's
    //  first frame draws a row above the body, inside that clip.
    TEST_METHOD (PageSlide_DetailsRowsStayBelowTheHeader)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int64_t               now    = 1000;
        int                   above  = 0;
        bool                  inBody = false;

        ConfigureDetails (list, 40, 300);
        list.SetShowHeader (true);
        list.SetPageSlideEnabled (true);
        list.SetClock ([&now] () { return now; });
        list.SetSelectedRow (7);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (14, list.GetSelectedRow());

        list.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::PushClipRect && call.y == 34.0f && call.height == 266.0f)
            {
                inBody = true;
            }
            else if (call.kind == RecordedTextKind::PopClipRect)
            {
                inBody = false;
            }
            else if (call.kind == RecordedTextKind::DrawString && call.text.starts_with (L"r"))
            {
                Assert::IsTrue (inBody, call.text.c_str());
                above += (call.y < 34.0f) ? 1 : 0;
            }
        }

        Assert::IsTrue (above > 0, L"A row drawn above the body, inside the clip");
    }



    //  A host putting the view back after its rows change, as a folder's
    //  refresh does: the same top leaves the rows a page down lifted as they
    //  are, and a top two rows on keeps them lifted, so nothing on screen
    //  moves by part of a row.
    TEST_METHOD (Details_MoveTopRowKeepsTheLift)
    {
        DxuiListView  list;

        ConfigureDetails (list, 40, 310);
        list.SetSelectedRow (9);
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual ( 8, list.GetTopRow());
        Assert::AreEqual (18, list.HitTestRow (10, 305));
        Assert::AreEqual ( 9, list.HitTestRow (10, 25));

        list.MoveTopRow (8);
        Assert::AreEqual ( 8, list.GetTopRow());
        Assert::AreEqual (18, list.HitTestRow (10, 305), L"Row 18 still flush with the bottom");
        Assert::AreEqual ( 9, list.HitTestRow (10, 25),  L"and row 8 still cut off");

        list.MoveTopRow (10);
        Assert::AreEqual (10, list.GetTopRow());
        Assert::AreEqual (20, list.HitTestRow (10, 305), L"Two rows on, still lifted");
        Assert::AreEqual (11, list.HitTestRow (10, 25));
    }



    //  The same for an item view: the same top leaves the line a page down
    //  put flush with the bottom where it is.
    TEST_METHOD (MediumIcons_MoveTopRowKeepsThePage)
    {
        DxuiListView  list;

        ConfigureItems (list, View::MediumIcons, 200, 600, 400);
        list.SetSelectedRow (16);
        list.OnKey (Key (VK_NEXT));
        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (400, (int) GetRect (list, 58).bottom);

        list.MoveTopRow (list.GetTopRow());
        Assert::AreEqual (400, (int) GetRect (list, 58).bottom, L"Still flush with the bottom");
    }



    //  A click in the scrollbar's track pages the view and leaves the focus.
    TEST_METHOD (TrackClick_PagesTheViewAndLeavesTheFocus)
    {
        DxuiListView                    list;
        DxuiListView::ScrollbarMetrics  bar;

        ConfigureDetails (list, 40, 300);
        list.SetSelectedRow (4);

        bar = list.GetScrollbarGeometry();
        Assert::IsTrue (bar.visible);

        list.GetPageFromTrackClick (bar.trackTop + bar.trackH - 2);
        Assert::AreEqual (10, list.GetTopRow());
        Assert::AreEqual ( 4, list.GetSelectedRow());
        Assert::IsTrue   (list.IsRowSelected (4));
    }



    //  Items 3 and 14 are on the third row of columns 0 and 1, the two whole
    //  in sight; 25, the same row in column 2, comes in with column 1 moved
    //  to the left edge at 254 px.
    TEST_METHOD (List_PagesAcrossColumnsKeepingTheRow)
    {
        DxuiListView  list;

        ConfigureItems (list, View::List, 60, 600, 400);
        list.SetSelectedRow (3);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (14, list.GetSelectedRow(), L"The right whole column, same row");
        Assert::AreEqual ( 0, list.GetLeftPx());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual ( 25, list.GetSelectedRow(), L"The column whole at the right once column 1 is at the left");
        Assert::AreEqual (254, list.GetLeftPx());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual ( 14, list.GetSelectedRow(), L"The left whole column");
        Assert::AreEqual (254, list.GetLeftPx());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (3, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetLeftPx());
    }



    TEST_METHOD (List_HomeAndEndGoToTheEnds)
    {
        DxuiListView  list;

        ConfigureItems (list, View::List, 60, 600, 400);
        list.SetSelectedRow (30);

        list.OnKey (Key (VK_END));
        Assert::AreEqual ( 59, list.GetSelectedRow());
        Assert::AreEqual (854, list.GetLeftPx());

        list.OnKey (Key (VK_HOME));
        Assert::AreEqual (0, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetLeftPx());
    }



    //  800x300: seven items to a column, A's three columns from 22 px and
    //  B's three from 764 px. From A's first column, A's third is the right
    //  whole one; from there, B's second, with A's third at the left edge.
    TEST_METHOD (GroupedList_PagesAcrossBlocks)
    {
        DxuiListView  list;

        ConfigureItems (list, View::List, 40, 800, 300);
        list.SetGroups ({ { L"A", 0 }, { L"B", 20 } });
        list.SetSelectedRow (2);

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual (16, list.GetSelectedRow());
        Assert::AreEqual ( 0, list.GetLeftPx());

        list.OnKey (Key (VK_NEXT));
        Assert::AreEqual ( 29, list.GetSelectedRow());
        Assert::AreEqual (502, list.GetLeftPx());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual ( 16, list.GetSelectedRow());
        Assert::AreEqual (502, list.GetLeftPx());

        list.OnKey (Key (VK_PRIOR));
        Assert::AreEqual (2, list.GetSelectedRow());
        Assert::AreEqual (0, list.GetLeftPx());

        list.OnKey (Key (VK_HOME));
        Assert::AreEqual (-1, list.GetFocusedGroup(), L"Home goes past A's header");
        Assert::AreEqual ( 0, list.GetSelectedRow());
    }
};
