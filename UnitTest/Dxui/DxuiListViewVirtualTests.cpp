#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewVirtualTests
//
//  GH #88. Covers the virtual (provider) row model added to DxuiListView:
//  the list holds no materialized rows, tracks only a total count, and
//  pulls cells for the visible window from a callback during Paint. These
//  tests lock down the count/flag plumbing, the scroll math over a virtual
//  count, that Paint pulls ONLY the visible window (the whole point of the
//  fix), hit-testing by absolute row, EnsureVisible / SetSelectedRow
//  scroll-into-view, sticky-tail following a growing count, and the
//  mutual-exclusion between provider mode and pushed rows.
//
//  Geometry note: tests use a 400x300 px rect at 96 DPI with the header
//  hidden, so rowH == 30 px and the visible capacity is a round 10 rows
//  (300 / 30). The single fixed-width column keeps content width below the
//  viewport so no horizontal bar steals row capacity.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};

        out.left   = l;
        out.top    = t;
        out.right  = r;
        out.bottom = b;
        return out;
    }


    // Build a list laid out to a 400x300 rect at 96 DPI with the header
    // hidden and one fixed-width column -> exactly 10 visible rows.
    void  ConfigureList (DxuiListView & list)
    {
        DxuiDpiScaler        scaler;
        std::vector<DxuiListView::Column>  cols;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column{ L"C", 40 });
        list.SetColumns    (std::move (cols));
        list.SetShowHeader (false);
        list.Layout        (MakeRect (0, 0, 400, 300), scaler);
    }


    std::vector<std::vector<DxuiListView::Cell>>  MakeRows (int count)
    {
        std::vector<std::vector<DxuiListView::Cell>>  rows;

        for (int i = 0; i < count; i++)
        {
            rows.push_back ({ DxuiListView::Cell{ L"row", false } });
        }
        return rows;
    }
}




