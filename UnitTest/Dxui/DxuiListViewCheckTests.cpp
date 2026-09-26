#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "Core/UnicodeSymbols.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewCheckTests
//
//  A cell's checkbox: drawn ahead of its icon and text, which move along past
//  it; a press on it reports the toggle without moving the selection; and
//  Space toggles the checkboxes of the selected rows, all to the state
//  opposite the keyboard row's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewCheckTests)
{
public:

    struct Toggle
    {
        int     row     = -1;
        size_t  column  = 0;
        bool    checked = false;
    };


    //  Rows alternate checked and clear, starting clear; the last row has no
    //  checkbox at all.
    static void  ConfigureList (DxuiListView & list, int rows)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<std::vector<DxuiListView::Cell>>  data;

        scaler.SetDpi (96);

        for (int i = 0; i < rows; i++)
        {
            DxuiListView::Cell  cell;

            cell.text  = L"breakpoint";
            cell.check = (i == rows - 1) ? std::optional<bool>() : std::optional<bool> ((i % 2) == 1);

            data.push_back ({ cell, DxuiListView::Cell { L"1", false } });
        }

        list.SetColumns           ({ DxuiListView::Column { L"Name", 160 }, DxuiListView::Column { L"Hits", 60 } });
        list.SetRows              (std::move (data));
        list.SetMultiSelect       (true);
        list.SetKeyboardColumnNav (true);
        list.Layout               (RECT { 0, 0, 400, 300 }, scaler);
    }


    static DxuiMouseEvent  MakePress (POINT at)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = at;
        return ev;
    }


    static DxuiKeyEvent  MakeKey (DxuiKeyEventKind kind, WPARAM vk)
    {
        DxuiKeyEvent  ev;

        ev.kind = kind;
        ev.vk   = vk;
        return ev;
    }


    static POINT  Center (const RECT & r)
    {
        return POINT { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
    }


    TEST_METHOD (APressOnTheCheckboxReportsTheToggleAndKeepsTheSelection)
    {
        DxuiListView         list;
        RECT                 box = {};
        std::vector<Toggle>  toggles;

        ConfigureList (list, 5);
        list.SetOnCheckToggled ([&] (int row, size_t column, bool checked) { toggles.push_back ({ row, column, checked }); });
        list.ClickRow (3, false, false);

        Assert::IsTrue (list.GetCheckRectPx (0, 0, box));
        Assert::IsTrue (list.OnMouse (MakePress (Center (box))));

        Assert::AreEqual ((size_t) 1, toggles.size());
        Assert::AreEqual (0, toggles[0].row);
        Assert::IsTrue   (toggles[0].checked, L"a clear box asks to be checked");
        Assert::IsTrue   (list.GetSelectedRows() == std::vector<int> { 3 }, L"the selection stays where it was");
    }


    TEST_METHOD (APressBesideTheCheckboxSelectsTheRow)
    {
        DxuiListView         list;
        RECT                 text = {};
        std::vector<Toggle>  toggles;

        ConfigureList (list, 5);
        list.SetOnCheckToggled ([&] (int row, size_t column, bool checked) { toggles.push_back ({ row, column, checked }); });

        Assert::IsTrue (list.GetCellTextRectPx (2, 0, text));
        list.OnMouse (MakePress (POINT { text.left + 4, (text.top + text.bottom) / 2 }));

        Assert::IsTrue (toggles.empty());
        Assert::AreEqual (2, list.GetSelectedRow());
    }


    TEST_METHOD (TheTextMovesPastTheCheckbox)
    {
        DxuiListView  list;
        RECT          withBox = {};
        RECT          without = {};
        RECT          box     = {};

        ConfigureList (list, 5);

        Assert::IsTrue  (list.GetCellTextRectPx (0, 0, withBox));
        Assert::IsTrue  (list.GetCellTextRectPx (4, 0, without));
        Assert::IsTrue  (list.GetCheckRectPx    (0, 0, box));
        Assert::IsFalse (list.GetCheckRectPx    (4, 0, box), L"the last row has no checkbox");
        Assert::IsFalse (list.GetCheckRectPx    (0, 1, box), L"nor has the second column");

        Assert::IsTrue (withBox.left > without.left);
        Assert::IsTrue (list.GetCheckRectPx (0, 0, box) && withBox.left >= box.right, L"the text starts past the box");
    }


    TEST_METHOD (SpaceTogglesTheSelectedRowsToTheKeyboardRowsOpposite)
    {
        DxuiListView         list;
        std::vector<Toggle>  toggles;

        ConfigureList (list, 5);
        list.SetOnCheckToggled ([&] (int row, size_t column, bool checked) { toggles.push_back ({ row, column, checked }); });

        //  Row 1 is checked and row 2 clear; the keyboard ends on row 1.
        list.ClickRow (2, false, false);
        list.ClickRow (1, true,  false);

        Assert::IsTrue (list.OnKey (MakeKey (DxuiKeyEventKind::Down, VK_SPACE)));
        Assert::IsTrue (list.OnKey (MakeKey (DxuiKeyEventKind::Char, L' ')), L"the character Space sends is spent");

        Assert::AreEqual ((size_t) 2, toggles.size());
        Assert::AreEqual (1, toggles[0].row, L"the keyboard row first");

        for (const Toggle & toggle : toggles)
        {
            Assert::IsFalse (toggle.checked, L"all go the way the keyboard row goes");
        }
    }


    TEST_METHOD (SpaceOnARowWithoutACheckboxIsLeftAlone)
    {
        DxuiListView         list;
        std::vector<Toggle>  toggles;

        ConfigureList (list, 5);
        list.SetOnCheckToggled ([&] (int row, size_t column, bool checked) { toggles.push_back ({ row, column, checked }); });
        list.ClickRow (4, false, false);

        (void) list.OnKey (MakeKey (DxuiKeyEventKind::Down, VK_SPACE));
        Assert::IsTrue (toggles.empty());
    }


    TEST_METHOD (ACheckedBoxCarriesAMarkAndAClearOneNone)
    {
        DxuiListView          list;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        size_t                marks = 0;

        ConfigureList (list, 5);
        list.SetTheme (&theme);
        list.Paint    (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            marks += (call.text == s_kpszMdl2Accept) ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 2, marks, L"rows 1 and 3 are checked");
    }
};
