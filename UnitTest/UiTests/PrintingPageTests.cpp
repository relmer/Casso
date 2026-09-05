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
