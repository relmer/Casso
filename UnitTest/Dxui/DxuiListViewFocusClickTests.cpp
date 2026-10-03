#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewFocusClickTests
//
//  A click on a list that does not yet hold focus. The wheel scrolls a list
//  without focusing it, so the first click after a wheel scroll is also the
//  one that focuses it, and the host delivers the focus change before the
//  press. These tests drive that order -- wheel, focus, press, release -- and
//  check that the row activated is the row under the pointer, not the row at
//  that position on the first page.
//
//  Geometry: a 400x300 px rect at 96 DPI with the header hidden, so rowH ==
//  30 px and the visible capacity is 10 rows. Wheel scrolls use 3 rows per
//  notch, so the top row is never a multiple of the page size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewFocusClickTests)
{
public:

    static constexpr int  s_kRowCount      = 100;
    static constexpr int  s_kRowHeightPx   = 30;
    static constexpr int  s_kLinesPerNotch = 3;


    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};

        out.left   = l;
        out.top    = t;
        out.right  = r;
        out.bottom = b;
        return out;
    }


    // Configured as the disk picker configures its list: keyboard column
    // navigation on, and a row activates on a single click's release.
    void  ConfigureList (DxuiListView & list, std::vector<int> & activated)
    {
        DxuiDpiScaler                                   scaler;
        std::vector<DxuiListView::Column>               cols;
        std::vector<std::vector<DxuiListView::Cell>>    rows;



        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column{ L"C", 40 });

        for (int i = 0; i < s_kRowCount; i++)
        {
            rows.push_back ({ DxuiListView::Cell{ std::to_wstring (i), false } });
        }

        list.SetColumns           (std::move (cols));
        list.SetShowHeader        (false);
        list.SetKeyboardColumnNav (true);
        list.Layout               (MakeRect (0, 0, 400, 300), scaler);
        list.SetRows              (std::move (rows));
        list.SetOnActivateRow     ([&activated] (int row) { activated.push_back (row); });
    }


    void  SendMouse (DxuiListView & list, DxuiMouseEventKind kind, int visibleRow)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { 100, visibleRow * s_kRowHeightPx + s_kRowHeightPx / 2 };

        list.OnMouse (ev);
    }


    // The host's order for a click on an unfocused list: focus, press, release.
    void  ClickUnfocused (DxuiListView & list, int visibleRow)
    {
        list.OnFocusChanged (true);
        SendMouse (list, DxuiMouseEventKind::Down, visibleRow);
        SendMouse (list, DxuiMouseEventKind::Up,   visibleRow);
    }


    void  WheelDownNotches (DxuiListView & list, int notches)
    {
        for (int i = 0; i < notches; i++)
        {
            list.ScrollByWheelDelta (-WHEEL_DELTA, s_kLinesPerNotch);
        }
    }


    TEST_METHOD (WheelScrolled_firstClick_activatesRowUnderPointer)
    {
        DxuiListView      list;
        std::vector<int>  activated;



        ConfigureList    (list, activated);
        WheelDownNotches (list, 5);

        Assert::AreEqual (15, list.GetTopRow());

        ClickUnfocused (list, 4);

        Assert::AreEqual (15,     list.GetTopRow());
        Assert::AreEqual (size_t (1), activated.size());
        Assert::AreEqual (19,     activated[0]);
        Assert::AreEqual (19,     list.GetSelectedRow());
    }


    TEST_METHOD (WheelScrolledToLastPage_firstClick_activatesRowUnderPointer)
    {
        DxuiListView      list;
        std::vector<int>  activated;



        ConfigureList    (list, activated);
        WheelDownNotches (list, 40);

        Assert::AreEqual (s_kRowCount - 10, list.GetTopRow());

        ClickUnfocused (list, 9);

        Assert::AreEqual (size_t (1),      activated.size());
        Assert::AreEqual (s_kRowCount - 1, activated[0]);
    }


    TEST_METHOD (WheelScrolledByMouseEvent_firstClick_activatesRowUnderPointer)
    {
        DxuiListView      list;
        std::vector<int>  activated;
        DxuiMouseEvent    wheel;
        int               top = 0;



        ConfigureList (list, activated);

        // Through OnMouse, as the picker receives it. The rows per notch come
        // from the user's mouse settings, so read where the list landed.
        wheel.kind        = DxuiMouseEventKind::Wheel;
        wheel.positionDip = POINT { 100, 100 };
        wheel.wheelDelta  = -1.0f;
        list.OnMouse (wheel);

        top = list.GetTopRow();
        Assert::IsTrue (top > 0);

        ClickUnfocused (list, 2);

        Assert::AreEqual (top,        list.GetTopRow());
        Assert::AreEqual (size_t (1), activated.size());
        Assert::AreEqual (top + 2,    activated[0]);
    }


    TEST_METHOD (Focus_scrolledWithoutSelection_selectsTopVisibleRowWithoutScrolling)
    {
        DxuiListView      list;
        std::vector<int>  activated;



        ConfigureList    (list, activated);
        WheelDownNotches (list, 7);

        list.OnFocusChanged (true);

        Assert::AreEqual (21, list.GetTopRow());
        Assert::AreEqual (21, list.GetSelectedRow());
    }


    TEST_METHOD (Focus_atTopWithoutSelection_selectsFirstRow)
    {
        DxuiListView      list;
        std::vector<int>  activated;



        ConfigureList (list, activated);

        list.OnFocusChanged (true);

        Assert::AreEqual (0, list.GetTopRow());
        Assert::AreEqual (0, list.GetSelectedRow());
    }


    TEST_METHOD (Focus_withSelectionScrolledAway_keepsSelectionAndScroll)
    {
        DxuiListView      list;
        std::vector<int>  activated;



        ConfigureList (list, activated);
        list.SetSelectedRow (2);
        WheelDownNotches (list, 5);

        list.OnFocusChanged (true);

        Assert::AreEqual (15, list.GetTopRow());
        Assert::AreEqual (2,  list.GetSelectedRow());
    }
};
