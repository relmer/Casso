#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewPanelRoutingTests
//
//  A list view placed away from the origin as a plain child of a DxuiPanel.
//  DxuiPanel::OnMouse passes each event on in the parent's coordinates, so
//  the list subtracts its bounds itself; nothing between the panel and the
//  list translates the point.
//
//  Geometry: the list covers (200, 150) to (600, 450) at 96 DPI with the
//  header hidden, so each row is 30 px tall and row N spans y = 150 + 30 N
//  to 180 + 30 N in the panel's coordinates.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewPanelRoutingTests)
{
public:

    static constexpr int   s_kRowCount    = 40;
    static constexpr int   s_kRowHeightPx = 30;
    static constexpr int   s_kHeaderRowY  = 10;
    static constexpr int   s_kGrabTolPx   = 4;
    static constexpr RECT  s_kListRect    = { 200, 150, 600, 450 };


    static void  BuildList (DxuiListView & list, bool showHeader, std::vector<int> & activated)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        int                                           i = 0;



        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", 200 });
        cols.push_back (DxuiListView::Column { L"Size", 100 });

        for (i = 0; i < s_kRowCount; i++)
        {
            rows.push_back ({ DxuiListView::Cell { std::to_wstring (i), false },
                              DxuiListView::Cell { L"1 KB", false } });
        }

        list.SetColumns       (std::move (cols));
        list.SetShowHeader    (showHeader);
        list.Layout           (s_kListRect, scaler);
        list.SetRows          (std::move (rows));
        list.SetOnActivateRow ([&activated] (int row) { activated.push_back (row); });
    }


    static bool  SendMouse (DxuiPanel & panel, DxuiMouseEventKind kind, POINT pt)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = pt;

        return panel.OnMouse (ev);
    }


    static POINT  GetRowPoint (int row)
    {
        return POINT { s_kListRect.left + 100, s_kListRect.top + row * s_kRowHeightPx + s_kRowHeightPx / 2 };
    }


    TEST_METHOD (ClickOnRow_throughPanel_selectsAndActivatesThatRow)
    {
        DxuiListView      list;
        DxuiPanel         panel;
        std::vector<int>  activated;
        bool              pressTaken   = false;
        bool              releaseTaken = false;



        BuildList   (list, false, activated);
        panel.Adopt (list);

        pressTaken   = SendMouse (panel, DxuiMouseEventKind::Down, GetRowPoint (4));
        releaseTaken = SendMouse (panel, DxuiMouseEventKind::Up,   GetRowPoint (4));

        Assert::IsTrue   (pressTaken,   L"The list takes a press inside its bounds");
        Assert::IsTrue   (releaseTaken, L"and the release that ends it");
        Assert::AreEqual (4, list.GetSelectedRow(), L"The row under the pointer is selected");
        Assert::AreEqual (size_t (1), activated.size());
        Assert::AreEqual (4, activated[0], L"The row under the pointer is activated");
    }


    TEST_METHOD (ClickOutsideList_throughPanel_isNotTaken)
    {
        DxuiListView      list;
        DxuiPanel         panel;
        std::vector<int>  activated;
        bool              taken = false;



        BuildList   (list, false, activated);
        panel.Adopt (list);

        //  A point inside the list only if it were read as list-relative.
        taken = SendMouse (panel, DxuiMouseEventKind::Down, POINT { 100, 45 });

        Assert::IsFalse  (taken, L"A press left of and above the list is not the list's");
        Assert::AreEqual (-1, list.GetSelectedRow(), L"and selects nothing");
    }


    TEST_METHOD (CursorOverDivider_throughPanel_isResizeCursor)
    {
        DxuiListView      list;
        DxuiPanel         panel;
        std::vector<int>  activated;
        int               dividerX = -1;
        int               x        = 0;
        LPCWSTR           atPanel  = nullptr;
        LPCWSTR           atLocal  = nullptr;



        BuildList   (list, true, activated);
        panel.Adopt (list);

        for (x = 0; x < s_kListRect.right - s_kListRect.left && dividerX < 0; x++)
        {
            if (list.HitTestColumnResize (x, s_kHeaderRowY, s_kGrabTolPx) == 0)
            {
                dividerX = x;
            }
        }

        Assert::IsTrue (dividerX >= 0, L"The first column has a divider to grab");

        atPanel = panel.GetCursorForPoint (POINT { s_kListRect.left + dividerX, s_kListRect.top + s_kHeaderRowY });
        atLocal = panel.GetCursorForPoint (POINT { dividerX, s_kHeaderRowY });

        Assert::IsTrue (atPanel == IDC_SIZEWE, L"Over the divider in the panel's coordinates");
        Assert::IsTrue (atLocal == nullptr,    L"The list-relative point is outside the list");
    }
};
