#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewGroupTests
//
//  Group headers in Details view: a header line above each group's first
//  row, row indices unchanged, and the scroll counted in lines.
//
//  Geometry: a 400x300 px rect at 96 DPI with the header hidden, so a line
//  is 30 px and ten fit.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewGroupTests)
{
public:

    static void  ConfigureList (DxuiListView & list, int rows)
    {
        DxuiDpiScaler                                  scaler;
        std::vector<DxuiListView::Column>              cols;
        std::vector<std::vector<DxuiListView::Cell>>   cells;
        RECT                                           rect = { 0, 0, 400, 300 };

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column{ L"C", 40 });
        list.SetColumns    (std::move (cols));
        list.SetShowHeader (false);
        list.SetMultiSelect (true);
        list.Layout        (rect, scaler);

        for (int i = 0; i < rows; i++)
        {
            cells.push_back ({ DxuiListView::Cell{ L"row", false } });
        }

        list.SetRows (std::move (cells));
    }



    //  Rows 0-2 in A, 3-5 in B: lines are A, 0, 1, 2, B, 3, 4, 5.
    TEST_METHOD (HitTest_HeadersTakeTheirOwnLines)
    {
        DxuiListView  list;

        ConfigureList (list, 6);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });

        Assert::AreEqual (-1, list.HitTestRow         (10,  5));
        Assert::AreEqual ( 0, list.HitTestGroupHeader (10,  5));
        Assert::AreEqual ( 0, list.HitTestRow         (10, 35));
        Assert::AreEqual ( 2, list.HitTestRow         (10, 95));
        Assert::AreEqual (-1, list.HitTestRow         (10, 125));
        Assert::AreEqual ( 1, list.HitTestGroupHeader (10, 125));
        Assert::AreEqual ( 3, list.HitTestRow         (10, 155));
        Assert::AreEqual (-1, list.HitTestGroupHeader (10, 155));
    }



    TEST_METHOD (SelectGroup_SelectsItsRows)
    {
        DxuiListView  list;

        ConfigureList (list, 6);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.SelectGroup (1);

        Assert::AreEqual ((size_t) 3, list.GetSelectedRows().size());
        Assert::IsTrue (list.IsRowSelected (3) && list.IsRowSelected (4) && list.IsRowSelected (5));
        Assert::IsFalse (list.IsRowSelected (2));
    }



    TEST_METHOD (Scroll_CountsHeaderLines)
    {
        DxuiListView  list;

        ConfigureList (list, 30);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });

        //  32 lines, ten shown.
        list.SetTopRow (1000);
        Assert::AreEqual (22, list.GetTopRow());
    }



    TEST_METHOD (EnsureVisible_BringsAGroupsHeaderWithItsFirstRow)
    {
        DxuiListView  list;

        ConfigureList (list, 30);
        list.SetGroups ({ { L"A", 0 }, { L"B", 15 } });

        //  Row 15 is on line 17, its header on line 16.
        list.SetTopRow (20);
        list.EnsureVisible (15);
        Assert::AreEqual (16, list.GetTopRow());

        list.SetTopRow (0);
        list.EnsureVisible (15);
        Assert::AreEqual (8, list.GetTopRow());
    }



    TEST_METHOD (NoGroups_LinesAreRows)
    {
        DxuiListView  list;

        ConfigureList (list, 6);

        Assert::AreEqual (0, list.HitTestRow (10, 5));
        Assert::AreEqual (-1, list.HitTestGroupHeader (10, 5));
    }


    static DxuiMouseEvent  Press (int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };
        return ev;
    }



    static DxuiKeyEvent  Key (WPARAM vk)
    {
        DxuiKeyEvent  ev;

        ev.kind = DxuiKeyEventKind::Down;
        ev.vk   = vk;
        return ev;
    }



    //  Collapsed, B keeps its header and hides 3-5: lines are A, 0, 1, 2, B.
    TEST_METHOD (Collapse_HidesRowsAndKeepsTheHeader)
    {
        DxuiListView  list;

        ConfigureList (list, 6);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.SetGroupCollapsed (1, true);

        Assert::IsTrue   (list.IsGroupCollapsed (1));
        Assert::AreEqual ( 1, list.HitTestGroupHeader (10, 125));
        Assert::AreEqual (-1, list.HitTestRow         (10, 155), L"Nothing below a collapsed last group");

        //  Set again under the same labels, it stays collapsed.
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        Assert::IsTrue   (list.IsGroupCollapsed (1));

        list.SetAllGroupsCollapsed (false);
        Assert::AreEqual ( 3, list.HitTestRow (10, 155));
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



    //  A's rows (90 px) slide up under its header over 250 ms on an ease in
    //  and out: halfway through, 45 px of them show, clipped below the header,
    //  and B's header follows their edge.
    TEST_METHOD (Collapse_SlidesTheRowsUnderTheHeader)
    {
        DxuiListView                                   list;
        MockDxuiPainter                                painter;
        MockDxuiTextRenderer                           text;
        MockDxuiTheme                                  theme;
        int64_t                                        now  = 1000;
        std::vector<std::vector<DxuiListView::Cell>>   cells;
        bool                                           clip = false;

        ConfigureList (list, 6);

        for (int i = 0; i < 6; i++)
        {
            cells.push_back ({ DxuiListView::Cell{ L"r" + std::to_wstring (i), false } });
        }

        //  Narrow enough for the 40 px column, so it is drawn rather than elided.
        text.SetCannedMetrics (L"r2", SIZE { 8, 16 });

        list.SetRows   (std::move (cells));
        list.SetClock  ([&now] () { return now; });
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.SetGroupCollapsed (0, true);

        list.Paint (painter, text, theme);
        Assert::AreEqual (120.0f, GetDrawnY (text, L"B"), L"At the start nothing has moved");
        Assert::IsTrue   (list.IsGroupSliding());

        now += 125;
        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (75.0f,  GetDrawnY (text, L"B"),  0.5f);
        Assert::AreEqual (45.0f,  GetDrawnY (text, L"r2"), 0.5f, L"Lifted by the 45 px out of sight");

        for (const RecordedTextCall & call : text.Calls())
        {
            clip = clip || (call.kind == RecordedTextKind::PushClipRect && call.y == 30.0f && std::abs (call.height - 45.0f) < 0.5f);
        }

        Assert::IsTrue (clip, L"Clipped to the 45 px under A's header");

        now += 125;
        Assert::IsTrue  (list.IsGroupSliding(), L"Once more, for the frame where it rests");
        Assert::IsFalse (list.IsGroupSliding());

        text.Reset();
        list.Paint (painter, text, theme);
        Assert::AreEqual (30.0f,   GetDrawnY (text, L"B"));
        Assert::AreEqual (-1000.0f, GetDrawnY (text, L"r2"));
    }



    //  The chevron at the header's start toggles; the rest of it selects.
    TEST_METHOD (HeaderPress_ChevronTogglesAndLabelSelects)
    {
        DxuiListView  list;

        ConfigureList (list, 6);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });

        list.OnMouse (Press (15, 125));
        Assert::IsTrue   (list.IsGroupCollapsed (1));
        Assert::AreEqual (1, list.GetFocusedGroup());

        list.OnMouse (Press (200, 5));
        Assert::IsFalse  (list.IsGroupCollapsed (0));
        Assert::AreEqual (0, list.GetFocusedGroup());
        Assert::AreEqual ((size_t) 3, list.GetSelectedRows().size());
    }



    //  Down from the last row of A reaches B's header, selecting B's rows;
    //  Left there collapses B and Right opens it.
    TEST_METHOD (Keys_HeaderIsAStopAndLeftRightToggle)
    {
        DxuiListView  list;

        ConfigureList (list, 6);
        list.SetKeyboardColumnNav (true);
        list.OnFocusChanged (true);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.SetSelectedRow (2);

        list.OnKey (Key (VK_DOWN));
        Assert::AreEqual (1, list.GetFocusedGroup());
        Assert::IsTrue   (list.IsRowSelected (3) && list.IsRowSelected (5));

        list.OnKey (Key (VK_LEFT));
        Assert::IsTrue   (list.IsGroupCollapsed (1));

        list.OnKey (Key (VK_RIGHT));
        Assert::IsFalse  (list.IsGroupCollapsed (1));

        list.OnKey (Key (VK_DOWN));
        Assert::AreEqual (-1, list.GetFocusedGroup());
        Assert::AreEqual ( 3, list.GetSelectedRow());
    }


    //  Rows chosen from a header take Ctrl clicks as any selection does,
    //  down to one row left.
    TEST_METHOD (CtrlClick_AfterAHeaderKeepsTheRest)
    {
        DxuiListView    list;
        DxuiMouseEvent  ev = Press (200, 155);

        ConfigureList (list, 6);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.OnMouse (Press (200, 125));

        ev.ctrl = true;
        list.OnMouse (ev);
        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);

        Assert::IsFalse  (list.IsRowSelected (3));
        Assert::IsTrue   (list.IsRowSelected (4) && list.IsRowSelected (5));

        ev = Press (200, 185);
        ev.ctrl = true;
        list.OnMouse (ev);
        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);

        Assert::IsFalse  (list.IsRowSelected (4), L"Ctrl took the row off");
        Assert::IsTrue   (list.IsRowSelected (5), L"and the last one stayed");
    }


    //  From a header's selection, Ctrl walks the focus without changing it,
    //  and Ctrl+Space adds the focused row, as in Explorer.
    TEST_METHOD (CtrlKeys_MoveFocusAndSpaceToggles)
    {
        DxuiListView  list;
        DxuiKeyEvent  up    = Key (VK_UP);
        DxuiKeyEvent  space = Key (VK_SPACE);

        ConfigureList (list, 6);
        list.SetKeyboardColumnNav (true);
        list.OnFocusChanged (true);
        list.SetGroups ({ { L"A", 0 }, { L"B", 3 } });
        list.OnMouse (Press (200, 125));

        up.ctrl    = true;
        space.ctrl = true;

        list.OnKey (up);
        Assert::AreEqual (-1, list.GetFocusedGroup());
        Assert::AreEqual ( 2, list.GetSelectedRow(), L"Focus on row 2");
        Assert::AreEqual ((size_t) 3, list.GetSelectedRows().size(), L"selection untouched");

        list.OnKey (space);
        Assert::IsTrue   (list.IsRowSelected (2));
        Assert::AreEqual ((size_t) 4, list.GetSelectedRows().size());

        list.OnKey (space);
        Assert::IsFalse  (list.IsRowSelected (2), L"and toggles off again");
    }


    //  Medium icons in 400 px: A's six items take two rows under its header,
    //  and B starts a row of its own under its header.
    TEST_METHOD (ItemsView_HeadersAndRowsStackByGroup)
    {
        DxuiListView  list;
        RECT          item0 = {};
        RECT          item1 = {};
        RECT          item5 = {};
        RECT          item6 = {};

        ConfigureList (list, 8);
        list.SetView   (DxuiListView::View::MediumIcons);
        list.SetGroups ({ { L"A", 0 }, { L"B", 6 } });

        Assert::IsTrue (list.GetItemRectPx (0, item0) && list.GetItemRectPx (1, item1));
        Assert::IsTrue (list.GetItemRectPx (5, item5) && list.GetItemRectPx (6, item6));

        Assert::AreEqual ( 0, list.HitTestGroupHeader (10, item0.top - 5), L"A's header is over its first row");
        Assert::AreEqual ( 0, list.HitTestRow         (item0.left + 2, item0.top + 2));
        Assert::IsTrue   (item5.top > item0.top, L"A's last item is on a second row");
        Assert::AreEqual ( 5, list.HitTestRow         (item5.left + 2, item5.top + 2));
        Assert::AreEqual (-1, list.HitTestRow         (item5.left + (item1.left - item0.left) + 2, item5.top + 2), L"and nothing follows it there");
        Assert::AreEqual ( 1, list.HitTestGroupHeader (10, item6.top - 5), L"B's header is over its first row");
        Assert::AreEqual ( 6, list.HitTestRow         (item6.left + 2, item6.top + 2));

        list.SetGroupCollapsed (0, true);
        Assert::AreEqual ( 1, list.HitTestGroupHeader (10, item0.top + 5), L"B's header moves up under a collapsed A");
    }



    //  Home in a grouped item view puts the focus on the first item, past
    //  the header above it, and leaves the selection alone, as Explorer does.
    TEST_METHOD (ItemsView_HomeFocusesTheFirstItem)
    {
        DxuiListView  list;

        ConfigureList (list, 8);
        list.SetKeyboardColumnNav (true);
        list.OnFocusChanged (true);
        list.SetView   (DxuiListView::View::MediumIcons);
        list.SetGroups ({ { L"A", 0 }, { L"B", 6 } });
        list.SetSelectedRow (6);

        list.OnKey (Key (VK_HOME));

        Assert::AreEqual (-1, list.GetFocusedGroup(), L"not A's header");
        Assert::AreEqual ( 0, list.GetSelectedRow(),  L"the focus is on the first item");
        Assert::IsTrue   (list.IsRowSelected (6),     L"the selection is untouched");
        Assert::IsFalse  (list.IsRowSelected (0));
        Assert::IsFalse  (list.IsRowSelected (1),     L"and A's items are not selected");
    }

    //  List, 240 px columns of 33 px rows under a 30 px header: A's block is
    //  its label over its items, indented 22 px, and B's block follows it.
    TEST_METHOD (ListView_GroupsAreBlocksOfColumns)
    {
        DxuiListView  list;

        ConfigureList (list, 8);
        list.SetView   (DxuiListView::View::List);
        list.SetGroups ({ { L"A", 0 }, { L"B", 6 } });

        Assert::AreEqual ( 0, list.HitTestGroupHeader (10, 5));
        Assert::AreEqual (-1, list.HitTestRow         (10, 35), L"The indent holds no item");
        Assert::AreEqual ( 0, list.HitTestRow         (30, 35));
        Assert::AreEqual ( 5, list.HitTestRow         (30, 30 + 33 * 5 + 5));
        Assert::AreEqual ( 1, list.HitTestGroupHeader (270, 5));
        Assert::AreEqual ( 6, list.HitTestRow         (290, 35));
    }
};
