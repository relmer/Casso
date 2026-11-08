#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../CaptureTests/FakeHostDialogs.h"
#include "../CassoLink/FakeApple.h"
#include "../CassoLink/InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "FakeExplorerWindow.h"
#include "FakeFolderWatcher.h"
#include "RecordingExplorerServices.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Window/ExplorerWindowParts.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowPartsTests
//
//  The Casso Explorer window's creation, private messages and timers, run
//  through ExplorerWindowParts with no window: what the window sets up when
//  it opens, and where each of its messages goes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerWindowPartsTests)
{
public:

    struct Rig
    {
        InMemoryFileSystem                       fs;
        FakeDiskFileIo                           io;
        CassoExplorerBrowser                     browser { fs, io };
        CassoExplorerActions                     actions { browser, fs };
        CassoExplorerPrefs                       prefs;
        CassoExplorerWindowContext               app;
        FakeFolderWatcher                        watcher;
        FakeApple                                apple;
        FakeAppleFactory                         factory { apple };
        InMemoryCassoLinkFolder                  folder;
        FakeDiskFileIo                           linkIo;
        std::unique_ptr<CassoLinkConnections>    links;
        DxuiDpiScaler                            scaler;
        const DxuiTheme                        * theme   = nullptr;
        FakeExplorerWindow                       window;
        RecordingShellVerbs                    * verbs   = nullptr;
        RecordingInfoTips                      * tips    = nullptr;
        BlankShellIcons                        * icons   = nullptr;
        std::unique_ptr<ExplorerWindowParts>     parts;

        Rig()
        {
            CassoLinkSettings  settings;
            ExplorerServices   services;



            settings.name      = "Test";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";
            prefs.links        = { settings };
            prefs.typedPaths   = { L"C:\\Disks", L"C:\\Games" };

            links = std::make_unique<CassoLinkConnections> (factory, folder, linkIo);

            app.fs      = &fs;
            app.watcher = &watcher;
            app.links   = links.get();

            services.dialogs    = std::make_unique<FakeHostDialogs>();
            services.shellIcons = std::make_unique<BlankShellIcons>();
            services.infoTips   = std::make_unique<RecordingInfoTips>();
            services.shellVerbs = std::make_unique<RecordingShellVerbs>();

            icons = static_cast<BlankShellIcons *>     (services.shellIcons.get());
            tips  = static_cast<RecordingInfoTips *>   (services.infoTips.get());
            verbs = static_cast<RecordingShellVerbs *> (services.shellVerbs.get());

            parts = std::make_unique<ExplorerWindowParts> (window, browser, actions, prefs, app, scaler, theme, std::move (services));
        }

        ~Rig()
        {
            parts.reset();
            links.reset();
        }

        //  A fixture image at a path on the in-memory host.
        void  Seed (const char * fixture, const wchar_t * path)
        {
            FixtureProvider    fixtures;
            std::vector<Byte>  bytes;
            std::string        narrow;



            AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));

            for (const wchar_t * p = path; *p != L'\0'; p++)
            {
                narrow.push_back ((char) *p);
            }

            io.files[narrow]  = bytes;
            io.stamps[narrow] = FileStamp { bytes.size(), 100 };
        }

        //  What the window runs as it opens, in order, then its first layout.
        void  Open()
        {
            parts->Prepare();
            parts->CreateControls();
            parts->Configure();
            parts->layout.SetClient (RECT { 0, 0, 1280, 800 });
            parts->layout.RecomputeLayout();
        }
    };



    TEST_METHOD (Open_ListsTheSavedConnectionsAsRoots)
    {
        Rig  rig;



        rig.Open();

        Assert::AreEqual ((size_t) 1, rig.browser.GetTreeModel().GetLinks().size());
        Assert::AreEqual (std::wstring (L"Test"), rig.browser.GetTreeModel().GetLinks()[0].name);
    }



    TEST_METHOD (Open_TakesTheTypedPathsFromThePrefs)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.browser.GetTypedPaths().GetEntries() == rig.prefs.typedPaths);
    }



    TEST_METHOD (Open_StartsTheServicesWithTheWindowsMessages)
    {
        Rig  rig;



        rig.Open();

        Assert::AreEqual ((UINT) AppMessage::ExplorerRecycleBin, rig.verbs->recycleBinMessage);
        Assert::AreEqual ((UINT) AppMessage::ExplorerInfoTip,    rig.tips->startMessage);
        Assert::IsTrue   (rig.icons->sizePx > 0);
    }



    TEST_METHOD (Open_StartsTheFolderWatch)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.parts->folderSync.IsWatching());
    }



    TEST_METHOD (Open_CreatesTheControlsAsChildren)
    {
        Rig  rig;



        rig.Open();

        Assert::IsTrue (rig.window.panel.GetChildCount() > 10);
    }



    TEST_METHOD (HandleAppMessage_TakesEveryExplorerMessageButALaunch)
    {
        Rig  rig;



        rig.Open();

        for (AppMessage message : { AppMessage::ExplorerFolderChanged, AppMessage::ExplorerShellListed, AppMessage::ExplorerCassoLink,
                                    AppMessage::ExplorerInfoTip, AppMessage::ExplorerContentDetails, AppMessage::ExplorerIconsLoaded,
                                    AppMessage::ExplorerNewChoices, AppMessage::ExplorerReply, AppMessage::ExplorerRecycleBin })
        {
            Assert::IsTrue (rig.parts->HandleAppMessage ((UINT) message, 0, 0));
        }

        Assert::IsFalse (rig.parts->HandleAppMessage ((UINT) AppMessage::ExplorerLaunch, 0, 0));
        Assert::IsFalse (rig.parts->HandleAppMessage (WM_APP + 0x7F, 0, 0));
    }



    TEST_METHOD (HandleAppMessage_RunCommandDispatchesTheCommand)
    {
        Rig  rig;



        rig.Open();
        rig.parts->HandleAppMessage ((UINT) AppMessage::ExplorerRunCommand, CassoExplorerCommands::kExit, 0);

        Assert::IsTrue (rig.window.closed);
    }



    TEST_METHOD (CtrlV_InADiskImage_RunsPaste)
    {
        Rig           rig;
        DxuiKeyEvent  key;
        int           checks = 0;



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks\\dos33.dsk"));
        Assert::IsTrue (rig.browser.IsVolumeLocation());

        rig.parts->input.SetFocusPane (ExplorerPane::List);
        checks   = rig.verbs->clipboardChecks;
        key.kind = DxuiKeyEventKind::Down;
        key.vk   = 'V';
        key.ctrl = true;

        Assert::IsTrue (rig.parts->listPane.HandleShortcutKey (key));
        Assert::IsTrue (rig.verbs->clipboardChecks > checks, L"Paste asks whether the clipboard has files");
    }



    //  A VAR file with A = 1, N% = -2, S$ = "HI", FN F, B%(2) and C(1,2),
    //  whose copy of memory starts at $95AE and ends at HIMEM, $9600.
    static std::vector<Byte> BuildVariablesFile()
    {
        std::vector<Byte>  file =
        {
            80, 0, 28, 0, 0x96,
            0x41, 0x00,  0x81, 0x00, 0x00, 0x00, 0x00,
            0xCE, 0x80,  0xFF, 0xFE, 0x00, 0x00, 0x00,
            0x53, 0x80,  0x02, 0xFE, 0x95, 0x00, 0x00,
            0xC6, 0x00,  0x01, 0x08, 0x10, 0x90, 0xB0,
            0xC2, 0x80,  13, 0,  1,  0x00, 0x03,  0x00, 0x01,  0x00, 0x02,  0x00, 0x03,
            0x43, 0x00,  39, 0,  2,  0x00, 0x03,  0x00, 0x02,
            0x00, 0x00, 0x00, 0x00, 0x00,  0x81, 0x00, 0x00, 0x00, 0x00,  0x82, 0x00, 0x00, 0x00, 0x00,
            0x82, 0x40, 0x00, 0x00, 0x00,  0x83, 0x00, 0x00, 0x00, 0x00,  0x83, 0x20, 0x00, 0x00, 0x00,
            0x48, 0x49,
        };

        file.resize (file.size() + 256);

        return file;
    }



    //  The preview's search box, clicked and typed into through the window's
    //  input handlers, filters a VAR file's variables.
    TEST_METHOD (VarPreview_SearchBox_TakesAClickAndFiltersAsTyped)
    {
        Rig                            rig;
        std::vector<Byte>              bytes = BuildVariablesFile();
        RECT                           box   = {};
        DxuiMouseEvent                 mouse;
        DxuiKeyEvent                   key;
        int                            all   = 0;
        std::vector<std::wstring>      names;



        rig.Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Drop\\SAVED#FD95AE", std::string (bytes.begin(), bytes.end())));
        rig.io.files["C:\\Drop\\SAVED#FD95AE"]  = bytes;
        rig.io.stamps["C:\\Drop\\SAVED#FD95AE"] = FileStamp { bytes.size(), 100 };
        rig.Open();

        //  The name's suffix sets the file's type and aux type, as in CiderPress.
        Assert::IsTrue (rig.browser.GetOperations().Put ("C:\\Disks\\prodos.po", "C:\\Drop\\SAVED#FD95AE", std::string(), std::string(), false, 0,
                                                         DiskOperations::Encoding::Verbatim).Succeeded());

        rig.prefs.previewVisible = true;
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks\\prodos.po"));
        rig.parts->listPane.FillList();

        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"SAVED"));

        rig.parts->preview.FillPreview();
        rig.parts->layout.RecomputeLayout();

        Assert::IsTrue (rig.parts->preview.GetListing().IsVariables(), L"A VAR file's preview is its variables");

        all = rig.parts->preview.GetListing().GetList()->GetRowCount();

        for (int row = 0; row < all; row++)
        {
            names.push_back (rig.parts->preview.GetListing().GetVariableNameAt (row));
        }

        Assert::IsTrue (names == std::vector<std::wstring> { L"A", L"B%(2)", L"C(1,2)", L"FN F", L"N%", L"S$" },
                        L"The arrays, closed, sort in among the simple variables by name");
        box = rig.parts->preview.GetFind().GetSearchBox()->GetRect();
        Assert::IsTrue (box.right > box.left, L"The search box is on the preview's toolbar");

        mouse.kind        = DxuiMouseEventKind::Down;
        mouse.button      = DxuiMouseButton::Left;
        mouse.positionDip = { (box.left + box.right) / 2, (box.top + box.bottom) / 2 };
        rig.parts->input.HandleMouse (mouse);
        mouse.kind = DxuiMouseEventKind::Up;
        rig.parts->input.HandleMouse (mouse);

        Assert::IsTrue (rig.parts->input.GetFocusPane() == ExplorerPane::Search, L"A click in the search box gives it focus");

        for (wchar_t ch : std::wstring (L"hi"))
        {
            key.kind = DxuiKeyEventKind::Char;
            key.vk   = ch;
            rig.parts->input.HandleKey (key);
        }

        Assert::AreEqual (std::wstring (L"hi"), rig.parts->preview.GetFind().GetSearchBox()->GetText());
        Assert::IsTrue   (rig.parts->preview.GetListing().GetList()->GetRowCount() < all, L"The rows are filtered as the search is typed");
        Assert::AreEqual (1, rig.parts->preview.GetListing().GetList()->GetRowCount(), L"Only S$, whose value has HI in it");

        key.kind = DxuiKeyEventKind::Down;
        key.vk   = VK_ESCAPE;
        rig.parts->input.HandleKey (key);

        Assert::AreEqual (all, rig.parts->preview.GetListing().GetList()->GetRowCount(), L"Escape clears the search and every row is back");
    }



    //  Only a folder of Casso's whose label is its name gets its path as its
    //  tip: not one labeled with its path, nor a folder or image below one.
    TEST_METHOD (TreeTip_IsTheFullPathOfCassosFoldersShownByName)
    {
        Rig                        rig;
        std::vector<DxuiTreeNode>  roots;
        std::vector<DxuiTreeNode>  folders;
        std::wstring               disksId;
        std::wstring               tipOfSub;
        std::wstring               tipOfImage;
        std::wstring               tipOfDisks;
        std::wstring               tipOfShared;
        std::wstring               tipOfPath = L"unset";



        rig.Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\Sub\\note.txt", "x"));
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\A\\games\\a.txt", "x"));
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\B\\games\\b.txt", "x"));
        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Music\\c.txt", "x"));
        AssertSucceeded (rig.fs.WriteAllText (L"D:\\Music\\d.txt", "x"));
        rig.Open();

        rig.browser.GetTreeModel().SetKnownFolders ({ L"C:\\Disks", L"C:\\A\\games", L"C:\\B\\games", L"C:\\Music", L"D:\\Music" });
        rig.browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring &) { return true; });
        rig.browser.GetTreeModel().InvalidateAll();
        rig.browser.GetTreeRoots (roots);

        for (const DxuiTreeNode & root : roots)
        {
            if (root.id == TreeModel::kCassoRootId)
            {
                folders = rig.browser.GetTreeChildren (root.id);
            }
        }

        for (const DxuiTreeNode & folder : folders)
        {
            if (folder.label == L"Disks")
            {
                disksId    = folder.id;
                tipOfDisks = rig.parts->treePane.GetFullPathTip (folder);
            }
            else if (folder.label == L"A\\games")
            {
                tipOfShared = rig.parts->treePane.GetFullPathTip (folder);
            }
            else if (folder.label == L"C:\\Music")
            {
                tipOfPath = rig.parts->treePane.GetFullPathTip (folder);
            }
        }

        Assert::IsFalse (disksId.empty(), L"Casso has the Disks folder");

        for (const DxuiTreeNode & child : rig.browser.GetTreeChildren (disksId))
        {
            if (child.label == L"Sub")
            {
                tipOfSub = rig.parts->treePane.GetFullPathTip (child);
            }
            else if (child.label == L"prodos.po")
            {
                tipOfImage = rig.parts->treePane.GetFullPathTip (child);
            }
        }

        Assert::AreEqual (std::wstring (L"C:\\Disks"), tipOfDisks, L"A folder shown by its name has its path");
        Assert::AreEqual (std::wstring (L"C:\\A\\games"), tipOfShared, L"and so has one labeled with the folder above, to tell it from another");
        Assert::IsTrue   (tipOfPath.empty(),            L"A folder labeled with its path has no tip");
        Assert::IsTrue   (tipOfSub.empty(),             L"nor has a folder inside one");
        Assert::IsTrue   (tipOfImage.empty(),           L"nor a disk image inside one");
    }



    TEST_METHOD (Rename_InADiskImage_SelectsTheRowUnderItsNewName)
    {
        Rig           rig;
        DxuiKeyEvent  key;



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks\\dos33.dsk"));
        rig.parts->listPane.FillList();
        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"HELLO"));

        rig.parts->fileOps.BeginRename();

        for (wchar_t ch : std::wstring (L"BYE"))
        {
            key.kind = DxuiKeyEventKind::Char;
            key.vk   = ch;
            rig.parts->fileOps.HandleRenameKey (key);
        }

        key.kind = DxuiKeyEventKind::Down;
        key.vk   = VK_RETURN;
        rig.parts->fileOps.HandleRenameKey (key);

        Assert::AreEqual ((size_t) 1, rig.browser.GetSelectedRows().size());
        Assert::AreEqual (std::wstring (L"BYE"), rig.browser.GetRows()[(size_t) rig.browser.GetSelectedRows()[0]].name);
    }



    //  An image renamed in a Full Send folder is renamed in the folder's
    //  manifest as well.
    TEST_METHOD (Rename_OfAnImageInAFullSendFolder_RenamesItInTheManifest)
    {
        Rig                rig;
        DxuiKeyEvent       key;
        FullSendManifest   manifest;
        FullSendRow        row;
        FullSendManifest   read;
        std::wstring       renamed;
        std::string        path    = "C:\\Sends\\" + std::string (FullSendManifest::kFileName);



        row.sequence    = 1;
        row.file        = "Disk 001.dsk";
        row.fingerprint = std::string (64, 'a');
        row.hash        = std::string (64, 'b');
        row.time        = "2026-11-08T10:00:00";
        row.status      = FullSendManifest::kStatusSent;
        manifest.Add (row);

        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Sends\\Disk 001.dsk");
        rig.io.files[path] = manifest.Format();
        rig.verbs->onRename = [&renamed] (const std::wstring &, const std::wstring & name) { renamed = name; return S_OK; };

        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Sends"));
        rig.parts->listPane.FillList();
        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"Disk 001.dsk"));

        rig.parts->fileOps.BeginRename();

        for (wchar_t ch : std::wstring (L"Archon"))
        {
            key.kind = DxuiKeyEventKind::Char;
            key.vk   = ch;
            rig.parts->fileOps.HandleRenameKey (key);
        }

        key.kind = DxuiKeyEventKind::Down;
        key.vk   = VK_RETURN;
        rig.parts->fileOps.HandleRenameKey (key);

        AssertSucceeded (read.Parse (rig.io.files[path]));
        Assert::IsFalse  (renamed.empty(), L"the image is renamed");
        Assert::AreEqual (TextEncoding::WideToNarrow (renamed), read.GetRows()[0].file, L"and the manifest has its new name");
    }



    TEST_METHOD (CreateImage_FromADrivesMenu_LeavesTheListWhereItIs)
    {
        Rig                 rig;
        FakeHostDialogs   * dialogs = nullptr;
        Location            drive   = Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 1), std::string());



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks\\dos33.dsk"));

        dialogs        = static_cast<FakeHostDialogs *> (rig.parts->services.dialogs.get());
        dialogs->picks = false;

        rig.parts->cassoLinkManager.ReadDiskToImage (drive);

        Assert::AreEqual (1, dialogs->saves);
        Assert::IsTrue   (dialogs->lastSpec.title.starts_with (L"Create image from"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), rig.browser.GetLocation().path);
    }




    TEST_METHOD (WriteImage_FromADrivesMenu_LeavesTheListWhereItIs)
    {
        Rig                 rig;
        FakeHostDialogs   * dialogs = nullptr;
        Location            drive   = Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 2), std::string());



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks\\dos33.dsk"));

        dialogs        = static_cast<FakeHostDialogs *> (rig.parts->services.dialogs.get());
        dialogs->picks = false;

        rig.parts->cassoLinkManager.WriteImageToDisk (drive);

        Assert::AreEqual (1, dialogs->opens);
        Assert::IsTrue   (dialogs->lastSpec.title.starts_with (L"Write image to"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.dsk"), rig.browser.GetLocation().path);
    }




    //  A file that sorts ahead of the image the drag is over takes its row,
    //  and a file is no drop target.
    TEST_METHOD (DropHighlight_FollowsTheRowUnderTheDrag_WhenTheListRefills)
    {
        Rig    rig;
        POINT  at  = {};
        bool   hit = false;



        rig.Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\m.dsk");
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (L"C:\\Disks"));
        rig.parts->listPane.FillList();

        for (at.x = 640, at.y = 0; at.y < 800 && !hit; at.y += 2)
        {
            hit = rig.parts->listPane.HitTestRow (at) == 0;
        }

        Assert::IsTrue (hit);
        at.y -= 2;

        rig.parts->dragDrop.ShowDropTarget (CassoExplorerWindow::kDropTagList, at, true);
        Assert::AreEqual (0, rig.parts->listPane.GetDropRow());

        AssertSucceeded (rig.fs.WriteAllText (L"C:\\Disks\\a.txt", "text"));
        AssertSucceeded (rig.browser.Refresh());
        rig.parts->listPane.FillList();

        Assert::AreEqual (std::wstring (L"a.txt"), rig.browser.GetRows()[0].name);
        Assert::AreEqual (-1, rig.parts->listPane.GetDropRow());
    }




    TEST_METHOD (HandleTimer_TakesTheWindowsTimersButTheTick)
    {
        Rig  rig;



        rig.Open();

        for (AppTimerId timer : { AppTimerId::ExplorerFolder, AppTimerId::ExplorerRecycleBin, AppTimerId::ExplorerRefresh })
        {
            Assert::IsTrue (rig.parts->HandleTimer ((UINT_PTR) timer));
        }

        Assert::IsFalse (rig.parts->HandleTimer ((UINT_PTR) AppTimerId::ExplorerTooltip));
    }
};
