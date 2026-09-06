#include "Pch.h"

#include "Ui/Settings/PrintingPage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPageTests
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

TEST_CLASS (PrintingPageTests)
{
public:

    TEST_METHOD (CaptureModeToIndex_MapsEachToken)
    {
        Assert::AreEqual (0, PrintingPage::CaptureModeToIndex ("scene"));
        Assert::AreEqual (1, PrintingPage::CaptureModeToIndex ("crt"));
        Assert::AreEqual (2, PrintingPage::CaptureModeToIndex ("raw"));
    }


    TEST_METHOD (IndexToCaptureMode_MapsEachIndex)
    {
        Assert::AreEqual ("scene", PrintingPage::IndexToCaptureMode (0));
        Assert::AreEqual ("crt",   PrintingPage::IndexToCaptureMode (1));
        Assert::AreEqual ("raw",   PrintingPage::IndexToCaptureMode (2));
    }


    TEST_METHOD (TheMappingRoundTripsBothWays)
    {
        const char *  tokens[] = { "scene", "crt", "raw" };
        int           i        = 0;

        for (i = 0; i < (int) std::size (tokens); i++)
        {
            int   index = PrintingPage::CaptureModeToIndex (tokens[i]);

            Assert::AreEqual (i, index);
            Assert::AreEqual (tokens[i], PrintingPage::IndexToCaptureMode (index));
        }
    }


    // Scene is index 0 because the radios are ordered default-first. If that
    // ever stops being true the default option stops being the top one.
    TEST_METHOD (SceneIsFirstBecauseTheDefaultLeadsTheGroup)
    {
        Assert::AreEqual (0, PrintingPage::CaptureModeToIndex ("scene"));
        Assert::AreEqual ("scene", PrintingPage::IndexToCaptureMode (0));
    }


    // A token this build does not know reads as the default rather than
    // leaving the group with nothing selected -- the same rule the prefs
    // loader follows, so the page and the setting agree about an unknown value.
    TEST_METHOD (AnUnknownTokenSelectsTheDefault)
    {
        Assert::AreEqual (0, PrintingPage::CaptureModeToIndex ("hologram"));
        Assert::AreEqual (0, PrintingPage::CaptureModeToIndex (""));
    }


    // Likewise an index outside the set: writing back a token nothing can
    // parse would be worse than writing the default.
    TEST_METHOD (AnOutOfRangeIndexWritesTheDefaultToken)
    {
        Assert::AreEqual ("scene", PrintingPage::IndexToCaptureMode (-1));
        Assert::AreEqual ("scene", PrintingPage::IndexToCaptureMode (3));
        Assert::AreEqual ("scene", PrintingPage::IndexToCaptureMode (99));
    }


    //
    //  The folder row's text
    //

    // An unset preference means "the default", and the row shows the default's
    // real path. Blank would read as broken when the feature is working, and
    // the user could not tell where their files go without leaving the page.
    TEST_METHOD (AnUnsetFolderShowsTheDefaultPath)
    {
        Assert::AreEqual (std::wstring (L"C:\\Pics\\Casso Screenshots"),
                          PrintingPage::FolderForDisplay ("", L"C:\\Pics\\Casso Screenshots", 60));
    }


    TEST_METHOD (AConfiguredFolderIsShownInsteadOfTheDefault)
    {
        Assert::AreEqual (std::wstring (L"D:\\Shots"),
                          PrintingPage::FolderForDisplay ("D:\\Shots", L"C:\\Pics\\Casso Screenshots", 60));
    }


    // The tail distinguishes one folder from another; the head is what every
    // path on the machine has in common, so the head is the half to lose.
    TEST_METHOD (ALongPathKeepsItsTailBehindAnEllipsis)
    {
        std::wstring   shown = PrintingPage::FolderForDisplay (
            "C:\\Users\\somebody\\OneDrive\\Documents\\Emulation\\Captures\\Casso",
            L"", 30);

        Assert::IsTrue (shown.rfind (L"...", 0) == 0, L"elided paths lead with an ellipsis");
        Assert::IsTrue (shown.find (L"Casso") != std::wstring::npos, L"the leaf survives");
        Assert::IsTrue (shown.find (L"somebody") == std::wstring::npos, L"the head is dropped");
    }


    // Cutting mid-name would produce something that looks like a real folder
    // and is not, so the ellipsis lands on a separator.
    TEST_METHOD (TheEllipsisLandsOnAComponentBoundary)
    {
        std::wstring   shown = PrintingPage::FolderForDisplay (
            "C:\\Users\\somebody\\OneDrive\\Documents\\Emulation\\Captures\\Casso",
            L"", 30);

        Assert::AreEqual (std::wstring (L"...\\Captures\\Casso"), shown);
    }


    TEST_METHOD (AShortPathIsNotElided)
    {
        std::wstring   shown = PrintingPage::FolderForDisplay ("D:\\Shots", L"", 30);

        Assert::AreEqual (std::wstring (L"D:\\Shots"), shown);
        Assert::IsTrue (shown.find (L"...") == std::wstring::npos);
    }


    // A single component longer than the budget has no boundary to cut on;
    // it still has to come back within budget rather than overflow the row.
    TEST_METHOD (ASingleOverlongComponentIsStillTrimmed)
    {
        std::wstring   shown = PrintingPage::FolderForDisplay (
            "D:\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", L"", 20);

        Assert::IsTrue (shown.length() <= 20);
        Assert::IsTrue (shown.rfind (L"...", 0) == 0);
    }


    // The page's own default title carries both subjects. Printing keeps the
    // first word so a user navigating to printer settings still recognizes it.
    TEST_METHOD (ThePageTitleNamesBothSubjectsPrintingFirst)
    {
        PrintingPage   page;
        std::wstring   title = page.GetTitle();

        Assert::IsTrue (title.find (L"Printing") != std::wstring::npos);
        Assert::IsTrue (title.find (L"Screenshots") != std::wstring::npos);
        Assert::IsTrue (title.find (L"Printing") < title.find (L"Screenshots"));
    }
};
