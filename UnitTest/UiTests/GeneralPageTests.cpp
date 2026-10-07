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


    TEST_METHOD (Layout_ContentEndsAtTheFolderButtonInsideThePad)
    {
        constexpr int  kPadDip = 16;

        GeneralPage    page;
        DxuiDpiScaler  scaler;
        RECT           rect   = { 10, 20, 730, 520 };
        RECT           row    = {};
        RECT           folder = {};

        scaler.SetDpi (96);
        page.Layout   (rect, scaler);
        row    = page.GetAutoUpdateCheckbox().GetBounds();
        folder = page.GetOpenFolderButton().GetBounds();

        Assert::AreEqual ((LONG) (rect.left + kPadDip), row.left);
        Assert::IsTrue   (row.top > rect.top + kPadDip, L"the Updates heading sits above the checkbox");
        Assert::AreEqual ((int) (folder.bottom + kPadDip - rect.top), page.GetContentHeightPx());
    }


    TEST_METHOD (SkippedRow_HiddenUntilSetAndAddsARow)
    {
        constexpr int  kRowStepDip = 34;

        GeneralPage    page;
        DxuiDpiScaler  scaler;
        RECT           rect   = { 0, 0, 720, 600 };
        int            before = 0;

        scaler.SetDpi (96);
        page.Layout   (rect, scaler);
        before = page.GetContentHeightPx();

        Assert::IsFalse (page.GetSkippedLabel().IsVisible());
        Assert::IsFalse (page.GetStopSkippingButton().IsVisible());

        // No Layout call here: setting the text lays the page out again.
        page.SetSkippedText (L"Skipped version: 1.31.91");

        Assert::IsTrue   (page.GetSkippedLabel().IsVisible());
        Assert::IsTrue   (page.GetStopSkippingButton().IsVisible());
        Assert::AreEqual (before + kRowStepDip, page.GetContentHeightPx());

        page.SetSkippedText (L"");

        Assert::IsFalse (page.GetSkippedLabel().IsVisible());
    }


    TEST_METHOD (Labels_AreSentenceCase)
    {
        GeneralPage  page;

        Assert::AreEqual (L"Offer to download disk drive sounds", page.GetAudioOfferCheckbox().GetLabel().c_str());
        Assert::AreEqual (L"Offer updated ROMs",                  page.GetRomOfferCheckbox().GetLabel().c_str());
        Assert::AreEqual (L"Never checked.",                      page.GetLastCheckedLabel().GetText().c_str());
    }


    TEST_METHOD (Buttons_ReachTheirCallbacks)
    {
        GeneralPage  page;
        int          checks  = 0;
        int          unskips = 0;
        int          folders = 0;

        page.SetOnCheckNow     ([&] () { ++checks;  });
        page.SetOnStopSkipping ([&] () { ++unskips; });
        page.SetOnOpenFolder   ([&] () { ++folders; });
        page.SetSkippedText    (L"Skipped version: 1.31.91");

        page.GetCheckNowButton().Click();
        page.GetStopSkippingButton().Click();
        page.GetOpenFolderButton().Click();

        Assert::AreEqual (1, checks);
        Assert::AreEqual (1, unskips);
        Assert::AreEqual (1, folders);
    }


    TEST_METHOD (OfferCheckboxes_ReachTheirCallbacks)
    {
        GeneralPage  page;
        int          audioCalls = 0;
        bool         romValue   = false;

        page.SetAudioOfferChecked   (true);
        page.SetRomOfferChecked     (false);
        page.SetOnAudioOfferToggled ([&] (bool) { ++audioCalls; });
        page.SetOnRomOfferToggled   ([&] (bool checked) { romValue = checked; });

        page.GetAudioOfferCheckbox().SetFocused (true);
        page.GetAudioOfferCheckbox().OnKey      ((WPARAM) VK_SPACE);
        page.GetRomOfferCheckbox().SetFocused   (true);
        page.GetRomOfferCheckbox().OnKey        ((WPARAM) VK_SPACE);

        Assert::AreEqual (1, audioCalls);
        Assert::IsTrue   (romValue, L"checking the ROM offer reports true");
    }
};
