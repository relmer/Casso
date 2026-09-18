#include "Pch.h"

#include "Widgets/DxuiComboBox.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiComboBoxScrollTests
//
//  A long list opens at most kMaxVisibleRows tall and scrolls. A controller
//  that reports 128 buttons was the case that found it: the list ran off the
//  bottom of the screen with no way to reach most of it.
//
//  Driven through the in-window menu, which shares the scroll with the popup
//  and needs no window to open.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTests
{
    TEST_CLASS (DxuiComboBoxScrollTests)
    {
    public:

        static std::vector<std::wstring> MakeItems (int count)
        {
            std::vector<std::wstring>  items;

            for (int i = 0; i < count; i++)
            {
                items.push_back (std::format (L"Item {}", i));
            }

            return items;
        }


        TEST_METHOD (AShortListShowsEveryRow)
        {
            DxuiComboBox  combo;

            combo.SetItems (MakeItems (5));

            Assert::AreEqual (5, combo.GetVisibleRowCount());
        }


        TEST_METHOD (ALongListIsCappedAndScrollsWithinItsEnds)
        {
            DxuiComboBox  combo;

            combo.SetItems (MakeItems (128));

            Assert::AreEqual (DxuiComboBox::kMaxVisibleRows, combo.GetVisibleRowCount());

            combo.ScrollBy (1000);
            Assert::AreEqual (128 - DxuiComboBox::kMaxVisibleRows, combo.GetScrollTop(), L"it stops with the last row at the bottom");

            combo.ScrollBy (-1000);
            Assert::AreEqual (0, combo.GetScrollTop(), L"and with the first row at the top");
        }


        TEST_METHOD (OpeningShowsTheSelectedRow)
        {
            DxuiComboBox  combo;

            combo.SetItems    (MakeItems (128));
            combo.SetSelected (100);
            combo.Open();

            Assert::IsTrue (combo.GetScrollTop() <= 100 && 100 < combo.GetScrollTop() + DxuiComboBox::kMaxVisibleRows,
                L"a selection far down the list is in view when it opens");
        }


        TEST_METHOD (TheKeyboardScrollsTheHighlightIntoView)
        {
            DxuiComboBox  combo;
            int           i = 0;

            combo.SetItems (MakeItems (40));
            combo.SetFocused (true);
            combo.Open();

            for (i = 0; i < 20; i++)
            {
                combo.HandleKey (VK_DOWN);
            }

            Assert::IsTrue (combo.GetScrollTop() <= combo.GetHighlightIndex() &&
                            combo.GetHighlightIndex() < combo.GetScrollTop() + DxuiComboBox::kMaxVisibleRows,
                L"moving the highlight past the bottom edge scrolls the list with it");
        }


        TEST_METHOD (AClickLandsOnTheScrolledRow)
        {
            DxuiComboBox  combo;
            DxuiDpiScaler scaler;
            RECT          bounds = { 0, 0, 200, 28 };
            int           picked = -1;

            combo.Layout    (bounds, scaler);
            combo.SetItems  (MakeItems (40));
            combo.SetSelect ([&picked] (int index) { picked = index; });
            combo.Open();
            combo.ScrollBy (10);

            Assert::AreEqual (10, combo.HitTestItem (5, bounds.bottom + 2),
                L"the first row shown is the tenth item, not the first");
        }
    };
}
