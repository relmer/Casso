#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewCheckBoxTests
//
//  A list with check boxes, as Choose Details has: Space toggles the
//  selected row's box, a space typed in a type-ahead search continues the
//  search, and a character typed in a dialog goes to the list.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewCheckBoxTests)
{
public:

    struct Fixture
    {
        DxuiListView      list;
        int64_t           now     = 10000;
        std::vector<int>  toggled;

        Fixture()
        {
            DxuiDpiScaler                                 scaler;
            std::vector<std::vector<DxuiListView::Cell>>  rows;

            scaler.SetDpi (96);

            for (const wchar_t * name : { L"Date accessed", L"Date created", L"Dimensions" })
            {
                DxuiListView::Cell  cell { name, false };

                cell.check = DxuiListView::Cell::Check::Off;
                rows.push_back ({ cell });
            }

            list.SetColumns           ({ DxuiListView::Column { L"", 200 } });
            list.SetMultiSelect       (true);
            list.SetKeyboardColumnNav (true);
            list.SetRows              (std::move (rows));
            list.Layout               (RECT { 0, 0, 400, 600 }, scaler);
            list.SetClock             ([this] () { return now; });
            list.SetOnCheckToggled    ([this] (int row) { toggled.push_back (row); });
        }

        void  Press (WPARAM vk)
        {
            list.OnKey (DxuiKeyEvent { DxuiKeyEventKind::Down, vk });
        }
    };



    TEST_METHOD (Space_TogglesTheSelectedRow)
    {
        Fixture  f;

        f.list.SetSelectedRow (2);
        f.Press (VK_SPACE);
        f.list.OnChar (L' ');

        Assert::IsTrue (f.toggled == std::vector<int> ({ 2 }), L"Once, for the key and its character together");
    }



    TEST_METHOD (ASpaceInATypeAheadSearch_ContinuesIt)
    {
        Fixture  f;

        for (wchar_t ch : std::wstring (L"date"))
        {
            f.list.OnChar (ch);
        }

        f.Press (VK_SPACE);
        f.list.OnChar (L' ');
        f.list.OnChar (L'c');

        Assert::IsTrue   (f.toggled.empty());
        Assert::AreEqual (1, f.list.GetSelectedRow(), L"Date created");
    }



    TEST_METHOD (AnItemCheckBoxPress_TogglesItsRow_AndKeepsTheSelection)
    {
        constexpr int  kRowHeightPx = 30;
        constexpr int  kBoxInsidePx = 16;   // past the cell's padding, inside its box

        Fixture         f;
        DxuiMouseEvent  press;



        f.list.SetShowHeader     (false);
        f.list.SetItemCheckBoxes (true);
        f.list.SetSelectedRows   ({ 0 }, 0);

        press.kind        = DxuiMouseEventKind::Down;
        press.button      = DxuiMouseButton::Left;
        press.positionDip = POINT { kBoxInsidePx, kRowHeightPx * 2 + kRowHeightPx / 2 };

        Assert::IsTrue (f.list.AsControl().OnMouse (press));

        press.kind = DxuiMouseEventKind::Up;

        Assert::IsTrue (f.list.AsControl().OnMouse (press));
        Assert::IsTrue (f.toggled == std::vector<int> ({ 2 }));
        Assert::IsTrue (f.list.GetSelectedRows() == std::vector<int> ({ 0 }), L"Only the host's toggle changes the selection");
    }



    //  A popup menu over the list closes on its press, and the release goes to
    //  the list: it selects nothing, as the press was not the list's.
    TEST_METHOD (AReleaseWithNoPressInTheList_SelectsNothing)
    {
        constexpr int  kRowHeightPx = 30;
        constexpr int  kTextPx      = 100;

        Fixture         f;
        DxuiMouseEvent  release;



        f.list.SetShowHeader   (false);
        f.list.SetSelectedRows ({ 0 }, 0);

        release.kind        = DxuiMouseEventKind::Up;
        release.button      = DxuiMouseButton::Left;
        release.positionDip = POINT { kTextPx, kRowHeightPx * 2 + kRowHeightPx / 2 };

        f.list.AsControl().OnMouse (release);

        Assert::IsTrue (f.list.GetSelectedRows() == std::vector<int> ({ 0 }));
    }
};
