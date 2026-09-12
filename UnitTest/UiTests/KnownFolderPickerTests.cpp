#include "Pch.h"
#include "../EhmTestHelper.h"
#include "InMemoryFileSystem.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Cassque/Model/LaunchCommand.h"
#include "Core/DxuiDpiScaler.h"
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



    static DialogImage MakePicture (float displayDp)
    {
        DialogImage  image;

        image.width     = 1;
        image.height    = 1;
        image.rgba      = { 0x10, 0x20, 0x30, 0xFF };
        image.displayDp = displayDp;

        return image;
    }


    static DxuiDpiScaler MakeScaler96()
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);   // 1:1 DIP == px

        return scaler;
    }



    TEST_METHOD (DialogStrip_CentersItsPiecesOnTheTallestPicture)
    {
        DialogBodyContent  content;
        DialogTextRun      run;
        RECT               label = {};

        //  48 + "+" (18, its floor) + an empty piece, which takes no room.
        run.strip = { { MakePicture (48.0f), L"" }, { {}, L"+" }, { {}, L"" } };

        content.SetRuns ({ run });
        content.Layout  ({ 0, 0, 400, 1000 }, MakeScaler96());
        label = content.GetChild (0)->GetBounds();

        Assert::AreEqual ((size_t) 1, content.GetChildCount());
        Assert::AreEqual (48 + 2 * 5, content.GetPreferredHeightDip());

        //  The strip is 48 + 8 + 18 wide, centered in 400; the label follows the
        //  picture and sits centered on the 58-high line.
        Assert::AreEqual ((LONG) ((400 - 74) / 2 + 48 + 8), label.left);
        Assert::AreEqual ((LONG) ((58 - 18) / 2),           label.top);
    }



    TEST_METHOD (DialogLeadingPicture_SetsItsTextBesideItOrLeavesProse)
    {
        DialogBodyContent  withPicture;
        DialogBodyContent  withoutPixels;
        DialogTextRun      run;
        RECT               label = {};

        run.text         = L"Casso";
        run.leadingImage = MakePicture (32.0f);

        withPicture.SetRuns ({ run });
        withPicture.Layout  ({ 0, 0, 400, 1000 }, MakeScaler96());
        label = withPicture.GetChild (0)->GetBounds();

        //  The label takes the whole row and centers its text in it, so the
        //  text reads level with the picture.
        Assert::AreEqual (32 + 2 * 5,      withPicture.GetPreferredHeightDip());
        Assert::AreEqual ((LONG) (32 + 8), label.left);
        Assert::AreEqual ((LONG) 0,        label.top);
        Assert::AreEqual ((LONG) 42,       label.bottom);

        run.leadingImage->rgba.clear();

        withoutPixels.SetRuns ({ run });
        withoutPixels.Layout  ({ 0, 0, 400, 1000 }, MakeScaler96());

        Assert::AreEqual (18, withoutPixels.GetPreferredHeightDip());
        Assert::AreEqual ((LONG) 0, withoutPixels.GetChild (0)->GetBounds().left);
    }



    TEST_METHOD (DialogLeadingPictures_ShareOneRowHeight)
    {
        DialogBodyContent  content;
        DialogTextRun      wrapping;
        DialogTextRun      short_;
        RECT               first  = {};
        RECT               second = {};

        //  Long enough to wrap, which would otherwise make its row taller
        //  than the next one and space the pictures unevenly.
        wrapping.text         = L"Cask, a container that stores things, and a homophone of casque, the crest atop a cassowary's head";
        wrapping.leadingImage = MakePicture (32.0f);
        short_.text           = L"Casso, a spiffy Apple II emulator";
        short_.leadingImage   = MakePicture (32.0f);

        content.SetRuns ({ wrapping, short_ });
        content.Layout  ({ 0, 0, 400, 1000 }, MakeScaler96());
        first  = content.GetChild (0)->GetBounds();
        second = content.GetChild (1)->GetBounds();

        //  Three lines at 18, the picture's padding around them, twice.
        Assert::AreEqual (2 * (3 * 18 + 2 * 5), content.GetPreferredHeightDip());
        Assert::AreEqual (first.bottom - first.top, second.bottom - second.top);
        Assert::AreEqual ((LONG) 64, second.top);
    }



    TEST_METHOD (DialogTrailingPicture_KeepsTheRunsBesideItClear)
    {
        DialogBodyContent  content;
        DialogImage        picture = MakePicture (128.0f);
        DialogTextRun      heading;
        DialogTextRun      below;

        heading.text = L"Cassque";
        below.text   = L"What's with the name?";

        content.SetImagePlacement (DialogBodyContent::ImagePlacement::TrailingBeside);
        Assert::IsTrue (content.SetImage (picture));

        //  Eight lines carry the runs past the 128-high picture.
        content.SetRuns ({ heading, {}, {}, {}, {}, {}, {}, {}, below });
        content.Layout  ({ 0, 0, 400, 1000 }, MakeScaler96());

        //  The picture takes the top right corner, so the first run stops one
        //  gap short of it; the ninth run starts below it and runs full width.
        Assert::AreEqual (9 * 18, content.GetPreferredHeightDip());
        Assert::AreEqual ((LONG) (400 - 128 - 12), content.GetChild (0)->GetBounds().right);
        Assert::AreEqual ((LONG) 400, content.GetChild (8)->GetBounds().right);
    }
};
