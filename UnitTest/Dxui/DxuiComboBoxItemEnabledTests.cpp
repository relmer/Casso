#include "Pch.h"

#include "Widgets/DxuiComboBox.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiComboBoxItemEnabledTests
//
//  An item can be listed and not chosen: shown, passed over by the keyboard,
//  and not committed by a click. A list whose flags were never set behaves
//  as it always has, and setting the items clears the flags.
//
//  Driven through the in-window menu, which needs no window to open.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTests
{
    TEST_CLASS (DxuiComboBoxItemEnabledTests)
    {
    public:

        static constexpr int  kRowHeightPx = 28;   // the list's row height at 96 DPI
        static constexpr int  kInRowPx     = 2;


        static std::vector<std::wstring> MakeItems()
        {
            return { L"Joystick", L"Joyport left (Atari)", L"Joyport right (Atari)", L"Paddle" };
        }


        TEST_METHOD (AnItemWithNoFlagCanBeChosen)
        {
            DxuiComboBox  combo;



            combo.SetItems (MakeItems());

            Assert::IsTrue (combo.IsItemEnabled (0));
            Assert::IsTrue (combo.IsItemEnabled (3));

            combo.SetItemsEnabled ({ true, false });
            Assert::IsFalse (combo.IsItemEnabled (1));
            Assert::IsTrue  (combo.IsItemEnabled (2), L"past the flags given");

            combo.SetItems (MakeItems());
            Assert::IsTrue (combo.IsItemEnabled (1), L"new items clear the flags");
        }


        TEST_METHOD (TheKeyboardPassesOverADisabledItem)
        {
            DxuiComboBox  combo;
            int           picked = -1;



            combo.SetItems        (MakeItems());
            combo.SetItemsEnabled ({ true, false, true, true });
            combo.SetSelected     (0);
            combo.SetSelect       ([&picked] (int index) { picked = index; });
            combo.SetFocused      (true);
            combo.Open();

            combo.HandleKey (VK_DOWN);
            Assert::AreEqual (2, combo.GetHighlightIndex(), L"down from Joystick skips the left jack");

            combo.HandleKey (VK_UP);
            Assert::AreEqual (0, combo.GetHighlightIndex(), L"and up skips it too");

            combo.HandleKey (VK_DOWN);
            combo.HandleKey (VK_RETURN);
            Assert::AreEqual (2, picked, L"the enabled item is chosen");
        }


        TEST_METHOD (AClickOnADisabledItemChoosesNothing)
        {
            DxuiComboBox   combo;
            DxuiDpiScaler  scaler;
            RECT           bounds = { 0, 0, 200, kRowHeightPx };
            int            picked = -1;



            combo.Layout          (bounds, scaler);
            combo.SetItems        (MakeItems());
            combo.SetItemsEnabled ({ true, false, true, true });
            combo.SetSelected     (0);
            combo.SetSelect       ([&picked] (int index) { picked = index; });
            combo.Open();

            Assert::AreEqual (1, combo.HitTestItem (kInRowPx, bounds.bottom + kRowHeightPx + kInRowPx), L"the click lands on the left jack");

            combo.OnLButtonDown (kInRowPx, bounds.bottom + kRowHeightPx + kInRowPx);
            combo.OnLButtonUp   (kInRowPx, bounds.bottom + kRowHeightPx + kInRowPx);

            Assert::AreEqual (-1, picked,                   L"which is not chosen");
            Assert::AreEqual (0,  combo.GetSelectedIndex(), L"and the selection stays");
        }
    };
}
