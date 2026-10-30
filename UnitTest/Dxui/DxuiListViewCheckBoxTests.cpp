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
};
