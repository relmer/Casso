#include "Pch.h"

#include "Shell/ScreenshotCapture.h"

#include "FakeHostDialogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HostDialogSeamTests
//
//  The folder picker through the host dialogs seam, with the operating
//  system's dialog replaced by one that answers at once.
//
//  Three answers exist -- a path, a cancel, a failure -- and the caller
//  treats a cancel as keeping what it had. That rule was unreachable while
//  the only dialog was the real one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HostDialogSeamTests)
{
public:

    TEST_METHOD (APickedFolderComesBackAsTheFolder)
    {
        FakeHostDialogs  dialogs;
        fs::path         folder;

        dialogs.picks = true;
        dialogs.path  = L"C:\\Users\\Someone\\Pictures\\Shots";

        Assert::IsTrue (ScreenshotCapture::BrowseForFolder (dialogs, nullptr, folder));
        Assert::AreEqual (std::wstring (L"C:\\Users\\Someone\\Pictures\\Shots"), folder.wstring());
        Assert::AreEqual (1, dialogs.folders, L"the folder picker, not a file picker");
        Assert::AreEqual (0, dialogs.opens + dialogs.saves);
    }


    TEST_METHOD (BackingOutKeepsWhatWasConfigured)
    {
        FakeHostDialogs  dialogs;
        fs::path         folder = L"D:\\Existing";

        dialogs.picks = false;

        Assert::IsFalse (ScreenshotCapture::BrowseForFolder (dialogs, nullptr, folder));
        Assert::AreEqual (std::wstring (L"D:\\Existing"), folder.wstring(), L"a cancel changes nothing");
    }


    TEST_METHOD (AFailedPickerReadsAsNoFolderPicked)
    {
        FakeHostDialogs  dialogs;
        fs::path         folder;

        dialogs.failure = E_FAIL;

        Assert::IsFalse (ScreenshotCapture::BrowseForFolder (dialogs, nullptr, folder));
        Assert::IsTrue (folder.empty());
    }
};
