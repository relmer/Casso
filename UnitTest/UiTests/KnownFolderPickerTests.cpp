#include "Pch.h"
#include "../EhmTestHelper.h"
#include "InMemoryFileSystem.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Cassque/Model/LaunchCommand.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "../Cassque/FakeProcessLauncher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KnownFolderPickerTests
//
//  What the emulator's side of the browser decides: which folders its disk
//  picker scans, how it starts the browser, and the picture a dialog can
//  carry.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KnownFolderPickerTests)
{
public:

    static constexpr const wchar_t *  kBase = L"C:\\Users\\me\\AppData\\Local\\Casso";



    static std::vector<DiskMru::Entry> MakeMru()
    {
        std::vector<DiskMru::Entry>  mru;

        mru.push_back (DiskMru::Entry { L"C:\\Recent\\one.dsk",  10 });
        mru.push_back (DiskMru::Entry { L"D:\\Shared\\two.woz",  11 });
        mru.push_back (DiskMru::Entry { L"C:\\Recent\\three.po", 12 });

        return mru;
    }



    TEST_METHOD (PickerFolders_AreTheKnownFoldersThenTheRecentOnes)
    {
        InMemoryFileSystem                    fs;
        KnownFolderStore                      store (fs, kBase, false);
        std::vector<KnownFolderStore::Entry>  known;
        std::vector<std::filesystem::path>    folders;

        AssertSucceeded (store.Append (L"E:\\Known", 1));
        AssertSucceeded (store.Append (L"d:\\shared", 2));
        AssertSucceeded (store.Load (known));

        folders = KnownFolderStore::MergePickerFolders (known, MakeMru());

        Assert::AreEqual ((size_t) 3, folders.size());
        Assert::AreEqual (std::wstring (L"E:\\Known"),  folders[0].wstring());
        Assert::AreEqual (std::wstring (L"d:\\shared"), folders[1].wstring(), L"a folder in both lists appears once, where the known list put it");
        Assert::AreEqual (std::wstring (L"C:\\Recent"), folders[2].wstring());
    }



    TEST_METHOD (PickerFolders_WithNoKnownListAreTheRecentOnes)
    {
        std::vector<std::filesystem::path>  folders = KnownFolderStore::MergePickerFolders ({}, MakeMru());

        Assert::AreEqual ((size_t) 2, folders.size());
        Assert::AreEqual (std::wstring (L"C:\\Recent"), folders[0].wstring());
        Assert::AreEqual (std::wstring (L"D:\\Shared"), folders[1].wstring());
    }



    TEST_METHOD (Launch_CassqueBesideTheModuleWithItsOwner)
    {
        FakeProcessLauncher     launcher;
        LaunchCommand::Outcome  outcome = LaunchCommand::Outcome::Failed;

        launcher.present.push_back (L"C:\\Casso\\x64\\Release\\Cassque.exe");

        outcome = LaunchCommand::LaunchCassque (launcher, L"C:\\Casso\\x64\\Release", reinterpret_cast<HWND> (0x1234), L"my worktree");

        Assert::IsTrue   (outcome == LaunchCommand::Outcome::Launched);
        Assert::AreEqual ((size_t) 1, launcher.launched.size());
        Assert::AreEqual (std::wstring (L"C:\\Casso\\x64\\Release\\Cassque.exe"), launcher.launched[0].exePath);
        Assert::AreEqual (std::wstring (L"--owner 4660 --title \"my worktree\""), launcher.launched[0].arguments);
    }



    TEST_METHOD (Launch_AMissingBrowserIsReportedAndNothingStarts)
    {
        FakeProcessLauncher     launcher;
        LaunchCommand::Outcome  outcome = LaunchCommand::LaunchCassque (launcher, L"C:\\Casso", nullptr, L"");

        Assert::IsTrue   (outcome == LaunchCommand::Outcome::Missing);
        Assert::IsTrue   (launcher.launched.empty());
        Assert::IsTrue   (LaunchCommand::DescribeMissing (L"C:\\Casso\\Cassque.exe").find (L"C:\\Casso\\Cassque.exe") != std::wstring::npos);
    }



    TEST_METHOD (Launch_CassoArgumentsAlwaysQuoteThePath)
    {
        Assert::AreEqual (std::wstring (L"--disk1 \"C:\\disks\\game.dsk\""),
                          LaunchCommand::MakeCassoArguments (L"C:\\disks\\game.dsk", L""));
        Assert::AreEqual (std::wstring (L"--disk1 \"C:\\my disks\\game.dsk\" --title tag"),
                          LaunchCommand::MakeCassoArguments (L"C:\\my disks\\game.dsk", L"tag"));
        Assert::AreEqual (std::wstring (L"\"a \\\"b\\\\\""), LaunchCommand::QuoteArgument (L"a \"b\\"));
    }



    TEST_METHOD (DialogImage_ConvertsToPremultipliedBgraOrRefusesAMismatch)
    {
        DialogImage            image;
        std::vector<uint32_t>  pixels;

        image.width  = 2;
        image.height = 1;
        image.rgba   = { 0xFF, 0x80, 0x00, 0xFF,     // opaque orange
                         0xFF, 0xFF, 0xFF, 0x80 };   // half-transparent white

        pixels = DialogBodyContent::ToPremultipliedBgra (image);

        Assert::AreEqual ((size_t) 2, pixels.size());
        Assert::AreEqual ((uint32_t) 0xFFFF8000, pixels[0]);
        Assert::AreEqual ((uint32_t) 0x80808080, pixels[1]);

        image.rgba.pop_back();

        Assert::IsTrue (DialogBodyContent::ToPremultipliedBgra (image).empty());
    }
};
