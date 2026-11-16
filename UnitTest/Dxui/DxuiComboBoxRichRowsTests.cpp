#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiComboBoxRichRowsTests
//
//  A list can have headings and commentary under its items. A heading is
//  passed over by the keyboard and a click on it doesn't close the list; a
//  row with commentary is taller than a plain row, and the rows below it
//  move down. Setting the items clears both.
//
//  The tests use the in-window menu, which opens without a window.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTests
{
    TEST_CLASS (DxuiComboBoxRichRowsTests)
    {
    public:

        static constexpr int  kRowHeightPx = 28;   // a plain row at 96 DPI
        static constexpr int  kInRowPx     = 2;


        static std::vector<std::wstring> MakeItems()
        {
            return { L"Automatic", L".dsk", L"Esoterica", L".nib" };
        }


        TEST_METHOD (AHeading_IsPassedOverByTheKeyboard_AndClearedWithTheItems)
        {
            DxuiComboBox  combo;
            int           picked = -1;



            combo.SetItems        (MakeItems());
            combo.SetItemHeadings ({ false, false, true });
            combo.SetSelected     (1);
            combo.SetSelect       ([&picked] (int index) { picked = index; });
            combo.SetFocused      (true);
            combo.Open();

            Assert::IsTrue (combo.IsItemHeading (2));

            combo.HandleKey (VK_DOWN);
            Assert::AreEqual (3, combo.GetHighlightIndex(), L"down from .dsk skips the heading");

            combo.HandleKey (VK_UP);
            Assert::AreEqual (1, combo.GetHighlightIndex(), L"and up skips it too");

            combo.HandleKey (VK_RETURN);
            combo.SetItems  (MakeItems());
            Assert::IsFalse (combo.IsItemHeading (2), L"new items clear the headings");
        }


        TEST_METHOD (AClickOnAHeading_ChoosesNothing_AndLeavesTheListOpen)
        {
            DxuiComboBox   combo;
            DxuiDpiScaler  scaler;
            RECT           bounds = { 0, 0, 200, kRowHeightPx };
            int            picked = -1;
            int            y      = 0;



            combo.Layout          (bounds, scaler);
            combo.SetItems        (MakeItems());
            combo.SetItemHeadings ({ false, false, true });
            combo.SetSelected     (0);
            combo.SetSelect       ([&picked] (int index) { picked = index; });
            combo.Open();

            y = bounds.bottom + 2 * kRowHeightPx + kInRowPx;
            Assert::AreEqual (2, combo.HitTestItem (kInRowPx, y), L"the click lands on the heading");

            combo.OnLButtonDown (kInRowPx, y);
            combo.OnLButtonUp   (kInRowPx, y);

            Assert::AreEqual (-1, picked);
            Assert::IsTrue (combo.IsOpen());
        }


        TEST_METHOD (ARowWithCommentary_IsTaller_AndTheRowsBelowItMoveDown)
        {
            DxuiComboBox   combo;
            DxuiDpiScaler  scaler;
            RECT           bounds = { 0, 0, 300, kRowHeightPx };
            int            plainY = bounds.bottom + kRowHeightPx + kInRowPx;



            combo.Layout              (bounds, scaler);
            combo.SetItems            (MakeItems());
            combo.SetItemDescriptions ({ L"Commentary under the first item, in a smaller font." });
            combo.Open();

            Assert::AreEqual (0, combo.HitTestItem (kInRowPx, plainY), L"the first row is taller than a plain row");
            Assert::IsTrue (combo.GetInWindowMenuRect().bottom - bounds.bottom > (int) combo.GetItems().size() * kRowHeightPx, L"the list is taller than its plain rows");
        }
    };
}
