#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/CassoExplorerSession.h"
#include "CassoExplorer/Picker/ExplorerPickerHost.h"
#include "CassoExplorer/Picker/PickerPrefs.h"
#include "CassoExplorer/Picker/PickerRequest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerHostTests
//
//  A picker's host over an in-memory host file system, without its window:
//  the preferences file it reads and writes, the files of Casso Explorer
//  that it does not touch, and the context its window is created with.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerPickerHostTests)
{
public:

    static constexpr const wchar_t *  kBaseDir = L"C:\\Base";



    static PickerRequest MakeDriveRequest()
    {
        PickerRequest  request;



        request.form           = PickerForm::DriveWindow;
        request.title          = L"Insert disk";
        request.receivingDrive = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };
        request.viewKey        = L"Floppy525";

        return request;
    }



    //  Casso Explorer's preferences with a serial CassoLink connection and
    //  a theme, and a session file.
    static void SeedCassoExplorerFiles (InMemoryFileSystem & fs)
    {
        CassoExplorerPrefs  prefs;
        CassoLinkSettings   link;



        link.name      = "Apple";
        link.transport = LinkTransport::Serial;
        link.address   = "COM3";
        prefs.links    = { link };
        prefs.theme    = CassoExplorerPrefs::kThemeRetroTerminal;
        prefs.tabs     = { Location::MakeHostFolder (L"C:\\Elsewhere") };

        AssertSucceeded (prefs.Save (kBaseDir, fs));
        AssertSucceeded (fs.WriteAllText (CassoExplorerSession::GetFilePath (kBaseDir), "session"));
    }



    TEST_METHOD (FilePath_HasTheViewKey_WithoutPathCharacters)
    {
        Assert::AreEqual (std::wstring (L"C:\\Base\\CassoPickerPrefs-Floppy525.json"), PickerPrefs::GetFilePath (kBaseDir, L"Floppy525"));
        Assert::AreEqual (std::wstring (L"C:\\Base\\CassoPickerPrefs-updir.json"),     PickerPrefs::GetFilePath (kBaseDir, L"..\\up/dir"));
        Assert::AreEqual (std::wstring (L"C:\\Base\\CassoPickerPrefs-Default.json"),   PickerPrefs::GetFilePath (kBaseDir, L""));
    }



    TEST_METHOD (Context_HasNoLinksCardsOrRunningCassos)
    {
        InMemoryFileSystem          fs;
        FakeDiskFileIo              io;
        ExplorerPickerHost          host (fs, io, kBaseDir);
        CassoExplorerWindowContext  context;



        SeedCassoExplorerFiles (fs);

        host.Prepare (MakeDriveRequest(), CassoExplorerPrefs::kThemeDark, nullptr, false);
        context = host.MakeContext();

        Assert::IsNull (context.links);
        Assert::IsNull (context.running);
        Assert::IsNull (context.announced);
        Assert::IsNull (context.firewall);
        Assert::IsNull (context.cards);
        Assert::IsTrue (context.host == &host);
        Assert::IsTrue (context.picker == &host.GetRequest());
        Assert::IsTrue (context.outcome == &host.GetOutcome());
    }



    TEST_METHOD (DrivePicker_StartsOnTheCassoNode_InTheOpeningWindowsTheme)
    {
        InMemoryFileSystem          fs;
        FakeDiskFileIo              io;
        ExplorerPickerHost          host (fs, io, kBaseDir);
        CassoExplorerWindowContext  context;



        SeedCassoExplorerFiles (fs);

        host.Prepare (MakeDriveRequest(), CassoExplorerPrefs::kThemeDark, nullptr, false);
        context = host.MakeContext();

        Assert::AreEqual ((size_t) 1, context.request.tabs.size());
        Assert::IsTrue (context.request.tabs[0].location == Location::MakeRoot (TreeModel::kCassoRootId));

        //  Nothing of Casso Explorer's preferences: not its theme, its tabs
        //  or its connections.
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeDark), host.GetPrefs().theme);
        Assert::IsTrue (host.GetPrefs().links.empty());
        Assert::IsTrue (host.GetPrefs().tabs.empty());
    }



    TEST_METHOD (SavingTheView_WritesOnlyThePickersFile)
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        ExplorerPickerHost  host (fs, io, kBaseDir);
        std::wstring        prefsPath   = CassoExplorerPrefs::GetFilePath (kBaseDir);
        std::wstring        sessionPath = CassoExplorerSession::GetFilePath (kBaseDir);
        std::wstring        pickerPath  = PickerPrefs::GetFilePath (kBaseDir, L"Floppy525");
        std::string         prefsBefore;
        size_t              filesBefore = 0;



        SeedCassoExplorerFiles (fs);

        prefsBefore = fs.PeekContent (prefsPath);
        filesBefore = fs.FileCount();

        host.Prepare (MakeDriveRequest(), CassoExplorerPrefs::kThemeDark, nullptr, false);

        //  A window's saves of its session do nothing in a picker.
        host.SaveSession();
        Assert::IsNull (host.OpenWindow (CassoExplorerWindowRequest()));
        Assert::AreEqual (filesBefore, fs.FileCount());

        AssertSucceeded (host.SaveView());

        Assert::AreEqual (filesBefore + 1, fs.FileCount());
        Assert::IsTrue (fs.Exists (pickerPath));
        Assert::AreEqual (prefsBefore, fs.PeekContent (prefsPath));
        Assert::AreEqual (std::string ("session"), fs.PeekContent (sessionPath));
    }



    TEST_METHOD (SavedView_IsReadBack_WithoutConnectionsOrADrivePickersLocation)
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        ExplorerPickerHost  host (fs, io, kBaseDir);
        CassoExplorerPrefs  saved;
        CassoLinkSettings   link;



        link.name             = "Apple";
        link.transport        = LinkTransport::Serial;
        link.address          = "COM3";
        saved.links           = { link };
        saved.tabs            = { Location::MakeHostFolder (L"C:\\Disks") };
        saved.previewVisible  = !CassoExplorerPrefs().previewVisible;
        saved.placement.valid = true;
        saved.placement.w     = 900;
        saved.placement.h     = 500;

        AssertSucceeded (saved.SaveFile (PickerPrefs::GetFilePath (kBaseDir, L"Floppy525"), fs));

        host.Prepare (MakeDriveRequest(), CassoExplorerPrefs::kThemeLight, nullptr, false);

        Assert::AreEqual (saved.previewVisible, host.GetPrefs().previewVisible);
        Assert::AreEqual (900, host.GetPrefs().placement.w);
        Assert::IsTrue (host.GetPrefs().links.empty());
        Assert::IsTrue (host.GetPrefs().tabs.empty());
    }



    TEST_METHOD (RequestWithoutADrive_StartsOnItsFolder_ElseTheFolderLastPickedFrom)
    {
        PickerRequest       request;
        CassoExplorerPrefs  prefs;
        Location            location;



        Assert::IsFalse (ExplorerPickerHost::TryGetStartLocation (request, prefs, location));

        prefs.tabs = { Location::MakeHostFolder (L"C:\\Last") };
        Assert::IsTrue (ExplorerPickerHost::TryGetStartLocation (request, prefs, location));
        Assert::IsTrue (location == Location::MakeHostFolder (L"C:\\Last"));

        request.initialFolder = L"C:\\Requested";
        Assert::IsTrue (ExplorerPickerHost::TryGetStartLocation (request, prefs, location));
        Assert::IsTrue (location == Location::MakeHostFolder (L"C:\\Requested"));
    }
};
