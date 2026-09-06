#include "Pch.h"

#include "Ui/Settings/ScreenshotsPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPageTests
//
//  The capture-mode mapping between the stored token and the radio's index.
//
//  This is the whole of the page's own logic; everything else on it is widget
//  plumbing that Dxui tests for itself. The mapping earns coverage because it
//  is the seam where a stored setting can be silently repointed: the enum's
//  declaration order and the radios' display order are two different things
//  that agree today, and the day one of them changes, a cast between them
//  would still compile and would quietly give every existing user a different
//  screenshot mode.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ScreenshotsPageTests)
{
public:

    TEST_METHOD (CaptureModeToIndex_MapsEachToken)
    {
        Assert::AreEqual (0, ScreenshotsPage::CaptureModeToIndex ("scene"));
        Assert::AreEqual (1, ScreenshotsPage::CaptureModeToIndex ("crt"));
        Assert::AreEqual (2, ScreenshotsPage::CaptureModeToIndex ("raw"));
    }


    TEST_METHOD (IndexToCaptureMode_MapsEachIndex)
    {
        Assert::AreEqual ("scene", ScreenshotsPage::IndexToCaptureMode (0));
        Assert::AreEqual ("crt",   ScreenshotsPage::IndexToCaptureMode (1));
        Assert::AreEqual ("raw",   ScreenshotsPage::IndexToCaptureMode (2));
    }


    TEST_METHOD (TheMappingRoundTripsBothWays)
    {
        const char *  tokens[] = { "scene", "crt", "raw" };
        int           i        = 0;

        for (i = 0; i < (int) std::size (tokens); i++)
        {
            int   index = ScreenshotsPage::CaptureModeToIndex (tokens[i]);

            Assert::AreEqual (i, index);
            Assert::AreEqual (tokens[i], ScreenshotsPage::IndexToCaptureMode (index));
        }
    }


    // Scene is index 0 because the radios are ordered default-first. If that
    // ever stops being true the default option stops being the top one.
    TEST_METHOD (SceneIsFirstBecauseTheDefaultLeadsTheGroup)
    {
        Assert::AreEqual (0, ScreenshotsPage::CaptureModeToIndex ("scene"));
        Assert::AreEqual ("scene", ScreenshotsPage::IndexToCaptureMode (0));
    }


    // A token this build does not know reads as the default rather than
    // leaving the group with nothing selected -- the same rule the prefs
    // loader follows, so the page and the setting agree about an unknown value.
    TEST_METHOD (AnUnknownTokenSelectsTheDefault)
    {
        Assert::AreEqual (0, ScreenshotsPage::CaptureModeToIndex ("hologram"));
        Assert::AreEqual (0, ScreenshotsPage::CaptureModeToIndex (""));
    }


    // Likewise an index outside the set: writing back a token nothing can
    // parse would be worse than writing the default.
    TEST_METHOD (AnOutOfRangeIndexWritesTheDefaultToken)
    {
        Assert::AreEqual ("scene", ScreenshotsPage::IndexToCaptureMode (-1));
        Assert::AreEqual ("scene", ScreenshotsPage::IndexToCaptureMode (3));
        Assert::AreEqual ("scene", ScreenshotsPage::IndexToCaptureMode (99));
    }


    //
    //  The folder row's text
    //
    //  It resolves WHICH path to show and nothing else. Fitting that path to
    //  the row is measured in pixels by the link at paint time, so there is no
    //  character budget here to test -- and deliberately so: a second answer
    //  to "does this fit" would be the wrong one, characters being off by up
    //  to a factor of three across a proportional face.
    //

    // An unset preference means "the default", and the row shows the default's
    // real path. Blank would read as broken when the feature is working, and
    // the user could not tell where their files go without leaving the page.
    TEST_METHOD (AnUnsetFolderShowsTheDefaultPath)
    {
        Assert::AreEqual (std::wstring (L"C:\\Pics\\Casso Screenshots"),
                          ScreenshotsPage::FolderForDisplay ("", L"C:\\Pics\\Casso Screenshots"));
    }


    TEST_METHOD (AConfiguredFolderIsShownInsteadOfTheDefault)
    {
        Assert::AreEqual (std::wstring (L"D:\\Shots"),
                          ScreenshotsPage::FolderForDisplay ("D:\\Shots", L"C:\\Pics\\Casso Screenshots"));
    }


    // Whole, however long. The link trims it to its own box; truncating here
    // as well would fight that with a worse rule.
    TEST_METHOD (ALongPathIsReturnedWhole)
    {
        std::wstring   shown = ScreenshotsPage::FolderForDisplay (
            "C:\\Users\\somebody\\OneDrive\\Documents\\Emulation\\Captures\\Casso", L"");

        Assert::AreEqual (std::wstring (L"C:\\Users\\somebody\\OneDrive\\Documents\\Emulation\\Captures\\Casso"),
                          shown);
        Assert::IsTrue (shown.find (L"\x2026") == std::wstring::npos, L"no ellipsis is added here");
    }


    TEST_METHOD (ThePageIsNamedForItsSubject)
    {
        ScreenshotsPage   page;

        Assert::AreEqual (std::wstring (L"Screenshots"), page.GetTitle());
    }
};