TEST_CLASS (DxuiListViewVirtualTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //  Count / flag plumbing
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (SetRowProvider_entersVirtualMode_reportsCount)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (50, [] (int, std::vector<DxuiListView::Cell> &) {});

        Assert::IsTrue   (list.IsVirtual());
        Assert::AreEqual (50, list.GetRowCount());
    }


    TEST_METHOD (SetVirtualRowCount_updatesCount_staysVirtual)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider     (50, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetVirtualRowCount (30);

        Assert::IsTrue   (list.IsVirtual());
        Assert::AreEqual (30, list.GetRowCount());
    }


    TEST_METHOD (SetVirtualRowCount_negative_clampsToZero)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider     (50, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetVirtualRowCount (-7);

        Assert::AreEqual (0, list.GetRowCount());
    }


    TEST_METHOD (SetRows_afterProvider_leavesVirtualMode)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (1000, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetRows        (MakeRows (3));

        Assert::IsFalse  (list.IsVirtual());
        Assert::AreEqual (3, list.GetRowCount());
    }


    ////////////////////////////////////////////////////////////////////////
    //  Scroll math over a virtual count
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (VirtualCount_capAndMaxTop_matchGeometry)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});

        Assert::AreEqual (10, list.GetVisibleRowCapacity());
        Assert::AreEqual (90, list.GetMaxTopRow());
    }


    TEST_METHOD (VirtualCount_belowCapacity_maxTopIsZero)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (4, [] (int, std::vector<DxuiListView::Cell> &) {});

        Assert::AreEqual (0, list.GetMaxTopRow());
    }


    TEST_METHOD (StickyTail_growingVirtualCount_followsBottom)
    {
        DxuiListView  list;

        ConfigureList (list);

        // Sticky by default -> installing 20 rows pins the view to the tail.
        list.SetRowProvider (20, [] (int, std::vector<DxuiListView::Cell> &) {});
        Assert::AreEqual (10, list.GetTopRow());
        Assert::IsTrue   (list.IsAtBottom());

        // Growing the count while parked at the tail keeps following it.
        list.SetVirtualRowCount (40);
        Assert::AreEqual (30, list.GetTopRow());
        Assert::IsTrue   (list.IsAtBottom());

        // Scrolling up breaks sticky; a later growth must NOT yank the view.
        list.SetTopRow (0);
        list.SetVirtualRowCount (60);
        Assert::AreEqual (0, list.GetTopRow());
    }


    ////////////////////////////////////////////////////////////////////////
    //  Paint pulls only the visible window (the fix)
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Paint_virtual_pullsOnlyVisibleWindow)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        std::vector<int>      requested;

        ConfigureList (list);
        list.SetRowProvider (1000, [&] (int row, std::vector<DxuiListView::Cell> & out)
        {
            requested.push_back (row);
            out.push_back ({ L"x", false });
        });

        list.SetTopRow (500);
        requested.clear();
        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        // Exactly the 10-row window [500, 510) -- nothing near the ends of
        // the 1000-row backing store.
        Assert::AreEqual (size_t (10), requested.size());
        for (size_t i = 0; i < requested.size(); i++)
        {
            Assert::AreEqual (500 + (int) i, requested[i]);
        }
    }


    TEST_METHOD (Paint_virtual_stickyTail_pullsTailWindow)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        std::vector<int>      requested;

        ConfigureList (list);
        list.SetRowProvider (1000, [&] (int row, std::vector<DxuiListView::Cell> & out)
        {
            requested.push_back (row);
            out.push_back ({ L"x", false });
        });

        // Sticky by default -> window is the last 10 rows [990, 1000).
        requested.clear();
        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        Assert::AreEqual (size_t (10), requested.size());
        Assert::AreEqual (990, requested.front());
        Assert::AreEqual (999, requested.back());
    }


    TEST_METHOD (Paint_afterSwitchToPush_providerNotCalled)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   providerCalls = 0;

        ConfigureList (list);
        list.SetRowProvider (1000, [&] (int, std::vector<DxuiListView::Cell> & out)
        {
            providerCalls++;
            out.push_back ({ L"x", false });
        });

        // Leaving virtual mode must detach the provider entirely.
        list.SetRows (MakeRows (5));
        static_cast<IDxuiControl &> (list).Paint (painter, text, theme);

        Assert::AreEqual (0, providerCalls);
        Assert::IsFalse  (list.IsVirtual());
    }


    ////////////////////////////////////////////////////////////////////////
    //  Hit-testing by absolute row in virtual mode
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (HitTestRow_virtual_addsTopRowToVisibleIndex)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetTopRow (20);

        // No header: body y maps directly through rowH (30 px).
        Assert::AreEqual (20, list.HitTestRow (10, 0));    // visible idx 0
        Assert::AreEqual (23, list.HitTestRow (10, 95));   // 95 / 30 == 3
    }


    TEST_METHOD (HitTestRow_virtual_belowLastVisibleRow_misses)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetTopRow (20);

        // y past the 10-row body -> no hit even though absolute rows exist.
        Assert::AreEqual (-1, list.HitTestRow (10, 5000));
    }


    ////////////////////////////////////////////////////////////////////////
    //  EnsureVisible / SetSelectedRow scroll-into-view in virtual mode
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (EnsureVisible_virtual_scrollsMinimally_keepsSelection)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetTopRow (50);

        // Row above the window -> top snaps down to it.
        list.EnsureVisible (20);
        Assert::AreEqual (20, list.GetTopRow());

        // Row below the window -> top snaps so the row is the last visible.
        list.EnsureVisible (75);
        Assert::AreEqual (66, list.GetTopRow());   // 75 - 10 + 1

        // Already visible -> no movement.
        list.EnsureVisible (70);
        Assert::AreEqual (66, list.GetTopRow());

        // EnsureVisible never selects.
        Assert::AreEqual (-1, list.GetSelectedRow());
    }


    TEST_METHOD (SetSelectedRow_virtual_clampsToCount_andScrollsIntoView)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});
        list.SetTopRow (0);

        // Past the end clamps to the last row and scrolls it into view.
        list.SetSelectedRow (500);
        Assert::AreEqual (99, list.GetSelectedRow());
        Assert::AreEqual (90, list.GetTopRow());   // 99 - 10 + 1

        // Selecting an earlier row scrolls back up to it.
        list.SetSelectedRow (5);
        Assert::AreEqual (5, list.GetSelectedRow());
        Assert::AreEqual (5, list.GetTopRow());
    }


    TEST_METHOD (SetSelectedRow_virtual_negative_clearsSelection)
    {
        DxuiListView  list;

        ConfigureList (list);
        list.SetRowProvider (100, [] (int, std::vector<DxuiListView::Cell> &) {});

        list.SetSelectedRow (-3);
        Assert::AreEqual (-1, list.GetSelectedRow());
    }
};
