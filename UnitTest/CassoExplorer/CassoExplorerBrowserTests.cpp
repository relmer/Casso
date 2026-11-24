#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "Seams/ShellVerbs/Win32ShellNamespace.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "../EhmTestHelper.h"
#include "Core/UnicodeSymbols.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "ScratchImages.h"
#include "FakeRecycleBin.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/CollectionResultsItems.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/AppleSingleCodec.h"
#include "Machines/Apple2/Common/AppleFileName.h"
#include "Devices/Disk/Archives/ContainerDiskFileIo.h"
#include "Update/ZipArchive.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "BrowserTestHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace BrowserTestHost;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowserTests
//
//  The browser's view state over an in-memory host holding the two scratch
//  images: tree nodes for the widget, list rows for a folder and for an
//  image, the preview a selection produces, sorting with the selection kept,
//  and the status text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerBrowserTests)
{
public:

    TEST_METHOD (AppleTypeIcons_FollowDos33AndProDosTypes)
    {
        using Kind = IShellIcons::Kind;

        Assert::IsTrue  (BrowserCells::IsBasicListingName (L"Joyport.bas.txt"), L"A BASIC listing kept as text");
        Assert::IsTrue  (BrowserCells::IsBasicListingName (L"HELLO.BAS"));
        Assert::IsFalse (BrowserCells::IsBasicListingName (L"notes.txt"));
        Assert::IsFalse (BrowserCells::IsBasicListingName (L".bas"), L"a name that is only the suffix is no listing");
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"A")   == Kind::AppleApplesoft);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"BAS") == Kind::AppleApplesoft);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"I")   == Kind::AppleInteger);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"T")   == Kind::AppleText);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"BIN") == Kind::AppleBinary);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"SYS") == Kind::AppleSystem);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"REL") == Kind::AppleRelocatable);
        Assert::IsTrue (BrowserCells::GetAppleTypeIconKind (L"$F5") == Kind::File, L"Any other type is a plain file");
    }



    TEST_METHOD (TreeRoots_AreCollapsedAndLazy)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;
        int                        missing = 0;

        host.browser.GetTreeRoots (roots);

        Assert::AreEqual ((size_t) 4, roots.size(), L"CassoLink, Casso, This PC and CassoLink again; the bin only with all folders on");
        Assert::IsFalse  (roots[1].expanded);
        Assert::IsFalse  (roots[1].childrenLoaded);
    }



    //  Explorer's navigation pane below Casso's root: Home, Gallery and
    //  OneDrive; a line; the pinned folders; a line; the rest, with Casso's
    //  own This PC in the shell's place and the Recycle Bin last.
    TEST_METHOD (NavigationPane_FollowsExplorersSectionsAndOrder)
    {
        Host                        host;
        FakeRecycleBin              shell;
        std::vector<DxuiTreeNode>   roots;
        std::vector<std::wstring>   labels;

        IShellItemVerbs::ShellFolderItem  home     { L"::{HOME}", L"Home" };
        IShellItemVerbs::ShellFolderItem  oneDrive { L"C:\\OneDrive", L"OneDrive", L"C:\\OneDrive" };
        IShellItemVerbs::ShellFolderItem  pc       { L"::{PC}", L"This PC" };
        IShellItemVerbs::ShellFolderItem  network  { L"::{NET}", L"Network" };
        IShellItemVerbs::ShellFolderItem  bin      { L"::{BIN}", L"Recycle Bin" };
        IShellItemVerbs::ShellFolderItem  pin      { L"C:\\Disks", L"Disks", L"C:\\Disks" };
        IShellItemVerbs::ShellFolderItem  libraries { L"::{LIB}", L"Libraries" };
        TreeModel::NavPaneOptions         options;

        home.leading     = true;
        oneDrive.leading = true;
        pc.isThisPc      = true;
        bin.isRecycleBin = true;
        network.hasSubfolders = true;
        network.isNetwork     = true;
        libraries.isLibraries = true;
        libraries.shownByShell = false;
        pin.isPinned           = true;

        shell.navRoots = { home, oneDrive, pc, libraries, network, bin };
        shell.pinned   = { pin };
        host.browser.SetShellVerbs (&shell);
        host.browser.GetTreeRoots (roots);

        for (const DxuiTreeNode & root : roots)
        {
            labels.push_back (root.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"CassoLink", L"Casso", L"Home", L"OneDrive", L"Disks", L"This PC", L"CassoLink", L"Network" }),
                        L"As File Explorer's pane: libraries hidden there, and the bin only with all folders shown");
        Assert::IsTrue (roots[4].dividerAbove, L"A line above the pinned folders");
        Assert::IsTrue (roots[5].dividerAbove, L"and one below them");
        Assert::IsTrue (roots[5].id == TreeModel::kThisPcRootId, L"Casso's own This PC, with its drives");
        Assert::IsTrue (host.browser.IsPinnedToQuickAccess (pin.path), L"a pinned folder offers Unpin");
        Assert::IsTrue (roots[2].childrenLoaded, L"Home has no folders under it");
        Assert::IsFalse (roots[7].childrenLoaded, L"Network does");

        //  The pane's own menu turns each root on and off, apart from File
        //  Explorer's choice once set.
        options.showThisPc     = false;
        options.showNetwork    = false;
        options.showLibraries  = true;
        host.browser.GetTreeModel().SetNavPaneOptions (options);
        host.browser.GetTreeRoots (roots);
        labels.clear();

        for (const DxuiTreeNode & root : roots)
        {
            labels.push_back (root.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"CassoLink", L"Casso", L"Home", L"OneDrive", L"Disks", L"Libraries" }));
        Assert::IsTrue (roots[5].dividerAbove, L"The line moves to the first root left");
    }



    //  With the all-folders option on, the pane has Casso's root and the desktop, as File
    //  Explorer's has the desktop alone: everything else is under it, in the
    //  desktop's order, with Casso's own This PC and Recycle Bin in theirs and
    //  the roots the pane's options hide hidden there too.
    TEST_METHOD (NavigationPane_AllFoldersHasTheDesktopRoot)
    {
        Host                        host;
        FakeRecycleBin              shell;
        std::vector<DxuiTreeNode>   roots;
        std::vector<DxuiTreeNode>   children;
        std::vector<std::wstring>   labels;

        IShellItemVerbs::ShellFolderItem  home     { L"::{HOME}", L"Home" };
        IShellItemVerbs::ShellFolderItem  docs     { L"C:\\Docs", L"Documents", L"C:\\Docs" };
        IShellItemVerbs::ShellFolderItem  pc       { L"::{PC}", L"This PC" };
        IShellItemVerbs::ShellFolderItem  network  { L"::{NET}", L"Network" };
        IShellItemVerbs::ShellFolderItem  bin      { L"::{BIN}", L"Recycle Bin" };
        TreeModel::NavPaneOptions         options;

        pc.isThisPc       = true;
        bin.isRecycleBin  = true;
        network.isNetwork = true;

        shell.navRoots     = { home, pc, network, bin };
        shell.desktopItems = { home, docs, pc, network, bin };
        host.browser.SetShellVerbs (&shell);

        options.showAllFolders = true;
        options.showNetwork    = false;
        host.browser.GetTreeModel().SetNavPaneOptions (options);
        host.browser.GetTreeRoots (roots);

        Assert::AreEqual ((size_t) 3, roots.size());
        Assert::AreEqual (std::wstring (TreeModel::kDesktopRootId), roots[2].id);
        Assert::AreEqual (std::wstring (L"Desktop"), roots[2].label);

        children = host.browser.GetTreeChildren (TreeModel::kDesktopRootId);

        for (const DxuiTreeNode & child : children)
        {
            labels.push_back (child.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"Home", L"Documents", L"This PC", L"Recycle Bin" }));
        Assert::AreEqual (std::wstring (TreeModel::kThisPcRootId), children[2].id, L"Casso's own This PC, with its drives");
        Assert::AreEqual (std::wstring (Location::kRecycleBinId), children[3].id);

        //  Once Quick access is read, the shell's desktop lists Downloads a
        //  second time, ahead of Home; it is listed once, in its own place.
        IShellItemVerbs::ShellFolderItem  downloads { L"C:\\Downloads", L"Downloads", L"C:\\Downloads" };

        shell.desktopItems = { downloads, home, docs, downloads, pc, bin };
        host.browser.GetTreeModel().Invalidate (TreeModel::kDesktopRootId);
        children = host.browser.GetTreeChildren (TreeModel::kDesktopRootId);
        labels.clear();

        for (const DxuiTreeNode & child : children)
        {
            labels.push_back (child.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"Home", L"Documents", L"Downloads", L"This PC", L"Recycle Bin" }));
    }



    TEST_METHOD (TypedPaths_RecordASuccessfulNavigationAndPromoteARepeat)
    {
        Host  host;

        Assert::IsTrue  (host.browser.GetNavigator().NavigateToAddress (L"C:\\Disks"));
        Assert::IsFalse (host.browser.GetNavigator().NavigateToAddress (L"C:\\Nowhere"), L"A path that goes nowhere navigates nowhere");

        Assert::AreEqual ((size_t) 1, host.browser.GetNavigator().GetTypedPaths().GetEntries().size(), L"and is not recorded");
        Assert::AreEqual (std::wstring (L"C:\\Disks"), host.browser.GetNavigator().GetTypedPaths().GetEntries()[0]);

        //  What the history dropdown does with a pick: submit the text again.
        Assert::IsTrue (host.browser.GetNavigator().NavigateToAddress (L"C:\\Disks\\prodos.po"));
        Assert::IsTrue (host.browser.GetNavigator().NavigateToAddress (L"C:\\Disks"));

        Assert::AreEqual ((size_t) 2, host.browser.GetNavigator().GetTypedPaths().GetEntries().size(), L"A repeat does not appear twice");
        Assert::AreEqual (std::wstring (L"C:\\Disks"), host.browser.GetNavigator().GetTypedPaths().GetEntries()[0], L"and moves to the top");
    }



    TEST_METHOD (TypedPath_WithAnEnvironmentVariable_GoesToItsValue)
    {
        Host  host;

        Assert::IsTrue (SetEnvironmentVariableW (L"CASSO_TEST_DISKS", L"C:\\Disks") != FALSE);

        Assert::IsTrue   (host.browser.GetNavigator().NavigateToAddress (L"%CASSO_TEST_DISKS%"));
        Assert::AreEqual (std::wstring (L"C:\\Disks"), host.browser.GetLocation().path);
        Assert::AreEqual (std::wstring (L"%CASSO_TEST_DISKS%"), host.browser.GetNavigator().GetTypedPaths().GetEntries()[0], L"The history has the text as typed");

        Assert::AreEqual (std::wstring (L"%CASSO_UNDEFINED_VARIABLE%\\x"), BrowserNavigator::ExpandVariables (L"%CASSO_UNDEFINED_VARIABLE%\\x"));

        SetEnvironmentVariableW (L"CASSO_TEST_DISKS", nullptr);
    }



    TEST_METHOD (SelectFolder_ListsHostEntriesAndFlagsImages)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());
        Assert::IsTrue   (host.browser.GetRows()[FindRow (host.browser, L"prodos.po")].isDiskImage);
        Assert::IsFalse  (host.browser.GetRows()[FindRow (host.browser, L"readme.txt")].isDiskImage);
        Assert::AreEqual (std::wstring (L"3 items"), host.browser.GetStatus().selection);
    }



    TEST_METHOD (HiddenItems_FollowExplorersFolderOptions)
    {
        Host           host;
        FolderOptions  options;
        CatalogRow     row;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\hidden.txt", "h"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\protected.sys", "p"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\system.txt", "s"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\packed.txt", "c"));
        host.fs.SetAttributes (L"C:\\Disks\\hidden.txt",    true,  false, false);
        host.fs.SetAttributes (L"C:\\Disks\\protected.sys", true,  true,  false);
        host.fs.SetAttributes (L"C:\\Disks\\system.txt",    false, true,  false);
        host.fs.SetAttributes (L"C:\\Disks\\packed.txt",    false, false, true);

        //  Explorer's defaults: a system item shows unless it is hidden too.
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        Assert::AreEqual ((size_t) 5, host.browser.GetRows().size());
        FindRow (host.browser, L"system.txt");

        options.showHidden = true;
        host.browser.SetFolderOptions (options);
        AssertSucceeded (host.browser.Reload (true));
        Assert::AreEqual ((size_t) 6, host.browser.GetRows().size(), L"Hidden items shown");

        row = host.browser.GetRows()[(size_t) FindRow (host.browser, L"hidden.txt")];
        Assert::IsTrue (row.isHidden);
        Assert::IsTrue (BrowserCells::ToCells (row)[0].iconGhosted, L"and drawn faded");

        options.showProtected = true;
        host.browser.SetFolderOptions (options);
        AssertSucceeded (host.browser.Reload (true));
        Assert::AreEqual ((size_t) 7, host.browser.GetRows().size(), L"Protected items shown");

        //  A compressed name is colored only when Explorer colors them.
        row = host.browser.GetRows()[(size_t) FindRow (host.browser, L"packed.txt")];
        Assert::AreEqual (0u, BrowserCells::GetNameArgb (row, options, true));

        options.colorCompressed = true;
        Assert::AreEqual (CassoExplorerBrowser::kCompressedDarkArgb,  BrowserCells::GetNameArgb (row, options, true));
        Assert::AreEqual (CassoExplorerBrowser::kCompressedLightArgb, BrowserCells::GetNameArgb (row, options, false));
        Assert::AreEqual (0u, BrowserCells::GetNameArgb (host.browser.GetRows()[(size_t) FindRow (host.browser, L"system.txt")], options, true));
    }



    TEST_METHOD (DriveTile_ShowsHowFullAndWhatIsFree)
    {
        CatalogRow                       drive;
        CatalogRow                       folder;
        std::vector<DxuiListView::Cell>  cells;

        drive.name        = L"Local Disk (C:)";
        drive.isDirectory = true;
        drive.isDrive     = true;
        drive.sizeBytes   = 4000ull * 1024 * 1024 * 1024;
        drive.freeBytes   = 1000ull * 1024 * 1024 * 1024;

        cells = BrowserCells::ToCells (drive);

        Assert::AreEqual ((size_t) 2, cells[0].tileLines.size());
        Assert::AreEqual (0.75f, cells[0].tileLines[0].meter, 0.001f);
        Assert::AreEqual (BrowserCells::FormatSize (drive.freeBytes) + L" free of " + BrowserCells::FormatSize (drive.sizeBytes),
                          cells[0].tileLines[1].text);

        folder.name        = L"Games";
        folder.isDirectory = true;

        Assert::IsTrue (BrowserCells::ToCells (folder)[0].tileLines.empty(), L"A folder's tile shows its columns");
    }



    TEST_METHOD (SelectImage_ListsCatalogAndFreeSpace)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        Assert::IsTrue  (host.browser.GetListError().empty());
        Assert::IsTrue  (host.browser.GetLocation().kind == Location::Kind::DiskImage);
        FindRow (host.browser, L"HELLO");
        FindRow (host.browser, L"PICTURE");
        Assert::IsFalse (host.browser.GetStatus().freeSpace.empty());
    }



    TEST_METHOD (AnAppleSingleHostFile_PreviewsAsTheFileItHolds)
    {
        Host               host;
        std::wstring       folder = host.OpenDisksFolder();
        AppleSingleFile    file;
        std::vector<Byte>  bytes;
        bool               named  = false;
        bool               typed  = false;
        bool               dated  = false;

        file.data          = { 0x60 };
        file.realName      = "GAME";
        file.hasProDosInfo = true;
        file.fileType      = 0x06;
        file.auxType       = 0x6000;
        file.modifyDate    = 86400;
        AppleSingleCodec::Encode (file, bytes);
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\GAME.as", std::string (bytes.begin(), bytes.end())));

        AssertSucceeded (host.browser.SelectTreeNode (folder));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"GAME.as") });

        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Details);

        for (const auto & [label, value] : host.browser.GetPreviewer().GetPreview().details)
        {
            named |= label == L"Name"     && value == L"GAME";
            typed |= label == L"Aux type" && value == L"$6000";
            dated |= label == L"Modified" && !value.empty() && value != L"Not valid";
        }

        Assert::IsTrue (named, L"The real name");
        Assert::IsTrue (typed, L"The aux type");
        Assert::IsTrue (dated, L"The date, counted from 2000");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex, L"Any other file still shows its bytes");
    }



    TEST_METHOD (SelectingFiles_PreviewsByType)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"HELLO") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Listing);
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().lines.empty());

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"PICTURE") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Picture);

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"ODD") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex);
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().disassembly, L"A binary file of data opens as bytes");
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().bytes.empty());
        Assert::IsTrue  (ExplorerPreviewManager::IsKnownPreview (host.browser.GetPreviewer().GetPreview()), L"and its hex opens the pane by itself");

        host.browser.GetPreviewer().SetHexForm (PreviewDecoder::HexForm::Disassembly);
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().disassembly);
        Assert::IsTrue (ExplorerPreviewManager::IsKnownPreview (host.browser.GetPreviewer().GetPreview()), L"as does its disassembly");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"HELLO") });
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"ODD") });
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().disassembly, L"and it opens as bytes again after another file");

        PreviewContent  fallback;

        fallback.kind  = PreviewContent::Kind::Hex;
        fallback.bytes = { 0x00 };
        Assert::IsFalse (ExplorerPreviewManager::IsKnownPreview (fallback), L"but hex as the fallback is not");
    }



    //  A double hi-res picture's monitor follows its form: color on the color
    //  monitor, its bits on the chosen monochrome one, or the white one when
    //  the color monitor is chosen. Any other picture is on the chosen one.
    TEST_METHOD (DoubleHiResForm_PicksTheMonitor)
    {
        using Mode    = PicturePreview::Mode;
        using Monitor = PicturePreview::Monitor;

        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Green,  Mode::DoubleHiRes)     == Monitor::Color);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Color,  Mode::DoubleHiRes)     == Monitor::Color);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Amber,  Mode::DoubleHiResMono) == Monitor::Amber);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Custom, Mode::DoubleHiResMono) == Monitor::Custom);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Color,  Mode::DoubleHiResMono) == Monitor::White);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Green,  Mode::HiRes)           == Monitor::Green);
        Assert::IsTrue (BrowserPreviewer::ChooseMonitor (Monitor::Color,  Mode::LoRes)           == Monitor::Color);
    }



    TEST_METHOD (MultipleSelection_PreviewsNothingAndCounts)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.GetSelection().SetSelectedRows ({ 0, 1 });

        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines.empty());
        Assert::AreEqual (0u, (unsigned) host.browser.GetStatus().selected.find (L"2 items selected"), L"the selection has a field of its own");
    }



    TEST_METHOD (ImageRowInFolder_PreviewsItsCatalog)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"prodos.po") });

        Assert::IsTrue  (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Catalog);
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().rows.empty());
    }



    TEST_METHOD (Status_ImageFreeSpaceIsOfTheWhole_AndSaysWhichDisk)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        Assert::IsTrue (host.browser.GetStatus().freeSpace.ends_with (L" free of 140 KB"));
        Assert::IsTrue (host.browser.GetStatus().freeSpaceTip.starts_with (L"dos33.dsk, DOS 3.3 volume "));
    }



    //  One image selected in its folder shows its own space, as it does open.
    TEST_METHOD (Status_ASelectedImageShowsItsFreeSpace)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"dos33.dsk") });

        Assert::IsTrue (host.browser.GetStatus().freeSpace.ends_with (L" free of 140 KB"));
        Assert::IsTrue (host.browser.GetStatus().freeSpaceTip.starts_with (L"dos33.dsk, DOS 3.3 volume "));

        host.browser.GetSelection().SetSelectedRows ({});
        Assert::IsTrue (host.browser.GetStatus().freeSpace.empty(), L"Nothing for a folder");
    }



    TEST_METHOD (LeaveMissingLocation_MovesToTheFolderAbove)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        Assert::IsFalse (host.browser.GetLoader().LeaveMissingLocation());

        AssertSucceeded (host.fs.Delete (L"C:\\Disks\\dos33.dsk"));
        Assert::IsTrue (host.browser.GetLoader().LeaveMissingLocation());

        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::HostFolder);
        Assert::AreEqual (std::wstring (kDisks), host.browser.GetLocation().path);
    }



    //  A card reader whose card can be put in or taken out. The card is never
    //  opened.
    struct ReaderCardHost : public IPhysicalCardHost
    {
        bool  hasCard = true;

        HRESULT ListDrives (std::vector<PhysicalDrive> & outDrives) override
        {
            PhysicalDrive  drive;

            drive.number    = 5;
            drive.bytes     = 2'048'094'720;
            drive.removable = true;

            outDrives.clear();

            if (hasCard)
            {
                outDrives.push_back (drive);
            }

            return S_OK;
        }

        HRESULT HasUnmountedPartition (int, bool & outHasUnmounted) override             { outHasUnmounted = true; return S_OK; }
        HRESULT ListVolumes           (int, std::vector<WindowsVolume> & outVolumes) override { outVolumes.clear(); return S_OK; }
        HRESULT FindVolumeDrive       (const std::wstring &, int &) override                { return E_NOTIMPL; }
        HRESULT InitializeDrive       (int, HWND, uint64_t, const WriteProgress &) override { return E_NOTIMPL; }
        HRESULT FormatFirstPartition  (int, const WriteProgress &, bool &) override         { return E_NOTIMPL; }
        bool    IsBrokerRunning       () override                                           { return false; }
        HRESULT OpenDrive             (int, HWND, PhysicalDrive &, std::unique_ptr<IBlockStore> &) override { return E_NOTIMPL; }
    };



    TEST_METHOD (LeaveMissingLocation_StaysOnACardUntilItIsTakenOut)
    {
        Host            host;
        ReaderCardHost  reader;
        PhysicalCards   cards (reader);
        std::wstring    card = L"\\\\.\\PhysicalDrive5";

        host.browser.SetPhysicalCards (&cards);
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetNavigator().NavigateToLocation (Location::MakeDiskImage (card + L"|#2.po"));
        Assert::IsTrue   (host.browser.GetLoader().LeaveMissingLocation(), L"a partition of a card that is not open moves to the card");
        Assert::AreEqual (card, host.browser.GetLocation().path);

        Assert::IsFalse  (host.browser.GetLoader().LeaveMissingLocation(), L"a card that is not open stays until it is opened");
        Assert::AreEqual (card, host.browser.GetLocation().path);

        reader.hasCard = false;
        Assert::IsTrue   (host.browser.GetLoader().LeaveMissingLocation(), L"a card taken out is left");
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::Root);
        Assert::AreEqual (std::wstring (TreeModel::kThisPcRootId), host.browser.GetLocation().path);
    }



    TEST_METHOD (CorruptImage_ShowsItsErrorInTheList)
    {
        Host          host;
        std::wstring  folder;

        host.SeedBytes (std::vector<Byte> { 'n', 'o', 't', ' ', 'a', ' ', 'd', 'i', 's', 'k' }, L"C:\\Disks\\bad.dsk");
        folder = host.OpenDisksFolder();

        host.browser.SelectTreeNode (host.FindChildId (folder, L"bad.dsk"));

        Assert::IsTrue  (host.browser.GetRows().empty());
        Assert::AreEqual (std::wstring (L"bad.dsk is not a valid .dsk image.\nFile size is 10 bytes but should be 143,360 bytes."),
                          host.browser.GetListError(), L"The file by its name, and why; the path is in the address bar");
    }



    TEST_METHOD (Cells_LeaveBlankWhatTheEntryDoesNotRecord)
    {
        CatalogRow                        row;
        std::vector<DxuiListView::Cell>   cells;

        row.name        = L"SUB";
        row.typeText    = L"DIR";
        row.isDirectory = true;
        row.hasModified = false;

        cells = BrowserCells::ToCells (row);

        Assert::AreEqual (BrowserCells::GetColumns().size(), cells.size());
        Assert::IsTrue   (cells[(size_t) CatalogModel::Column::Size].text.empty(),     L"a folder has no size");
        Assert::IsTrue   (cells[(size_t) CatalogModel::Column::Modified].text.empty(), L"and no date was recorded");
    }



    TEST_METHOD (KnownFolderNode_ShowsTheFolderName)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;
        int                        missing = 0;

        host.browser.GetTreeRoots (roots);

        Assert::AreEqual (std::wstring (L"Disks"), host.browser.GetTreeChildren (roots[1].id)[0].label);
    }



    //  The Casso root lists the images of its folders, not the folders, and
    //  an image opens from there as it does in its own folder.
    TEST_METHOD (CassoRoot_ListsItsFoldersImages_AndOpensThem)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeRoots (roots);

        AssertSucceeded (host.browser.SelectTreeNode (roots[1].id));
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size(), L"dos33.dsk and prodos.po, without readme.txt");
        Assert::IsFalse  (host.browser.GetRows()[0].isDirectory);

        Assert::IsTrue   (host.browser.OpenRow (FindRow (host.browser, L"dos33.dsk")));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeDiskImage (L"C:\\Disks\\dos33.dsk"));
    }



    TEST_METHOD (ARoot_IsALocationUnderItsOwnLabel)
    {
        Host                                      host;
        std::vector<DxuiTreeNode>                 roots;
        std::vector<BrowserModel::AddressSegment> segments;

        host.browser.GetTreeRoots (roots);

        AssertSucceeded (host.browser.SelectTreeNode (TreeModel::kThisPcRootId));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeRoot (TreeModel::kThisPcRootId));
        Assert::AreEqual (std::wstring (TreeModel::kThisPcRootId), host.browser.GetRootId());
        Assert::AreEqual (std::wstring (L"This PC"), host.browser.GetCells().GetTabLabel (0), L"The tab shows the root's label, not Home");
        Assert::AreEqual (std::wstring (L"This PC"), BrowserModel::FormatAddress (host.browser.GetLocation()));

        segments = BrowserModel::GetAddressSegments (host.browser.GetLocation());
        Assert::AreEqual ((size_t) 1, segments.size());
        Assert::AreEqual (std::wstring (L"This PC"), segments[0].label);

        //  The root is in the history like any other location.
        AssertSucceeded (host.browser.SelectTreeNode (TreeModel::kCassoRootId));
        Assert::AreEqual (std::wstring (L"Casso"), host.browser.GetCells().GetTabLabel (0));
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size(), L"the known folder's two images");

        Assert::IsTrue   (host.browser.GetNavigator().GoBack());
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeRoot (TreeModel::kThisPcRootId));
        Assert::IsFalse  (host.browser.GetNavigator().CanGoUp());

        //  And its label, typed, goes there.
        Assert::IsTrue   (host.browser.GetNavigator().NavigateToAddress (L"casso"));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeRoot (TreeModel::kCassoRootId));
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());
    }


    //  Drive information for E: and F:, both with media, and A:, a drive
    //  with none.
    struct DriveIcons : public IShellIcons
    {
        std::shared_ptr<const DxuiIconImage>  GetForPath (const std::wstring &, bool) override { return nullptr; }
        std::shared_ptr<const DxuiIconImage>  GetForKind (Kind) override                       { return nullptr; }

        bool  GetDriveInfo (const std::wstring & root, DriveInfo & outInfo) override
        {
            outInfo.name       = L"Removable Disk (" + root.substr (0, 2) + L")";
            outInfo.typeName   = L"Removable Disk";
            outInfo.totalBytes = root[0] == L'A' ? 0 : 1ull << 30;
            return true;
        }
    };



    //  A drive with A2retroNET.txt in its root is an A2Pico card. A drive
    //  with no media isn't read, so an empty floppy drive doesn't stall the
    //  list.
    TEST_METHOD (ThisPc_ListsAnA2PicoCardByItsType)
    {
        Host                       host;
        DriveIcons                 icons;
        std::vector<DxuiTreeNode>  roots;



        AssertSucceeded (host.fs.WriteAllText (L"E:\\A2retroNET.txt", "[drives]\n"));
        host.browser.SetShellIcons (&icons);
        host.browser.GetTreeModel().SetDrives ({ L"A:\\", L"E:\\", L"F:\\" });

        host.browser.GetTreeRoots (roots);

        AssertSucceeded (host.browser.SelectTreeNode (TreeModel::kThisPcRootId));
        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());

        Assert::AreEqual (std::wstring (L"Removable Disk"), host.browser.GetRows()[0].typeText);
        Assert::AreEqual (std::wstring (L"A2Pico card"),    host.browser.GetRows()[1].typeText);
        Assert::AreEqual (std::wstring (L"Removable Disk"), host.browser.GetRows()[2].typeText);
    }



    //  A known folder whose drive is gone adds no rows; the others' images
    //  are listed.
    TEST_METHOD (CassoRoot_SkipsAMissingFolder)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeModel().SetKnownFolders ({ L"H:\\Games", kDisks });
        host.browser.GetTreeRoots (roots);
        AssertSucceeded (host.browser.SelectTreeNode (TreeModel::kCassoRootId));

        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());

        for (const CatalogRow & row : host.browser.GetRows())
        {
            Assert::AreEqual (std::wstring (kDisks), row.folderPath);
            Assert::IsFalse  (row.isMissing);
        }
    }



    TEST_METHOD (OpenRow_EntersImagesAndRefusesPlainFiles)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        Assert::IsFalse (host.browser.OpenRow (FindRow (host.browser, L"readme.txt")));
        Assert::IsTrue  (host.browser.GetLocation().kind == Location::Kind::HostFolder);

        Assert::IsTrue  (host.browser.OpenRow (FindRow (host.browser, L"dos33.dsk")));
        Assert::IsTrue  (host.browser.GetLocation() == Location::MakeDiskImage (L"C:\\Disks\\dos33.dsk"));
        FindRow (host.browser, L"HELLO");
    }



    TEST_METHOD (UpBackForward_Navigate)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        Assert::IsTrue (host.browser.OpenRow (FindRow (host.browser, L"prodos.po")));

        Assert::IsTrue (host.browser.GetNavigator().GoUp());
        Assert::IsTrue (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));

        Assert::IsTrue (host.browser.GetNavigator().GoBack());
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::DiskImage);

        Assert::IsTrue (host.browser.GetNavigator().GoForward());
        Assert::IsTrue (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
        Assert::IsTrue (host.browser.GetNavigator().CanGoUp());
    }



    TEST_METHOD (ParentFolder_StopsAtTheDriveRoot)
    {
        Assert::AreEqual (std::wstring (L"C:\\Disks"), BrowserRowPaths::GetParentFolder (L"C:\\Disks\\Sub"));
        Assert::AreEqual (std::wstring (L"C:\\"),      BrowserRowPaths::GetParentFolder (L"C:\\Disks\\"));
        Assert::IsTrue   (BrowserRowPaths::GetParentFolder (L"C:\\").empty());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), BrowserRowPaths::JoinPath (L"C:\\Disks", L"a.dsk"));
        Assert::AreEqual (std::wstring (L"C:\\a.dsk"),        BrowserRowPaths::JoinPath (L"C:\\", L"a.dsk"));
    }



    TEST_METHOD (CatalogPreviewCells_MatchTheirColumns)
    {
        CatalogRow  row;

        row.name      = L"HELLO";
        row.typeText  = L"A";
        row.sizeBytes = 512;

        Assert::AreEqual (BrowserCells::GetCatalogPreviewColumns().size(), BrowserCells::ToCatalogPreviewCells (row).size());
    }



    TEST_METHOD (Reload_KeepsTheSelectionByName)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        AssertSucceeded (host.browser.Reload (true));

        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        Assert::AreEqual (std::wstring (L"NOTES"), host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name);

        AssertSucceeded (host.browser.Reload (false));
        Assert::IsTrue (host.browser.GetSelectedRows().empty());
    }



    TEST_METHOD (Reload_KeepsALinkDriveSelected_WhenItsLabelChanges)
    {
        Host           host;
        LinkTreeEntry  link;
        std::wstring   linkId;
        std::wstring   drivePath;
        int            row = -1;

        link.name     = L"Apple II (COM3)";
        link.replying = true;
        link.drives   = { { 6, 1, false, L"" }, { 6, 2, true, L"" } };
        host.browser.GetTreeModel().SetLinks ({ link });

        for (const DxuiTreeNode & node : host.browser.GetTreeChildren (TreeModel::kCassoLinkRootId))
        {
            linkId = (node.label == link.name) ? node.id : linkId;
        }

        AssertSucceeded (host.browser.SelectTreeNode (linkId));
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());

        drivePath = host.browser.GetRows()[0].hostPath;
        host.browser.GetSelection().SetSelectedRows ({ 0 });

        //  Reading the drive's catalog relabels it.
        link.drives[0].volume = L"DOS 3.3 volume 254";
        host.browser.GetTreeModel().SetLinks ({ link });

        AssertSucceeded (host.browser.Reload (true));

        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        row = host.browser.GetSelectedRows()[0];
        Assert::AreEqual (drivePath, host.browser.GetRows()[(size_t) row].hostPath);
    }



    TEST_METHOD (KnownFolders_CanBeRemovedAndOtherFoldersAdded)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;
        int                        missing = 0;
        std::wstring               known;
        std::wstring               path;

        host.browser.GetTreeRoots (roots);
        known = host.browser.GetTreeChildren (roots[1].id)[0].id;

        Assert::IsTrue  (host.browser.CanRemoveFromCasso (known));
        Assert::IsFalse (host.browser.CanAddToCasso (known));
        Assert::IsTrue  (host.browser.TryGetNodePath (known, path));
        Assert::AreEqual (std::wstring (kDisks), path);

        Assert::IsFalse (host.browser.CanRemoveFromCasso (roots[1].id));
        Assert::IsFalse (host.browser.CanAddToCasso (roots[0].id));
    }



    TEST_METHOD (AKnownFolder_IsKnownByItsPath_WhateverItsCaseOrTrailingSlash)
    {
        Host          host;
        std::wstring  path = kDisks;



        Assert::IsTrue  (host.browser.IsKnownFolder (path));
        Assert::IsTrue  (host.browser.IsKnownFolder (path + L"\\"));

        std::transform (path.begin(), path.end(), path.begin(), ::towupper);

        Assert::IsTrue  (host.browser.IsKnownFolder (path),             L"Folder names are not case-sensitive");
        Assert::IsFalse (host.browser.IsKnownFolder (L"C:\\Elsewhere"), L"A folder not added is not known");
    }



    //  A folder or disk image row leads somewhere, a plain file does not, and
    //  every row has a path to copy.
    TEST_METHOD (Rows_LocationAndPath)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        Location      location;
        std::wstring  path;

        AssertSucceeded (host.browser.SelectTreeNode (folder));

        Assert::IsTrue   (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"dos33.dsk"), location));
        Assert::IsTrue   (location == Location::MakeDiskImage (L"C:\\Disks\\dos33.dsk"));
        Assert::IsFalse  (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"readme.txt"), location), L"A plain file leads nowhere");

        Assert::IsTrue   (BrowserRowPaths::TryGetRowPath (host.browser.GetRowListing(), FindRow (host.browser, L"readme.txt"), path));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\readme.txt"), path);

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        Assert::IsTrue   (BrowserRowPaths::TryGetRowPath (host.browser.GetRowListing(), FindRow (host.browser, L"NOTES"), path));
        Assert::IsTrue   (path.rfind (L"C:\\Disks\\dos33.dsk", 0) == 0, L"An entry in an image starts at the image");
        Assert::IsTrue   (path.size() > 5 && path.compare (path.size() - 5, 5, L"NOTES") == 0, L"and ends at the entry");
    }



    TEST_METHOD (HomeRow_WithAFile_HasItsPath_AndACloudRowHasNone)
    {
        Host                              host;
        FakeRecycleBin                    shell;
        IShellItemVerbs::ShellFolderItem  local;
        IShellItemVerbs::ShellFolderItem  cloud;
        std::wstring                      path;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\memo.txt", "memo"));

        local.id        = L"local";
        local.name      = L"memo.txt";
        local.path      = L"C:\\Disks\\memo.txt";
        local.homeGroup = Win32ShellNamespace::s_kHomeGroupRecent;
        cloud.id        = L"shell-idlist:00\\plan.pdf";
        cloud.name      = L"plan.pdf";
        cloud.homeGroup = Win32ShellNamespace::s_kHomeGroupRecent;
        shell.shellItems = { local, cloud };
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location());
        AssertSucceeded (host.browser.Refresh());

        Assert::IsTrue   (host.browser.IsHomeLocation());
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size(), host.browser.GetListError().c_str());
        Assert::IsTrue   (BrowserRowPaths::TryGetRowPath (host.browser.GetRowListing(), FindRow (host.browser, L"memo.txt"), path));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\memo.txt"), path, L"not joined to any folder");
        Assert::IsTrue   (host.browser.GetRows()[(size_t) FindRow (host.browser, L"plan.pdf")].hostPath.empty());
        Assert::IsFalse  (host.browser.GetRows()[(size_t) FindRow (host.browser, L"plan.pdf")].shellId.empty());
    }



    TEST_METHOD (DesktopListing_DropsTheControlPanelCommandBesideTheFolder)
    {
        std::vector<IShellItemVerbs::ShellFolderItem>  items (4);

        items[0].name = L"Control Panel";   items[0].isFolder = true;
        items[1].name = L"Control Panel";
        items[2].name = L"Shortcut";        items[2].path     = L"C:\\Users\\Public\\Desktop\\Shortcut.lnk";
        items[3].name = L"Command";

        Win32ShellNamespace::DropDesktopCommandTwins (items);

        Assert::AreEqual ((size_t) 3, items.size());
        Assert::IsTrue   (items[0].isFolder && items[1].name == L"Shortcut" && items[2].name == L"Command");
    }



    TEST_METHOD (HomeGroup_PinnedFile_IsAFavorite)
    {
        IShellItemVerbs::ShellFolderItem  entry;

        entry.homePinned = true;
        entry.homeGroup  = Win32ShellNamespace::s_kHomeGroupRecent;

        Assert::AreEqual (Win32ShellNamespace::s_kHomeGroupFavorites, Win32ShellNamespace::GetHomeGroup (entry));

        entry.homePinned = false;

        Assert::AreEqual (Win32ShellNamespace::s_kHomeGroupRecent, Win32ShellNamespace::GetHomeGroup (entry));

        entry.homeGroup  = 0;
        entry.isFolder   = true;
        entry.homePinned = true;

        Assert::AreEqual (Win32ShellNamespace::s_kHomeGroupQuickAccess, Win32ShellNamespace::GetHomeGroup (entry), L"a pinned folder is in Quick access");
    }



    //  Recent is the local and cloud files by latest access, and a pinned cloud
    //  file is also a favorite.
    TEST_METHOD (HomeRecent_MergesCloudFilesByDateAccessed)
    {
        std::vector<IShellItemVerbs::ShellFolderItem>  items (2);
        std::vector<IShellItemVerbs::ShellFolderItem>  cloud (2);

        items[0].name         = L"folder";
        items[0].homeGroup    = Win32ShellNamespace::s_kHomeGroupQuickAccess;
        items[1].name         = L"local";
        items[1].homeGroup    = Win32ShellNamespace::s_kHomeGroupRecent;
        items[1].accessedUnix = 200;
        cloud[0].name         = L"pinned";
        cloud[0].homePinned   = true;
        cloud[0].accessedUnix = 100;
        cloud[1].name         = L"newest";
        cloud[1].accessedUnix = 300;

        Win32ShellNamespace::AddCloudRecent (cloud, items);

        std::vector<std::wstring>  recent;
        size_t                     favorites = 0;

        for (const IShellItemVerbs::ShellFolderItem & item : items)
        {
            if (item.homeGroup == Win32ShellNamespace::s_kHomeGroupRecent)
            {
                recent.push_back (item.name);
            }

            favorites += (item.homeGroup == Win32ShellNamespace::s_kHomeGroupFavorites) ? 1 : 0;
        }

        Assert::AreEqual (L"folder", items[0].name.c_str());
        Assert::IsTrue   ((recent == std::vector<std::wstring> { L"newest", L"local", L"pinned" }));
        Assert::AreEqual ((size_t) 1, favorites);
    }



    TEST_METHOD (FolderColumn_IsTheLeafThenItsParent)
    {
        Assert::AreEqual (L"relmer (C:\\Users)", CatalogModel::GetFolderColumnText (L"C:\\Users\\relmer").c_str());
        Assert::AreEqual (L"Users (C:)",         CatalogModel::GetFolderColumnText (L"C:\\Users\\").c_str());
        Assert::AreEqual (L"C:\\",               CatalogModel::GetFolderColumnText (L"C:\\").c_str());
        Assert::AreEqual (L"",                   CatalogModel::GetFolderColumnText (L"").c_str());
    }



    TEST_METHOD (SpinnerFrame_ArcLengthGrowsAndShrinks)
    {
        auto  count = [] (int frame)
        {
            DxuiIconImage  image = DxuiIconImage::MakeSpinnerFrame (frame, 60, 24, 0x4CC2FF);

            return std::count_if (image.bgraPremul.begin(), image.bgraPremul.end(), [] (uint32_t pixel) { return (pixel >> 24) > 128; });
        };

        Assert::IsTrue (count (0) > 0);
        Assert::IsTrue (count (30) > count (0) * 3, L"the arc is longest mid-cycle");
    }



    TEST_METHOD (CommandBar_SearchHasCloseSearch)
    {
        CassoExplorerCommands  commands (CassoExplorerCommands::Handlers {});
        auto                   hasClose = [] (const std::vector<DxuiToolbar::Entry> & entries)
        {
            return std::any_of (entries.begin(), entries.end(), [] (const DxuiToolbar::Entry & entry)
            {
                return entry.command != nullptr && entry.command->id == CassoExplorerCommands::kCloseSearch;
            });
        };

        Assert::IsTrue  (hasClose (commands.BuildCommandBarEntries (CassoExplorerCommands::CommandBarExtra::Search)));
        Assert::IsFalse (hasClose (commands.BuildCommandBarEntries()));
    }



    TEST_METHOD (HomeMenu_RemoveVerb_MarksAFavorite)
    {
        Assert::IsTrue  (ExplorerListMenus::IsFavorite ({ L"Open", L"Remove from Favorites" }));
        Assert::IsFalse (ExplorerListMenus::IsFavorite ({ L"Open", L"Add to Favorites" }));
    }



    //  An assembler's source previews as text with its line numbers, in
    //  whatever character set it is; UTF-8 text of any kind previews as text,
    //  which the hex view cannot draw; plain ASCII text stays in the hex view.
    TEST_METHOD (SourceAndUtf8Files_PreviewAsText)
    {
        Host  host;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\demo.a65", "\xEF\xBB\xBF; caf\xC3\xA9\tx\r\n\tLDA #1\r\n"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\notes.txt", "a \xE2\x80\x94 b\n"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\wide.txt",  std::string ("\xFF\xFEh\0i\0\r\0\n\0", 10)));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\big.txt",   std::string ("\xFE\xFF\0h\0i", 6)));
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"demo.a65") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Text);
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().source, L"with its line numbers");
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines == std::vector<std::wstring> ({ L"; caf\u00E9  x", L"        LDA #1" }),
                        L"The mark dropped, the letters decoded and the tabs expanded");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"notes.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Text);
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().source);
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines == std::vector<std::wstring> ({ L"a \u2014 b" }));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"wide.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines == std::vector<std::wstring> ({ L"hi" }), L"UTF-16 with its mark, little-endian");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"big.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines == std::vector<std::wstring> ({ L"hi" }), L"and big-endian");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex, L"ASCII text draws in the hex view");
        Assert::IsTrue (BrowserPreviewer::IsSourceName (L"STAGE2.S") && !BrowserPreviewer::IsSourceName (L"readme.txt"));
    }



    //  A listing or text Casso Explorer exported previews as lines of text,
    //  by its name, where other ASCII text opens in the hex view.
    TEST_METHOD (ExportedAppleText_PreviewsAsLines)
    {
        Host  host;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\HELLO.Applesoft BASIC.txt", "10  PRINT \"HELLO\"\r\n"));
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"HELLO.Applesoft BASIC.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Text, L"ASCII, read as lines");
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().lines == std::vector<std::wstring> ({ L"10  PRINT \"HELLO\"" }));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex, L"ASCII without an export suffix opens in the hex view");

        Assert::IsTrue  (BrowserPreviewer::IsAppleExportName (L"GAME.Integer BASIC.txt"));
        Assert::IsTrue  (BrowserPreviewer::IsAppleExportName (L"NOTES.Text.txt"));
        Assert::IsFalse (BrowserPreviewer::IsAppleExportName (L"readme.txt"));
    }



    //  A disk image opens as one even with no file system on it, and the
    //  reason appears in the list, rather than another program opening it.
    TEST_METHOD (ImageWithNoFileSystem_OpensAndSaysWhy)
    {
        Host  host;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\blank.dsk", std::string (143360, '\0')));
        host.io.files["C:\\Disks\\blank.dsk"]  = std::vector<Byte> (143360, 0);
        host.io.stamps["C:\\Disks\\blank.dsk"] = FileStamp { 143360, 100 };
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        Assert::IsTrue (host.browser.OpenRow (FindRow (host.browser, L"blank.dsk")));
        Assert::IsTrue (host.browser.GetLocation() == Location::MakeDiskImage (L"C:\\Disks\\blank.dsk"));
        Assert::IsTrue (host.browser.GetRows().empty());
        Assert::AreEqual (std::wstring (L"blank.dsk can't be previewed because it doesn't have a recognized file system."),
                          host.browser.GetListError(), L"with the reason the list is empty");
    }
};
