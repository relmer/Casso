#include "Pch.h"

#include "Ui/Settings/GeneralPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageTests
//
//  The General page's one option: its label, the callback a toggle reaches,
//  and the content height the sheet scrolls by.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GeneralPageTests)
{
public:

    TEST_METHOD (AutoUpdateCheckbox_HasSentenceCaseLabel)
    {
        GeneralPage  page;

        Assert::AreEqual (L"Check for updates automatically", page.GetAutoUpdateCheckbox().GetLabel().c_str());
    }


    TEST_METHOD (AutoUpdateCheckbox_ToggleReachesCallback)
    {
        GeneralPage  page;
        int          calls    = 0;
        bool         received = true;

        page.SetAutoUpdateChecked   (true);
        page.SetOnAutoUpdateToggled ([&] (bool enabled)
        {
            ++calls;
            received = enabled;
        });

        page.GetAutoUpdateCheckbox().SetFocused (true);
        page.GetAutoUpdateCheckbox().OnKey      ((WPARAM) VK_SPACE);

        Assert::AreEqual (1, calls);
        Assert::IsFalse  (received, L"unchecking reports false");
        Assert::IsFalse  (page.GetAutoUpdateCheckbox().IsChecked());
    }


    TEST_METHOD (Layout_ContentHeightIsOneRowInsideThePad)
    {
        constexpr int  kPadDip = 16;
        constexpr int  kRowDip = 28;

        GeneralPage    page;
        DxuiDpiScaler  scaler;
        RECT           rect   = { 10, 20, 730, 520 };
        RECT           row    = {};

        scaler.SetDpi (96);
        page.Layout   (rect, scaler);
        row = page.GetAutoUpdateCheckbox().GetBounds();

        Assert::AreEqual ((LONG) (rect.left + kPadDip), row.left);
        Assert::AreEqual ((LONG) (rect.top  + kPadDip), row.top);
        Assert::AreEqual (kPadDip + kRowDip + kPadDip, page.GetContentHeightPx());
    }
};
