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
#include "CassoExplorerBrowserTestHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace CassoExplorerBrowserTestHost;





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


    //
    //  The Recycle Bin by its name lists what the shell says it holds, with
    //  where each item was and when it was deleted, and its two columns' cells.
    //
    TEST_METHOD (RecycleBin_ByName_ListsItsItemsWithTheirOrigins)
    {
        Host            host;
        FakeRecycleBin  bin;
        int             row = -1;

        bin.items.push_back ({ L"C:\\$Recycle.Bin\\S-1\\$R1.txt", L"notes.txt", L"C:\\Docs", L"Text Document", 12, 1700000000, true, 1690000000, true, false });
        bin.items.push_back ({ L"D:\\$Recycle.Bin\\S-1\\$R2",     L"Old",       L"D:\\",     L"File folder",   4096, 1700000100, true, 0, false, true });
        host.browser.SetShellVerbs (&bin);

        Assert::IsTrue  (host.browser.GetNavigator().NavigateToAddress (L"recycle bin"));
        Assert::IsTrue  (host.browser.GetLocation() == Location::MakeRecycleBin());
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());

        row = FindRow (host.browser, L"notes.txt");

        std::vector<DxuiListView::Cell>  cells = BrowserCells::ToCells (host.browser.GetRows()[(size_t) row], host.browser.GetLocation());

        Assert::AreEqual (std::wstring (L"C:\\Docs"), cells[(size_t) CatalogModel::Column::OriginalLocation].text);
        Assert::IsFalse  (cells[(size_t) CatalogModel::Column::DateDeleted].text.empty());
        Assert::AreEqual (std::wstring (L"Recycle Bin"), BrowserCells::GetLocationLabel (host.browser.GetLocation()));

        host.browser.GetSelection().SetSelectedRows ({ row });

        std::vector<std::wstring>  ids;

        host.browser.GetLoader().GetSelectedRecycledIds (ids);
        Assert::AreEqual ((size_t) 1, ids.size());
        Assert::AreEqual (std::wstring (L"C:\\$Recycle.Bin\\S-1\\$R1.txt"), ids[0]);
    }


    //  Any shell folder by the shell's name for it lists what the shell lists:
    //  a folder on a disk opens as a host folder, a folder that is not on a
    //  disk opens through the shell, a folder that is a file too (a zip, a
    //  library) as well, and a disk image as an image; its tab keeps its name.
    TEST_METHOD (ShellFolder_ListsTheShellsItemsAndOpensEachAsWhatItIs)
    {
        Host                host;
        FakeRecycleBin      shell;
        Location            target;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        shell.shellItems.push_back ({ L"::{LIB}\\Documents.library-ms", L"Documents", L"C:\\Users\\a\\Documents.library-ms", L"Library", 0, 0, false, true, true });
        shell.shellItems.push_back ({ L"C:\\Disks",                     L"Disks",     L"C:\\Disks",                         L"File folder", 0, 0, false, true, false });
        shell.shellItems.push_back ({ L"::{NET}\\SERVER",               L"SERVER",    L"",                                  L"Computer", 0, 0, false, true, false });
        host.browser.SetShellVerbs (&shell);

        Assert::IsTrue (host.browser.GetNavigator().NavigateToAddress (L"shell:Libraries"));
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::ShellFolder);
        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());

        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"Disks"), target));
        Assert::IsTrue (target == Location::MakeHostFolder (L"C:\\Disks"), L"A folder on a disk opens as one");

        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"Documents"), target));
        Assert::IsTrue (target.kind == Location::Kind::ShellFolder, L"A library opens through the shell");
        Assert::AreEqual (std::wstring (L"Documents"), target.label);

        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"SERVER"), target));
        Assert::IsTrue (target == Location::MakeShellFolder (L"::{NET}\\SERVER", L""));

        saved.tabs.push_back (Location::MakeShellFolder (L"::{NET}\\SERVER", L"SERVER"));
        AssertSucceeded (saved.Save (L"C:\\Prefs", host.fs));
        AssertSucceeded (loaded.Load (L"C:\\Prefs", host.fs));

        Assert::AreEqual ((size_t) 1, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[0].kind == Location::Kind::ShellFolder);
        Assert::AreEqual (std::wstring (L"SERVER"), loaded.tabs[0].label, L"A saved tab keeps the name it shows");
    }


    //  A file the shell opens as a folder, as a zip, is a node after the
    //  folders in the tree, and a row in the list that opens through the shell.
    TEST_METHOD (Archive_ListsAfterFoldersAndOpensThroughTheShell)
    {
        Host                       host;
        FakeRecycleBin             shell;
        std::vector<DxuiTreeNode>  children;
        std::vector<std::wstring>  labels;
        Location                   target;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\Sub\\a.txt", "a"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\notes.zip",  "PK"));
        shell.archiveExtension = L".zip";
        host.browser.SetShellVerbs (&shell);

        children = host.browser.GetTreeChildren (host.OpenDisksFolder());

        for (const DxuiTreeNode & child : children)
        {
            labels.push_back (child.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"Sub", L"notes.zip", L"dos33.dsk", L"prodos.po" }),
                        L"Folders, then archives, then disk images");
        Assert::IsFalse (children[1].childrenLoaded, L"An archive opens like a folder");

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"notes.zip"), target));
        Assert::IsTrue (target == Location::MakeShellFolder (L"C:\\Disks\\notes.zip", L"notes.zip"));
        Assert::IsFalse (BrowserRowPaths::TryGetRowLocation (host.browser.GetRowListing(), FindRow (host.browser, L"readme.txt"), target), L"A plain file does not open");

        //  Selected, it previews as what it holds, folders first.
        {
            IShellItemVerbs::ShellFolderItem  file;
            IShellItemVerbs::ShellFolderItem  folder;

            file.name       = L"a.txt";
            file.sizeBytes  = 12;
            folder.name     = L"docs";
            folder.isFolder = true;
            shell.shellItems = { file, folder };

            host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"notes.zip") });

            Assert::IsTrue   (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Catalog);
            Assert::AreEqual ((size_t) 2, host.browser.GetPreviewer().GetPreview().rows.size());
            Assert::AreEqual (std::wstring (L"docs"), host.browser.GetPreviewer().GetPreview().rows[0].name);
        }
    }


    //  A disk image the shell lists previews as its catalog, as it does in its
    //  folder.
    TEST_METHOD (ShellPreview_ADiskImagePreviewsAsItsCatalog)
    {
        Host                              host;
        FakeRecycleBin                    shell;
        IShellItemVerbs::ShellFolderItem  image;

        image.name   = L"prodos.po";
        image.path   = L"C:\\Disks\\prodos.po";
        image.isFile = true;
        shell.shellItems = { image };
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"::{test}", L"Test"));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"prodos.po") });

        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Catalog);
    }


    //  A file of a type Windows previews gets Windows' preview; an assembler's
    //  source and an exported Apple file are Casso Explorer's own.
    TEST_METHOD (ShellPreview_ForTypesWindowsPreviews)
    {
        Host            host;
        FakeRecycleBin  shell;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\report.pdf", "%PDF"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\demo.pdf.a65", "; source"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\HELLO.Text.txt", "HELLO"));
        shell.previewExtension = L".pdf";
        host.browser.SetShellVerbs (&shell);
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"report.pdf") });
        Assert::IsTrue   (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Shell);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\report.pdf"), host.browser.GetPreviewer().GetPreview().hostPath);

        host.browser.GetPreviewer().GetShellItems().ShowShellPreviewBytes();
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex, L"A failed shell preview is replaced by the bytes");

        shell.previewExtension = L".txt";
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"HELLO.Text.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Text, L"An exported Apple file is Casso Explorer's");

        shell.previewExtension = L".a65";
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"demo.pdf.a65") });
        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Text, L"and so is an assembler's source");
    }


    //  A damaged zip, which the shell lists nothing in, has the reason in the preview
    //  and in the list rather than showing as an empty folder.
    TEST_METHOD (DamagedArchive_SaysWhyInThePreviewAndTheList)
    {
        Host            host;
        FakeRecycleBin  shell;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\broken.zip", std::string ("PK\x03\x04garbage", 11)));
        shell.archiveExtension = L".zip";
        host.browser.SetShellVerbs (&shell);
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"broken.zip") });
        Assert::IsTrue   (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Error);
        Assert::AreEqual (std::wstring (L"broken.zip can't be opened because it is damaged."), host.browser.GetPreviewer().GetPreview().message);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"C:\\Disks\\broken.zip", L"broken.zip"));
        Assert::AreEqual (std::wstring (L"broken.zip can't be opened because it is damaged."), host.browser.GetListError());
    }


    //  Extract all applies to an archive selected alone, or to the one open,
    //  from any folder inside it.
    TEST_METHOD (ExtractAll_TargetsTheSelectedOrOpenArchive)
    {
        Host            host;
        FakeRecycleBin  shell;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\notes.zip", "PK"));
        shell.archiveExtension = L".zip";
        host.browser.SetShellVerbs (&shell);
        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"notes.zip") });
        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), host.browser.GetArchives().GetExtractTarget());

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetArchives().GetExtractTarget().empty(), L"A plain file has none");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"notes.zip"), FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetArchives().GetExtractTarget().empty(), L"nor does a selection of several");

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"C:\\Disks\\notes.zip\\docs", L"docs"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), host.browser.GetArchives().GetExtractTarget(), L"Inside, the archive open");
    }


    //  A file inside an archive has no file of its own; its preview is its
    //  bytes, read through the shell.
    TEST_METHOD (Archive_FileInsidePreviewsItsBytes)
    {
        Host                              host;
        FakeRecycleBin                    shell;
        IShellItemVerbs::ShellFolderItem  inside;

        inside.id       = L"C:\\Disks\\notes.zip\\hello.txt";
        inside.name     = L"hello.txt";
        inside.typeText = L"Text Document";
        shell.shellItems.push_back (inside);
        shell.itemBytes = { 'h', 'i', '\r', '\n' };
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"C:\\Disks\\notes.zip", L"notes.zip"));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"hello.txt") });

        Assert::IsTrue   (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Hex);
        Assert::AreEqual ((size_t) 4, host.browser.GetPreviewer().GetPreview().bytes.size());
        Assert::IsTrue   (host.browser.GetPreviewer().GetPreview().textFile, L"Plain text opens showing its characters");
    }


    //  A disk image in a ZIP is listed as an image, and opens by the ZIP's
    //  path and its path in the ZIP.
    TEST_METHOD (Archive_ImageInsideOpensThroughTheZip)
    {
        static constexpr Byte  kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };

        InMemoryFileSystem                fs;
        FakeDiskFileIo                    io;
        ContainerDiskFileIo               nested  { io };
        CassoExplorerBrowser              browser { fs, nested };
        FakeRecycleBin                    shell;
        IShellItemVerbs::ShellFolderItem  inside;
        std::vector<Byte>                 image   = ScratchImages::GetDos33 (true);
        std::vector<Byte>                 zip;
        Location                          target;
        int                               row     = 0;



        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "disks/dos33.dsk", image, zip));
        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\set.zip", std::string (zip.begin(), zip.end())));
        io.files["C:\\Disks\\set.zip"]  = zip;
        io.stamps["C:\\Disks\\set.zip"] = FileStamp { zip.size(), 100 };

        inside.id       = L"C:\\Disks\\set.zip\\disks\\dos33.dsk";
        inside.name     = L"dos33.dsk";
        inside.typeText = L"Disk image";
        shell.shellItems.push_back (inside);
        browser.SetShellVerbs (&shell);

        browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"C:\\Disks\\set.zip\\disks", L"disks"));
        row = FindRow (browser, L"dos33.dsk");

        Assert::IsTrue (browser.GetRows()[(size_t) row].isDiskImage);
        Assert::IsTrue (BrowserRowPaths::TryGetRowLocation (browser.GetRowListing(), row, target));
        Assert::IsTrue (target.kind == Location::Kind::DiskImage);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\set.zip|disks/dos33.dsk"), target.path);

        Assert::IsTrue (browser.OpenRow (row));
        Assert::IsFalse (browser.GetRows().empty(), L"The image's catalog is listed");
        Assert::AreEqual (std::wstring (L"dos33.dsk"), BrowserCells::GetLocationLabel (target));
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


    TEST_METHOD (RecycleBin_ShellNameAndSavedTab_RoundTrip)
    {
        Host                host;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        Assert::IsTrue (host.browser.GetNavigator().NavigateToAddress (L"shell:RecycleBinFolder"));
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::RecycleBin);

        saved.tabs = { Location::MakeRecycleBin() };
        AssertSucceeded (loaded.FromJson (saved.ToJson()));

        Assert::AreEqual ((size_t) 1, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[0] == Location::MakeRecycleBin());
        Assert::AreEqual (std::wstring (L"Recycle Bin"), BrowserModel::FormatAddress (loaded.tabs[0]));
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


    //  A search's match inside a disk image previews and copies as it does in
    //  its image, found again there by where its catalog lists it.
    TEST_METHOD (SearchMatchInAnImage_PreviewsAndCopiesAsInItsImage)
    {
        Host                                            host;
        FakeRecycleBin                                  shell;
        IShellItemVerbs::ShellFolderItem                match;
        std::wstring                                    folder = host.OpenDisksFolder();
        std::vector<CassoExplorerBrowser::SearchMatch>  matches;
        size_t                                          index  = 0;


        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        for (const FileEntry & entry : host.browser.GetListing().entries)
        {
            index = (entry.name == "HELLO") ? entry.catalogIndex : index;
        }

        match.id           = L"C:\\Disks\\dos33.dsk\\HELLO";
        match.name         = L"HELLO";
        match.typeText     = L"Applesoft BASIC";
        match.folder       = L"C:\\Disks\\dos33.dsk";
        match.imagePath    = L"C:\\Disks\\dos33.dsk";
        match.catalogIndex = index;
        shell.shellItems.push_back (match);
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (kDisks, L"hello"), SearchQuery::GetLabel (kDisks)));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"HELLO") });

        Assert::IsTrue  (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Listing, L"The match previews as the program it is");
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().lines.empty());
        Assert::IsTrue  (host.browser.GetSearchMatches().AreSelectedRowsSearchMatches());

        host.browser.GetSearchMatches().GetSelectedSearchMatches (matches);
        Assert::AreEqual ((size_t) 1, matches.size(), L"and copies from its image");
        Assert::AreEqual (std::string ("HELLO"), matches[0].entry.name);
        Assert::IsTrue   (matches[0].kind == VolumeKind::Dos33);
    }


    //  Disk list's results: a row for each image, with its columns, which
    //  opens as the image and goes up to the folder walked.
    TEST_METHOD (DiskListResults_ListEachImage_AndGoUpToTheFolder)
    {
        Host            host;
        FakeRecycleBin  shell;
        std::wstring    id   = CollectionQuery::MakeId (CollectionQuery::Tool::DiskList, kDisks);
        Location        up;
        int             row  = 0;



        AssertSucceeded (CollectionResultsItems::Read (host.io, id, shell.shellItems));
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (id, CollectionQuery::GetLabel (CollectionQuery::Tool::DiskList, kDisks)));

        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());

        row = FindRow (host.browser, L"prodos.po");

        Assert::IsTrue   (host.browser.GetRows()[(size_t) row].isDiskImage);
        Assert::AreEqual (std::wstring (L"ProDOS"), host.browser.GetRows()[(size_t) row].collection.fileSystem);
        Assert::IsFalse  (host.browser.GetRows()[(size_t) row].collection.csvLine.empty());

        Assert::IsTrue   (host.browser.GetNavigator().TryGetUpTarget (up));
        Assert::IsTrue   (up.kind == Location::Kind::HostFolder);
        Assert::AreEqual (std::wstring (kDisks), up.path);
    }


    //  Find duplicates' results: two copies of one disk, in one set.
    TEST_METHOD (DuplicateResults_PutTheCopiesInOneSet)
    {
        Host            host;
        FakeRecycleBin  shell;
        std::wstring    id = CollectionQuery::MakeId (CollectionQuery::Tool::Duplicates, kDisks);



        host.SeedBytes (ScratchImages::GetProDos (true), L"C:\\Disks\\copy.po");

        AssertSucceeded (CollectionResultsItems::Read (host.io, id, shell.shellItems));
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (id, CollectionQuery::GetLabel (CollectionQuery::Tool::Duplicates, kDisks)));

        Assert::IsTrue   (host.browser.GetRows().size() >= 2);
        Assert::AreEqual (1, host.browser.GetRows()[(size_t) FindRow (host.browser, L"copy.po")].collection.set);
        Assert::AreEqual (1, host.browser.GetRows()[(size_t) FindRow (host.browser, L"prodos.po")].collection.set);
    }

    //  A match in a directory inside an image is read from that directory.
    TEST_METHOD (SearchMatchInAnImagesDirectory_PreviewsAndCopiesFromThere)
    {
        Host                                            host;
        FakeRecycleBin                                  shell;
        IShellItemVerbs::ShellFolderItem                match;
        VolumeListing                                   root;
        VolumeListing                                   inner;
        VolumeKind                                      kind      = VolumeKind::Unknown;
        std::string                                     directory;
        const FileEntry                               * file      = nullptr;
        std::vector<CassoExplorerBrowser::SearchMatch>  matches;


        //  The scratch ProDOS image's SUBDIR, with a file in it.
        host.SeedBytes (ScratchImages::GetProDos (false), L"C:\\Disks\\search.po");
        host.SeedBytes ({ 'h', 'i', '\n' }, L"C:\\In\\inner.txt");
        Assert::IsTrue (host.browser.GetOperations().Put ("C:\\Disks\\search.po", "C:\\In\\inner.txt", "SUBDIR/INNER", "T", false, 0, DiskOperations::Encoding::Text).Succeeded());
        Assert::IsTrue (host.browser.GetOperations().List ("C:\\Disks\\search.po", root, kind).Succeeded());

        for (const FileEntry & entry : root.entries)
        {
            directory = (directory.empty() && entry.isDirectory) ? entry.name : directory;
        }

        Assert::IsFalse (directory.empty(), L"this disk must have a subdirectory");
        Assert::IsTrue  (host.browser.GetOperations().List ("C:\\Disks\\search.po", directory, inner, kind).Succeeded());

        for (const FileEntry & entry : inner.entries)
        {
            file = (file == nullptr && !entry.isDirectory) ? &entry : file;
        }

        Assert::IsNotNull (file);

        match.id             = L"C:\\Disks\\search.po\\" + AppleFileName::ToDisplay (directory) + L"\\" + AppleFileName::ToDisplay (file->name);
        match.name           = AppleFileName::ToDisplay (file->name);
        match.imagePath      = L"C:\\Disks\\search.po";
        match.catalogIndex   = file->catalogIndex;
        match.imageDirectory = directory;
        shell.shellItems.push_back (match);
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (kDisks, L"x"), SearchQuery::GetLabel (kDisks)));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, match.name.c_str()) });

        Assert::IsTrue (host.browser.GetPreviewer().GetPreview().kind != PreviewContent::Kind::Error, L"It previews from its directory");

        host.browser.GetSearchMatches().GetSelectedSearchMatches (matches);
        Assert::AreEqual ((size_t) 1, matches.size());
        Assert::AreEqual (directory, matches[0].directory, L"and copies from there");
    }


    //  A search's result that is a file on the disk previews as it does in its
    //  own folder: a disk image shows its catalog.
    TEST_METHOD (SearchResultOnTheDisk_PreviewsAsInItsFolder)
    {
        Host                              host;
        FakeRecycleBin                    shell;
        IShellItemVerbs::ShellFolderItem  image;


        image.id     = L"C:\\Disks\\dos33.dsk";
        image.name   = L"dos33.dsk";
        image.path   = L"C:\\Disks\\dos33.dsk";
        image.folder = kDisks;
        shell.shellItems.push_back (image);
        host.browser.SetShellVerbs (&shell);

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (kDisks, L"dos"), SearchQuery::GetLabel (kDisks)));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"dos33.dsk") });

        Assert::IsTrue  (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Catalog, L"The image shows its catalog");
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().lines.empty());
        Assert::IsTrue  (host.browser.GetSelection().AreSelectedRowsImages(), L"and is an image, for its thumbnail and inspector");
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

        host.browser.GetPreviewer().GetViewState().SetHexForm (PreviewDecoder::HexForm::Disassembly);
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



    //  A shell folder's rows sort by name in the folder's own order, as
    //  Explorer's do, and other rows by name, numbers by their value.
    TEST_METHOD (SortByName_UsesAShellFoldersOwnOrder)
    {
        std::vector<CatalogRow>  rows (3);

        rows[0].name = L"Alpha";    rows[0].shellOrder = 3;
        rows[1].name = L"Zulu";     rows[1].shellOrder = 1;
        rows[2].name = L"Mike";     rows[2].shellOrder = 2;

        CatalogModel::Sort (rows, CatalogModel::Column::Name, false);
        Assert::IsTrue (rows[0].name == L"Zulu" && rows[1].name == L"Mike" && rows[2].name == L"Alpha");

        CatalogModel::Sort (rows, CatalogModel::Column::Name, true);
        Assert::IsTrue (rows[0].name == L"Alpha" && rows[2].name == L"Zulu", L"and descending reverses it");

        for (CatalogRow & row : rows)
        {
            row.shellOrder = 0;
        }

        rows[0].name = L"f10";
        rows[1].name = L"f2";
        rows[2].name = L"e";
        CatalogModel::Sort (rows, CatalogModel::Column::Name, false);
        Assert::IsTrue (rows[0].name == L"e" && rows[1].name == L"f2" && rows[2].name == L"f10");
    }


    //  The desktop's order puts a public shortcut ahead of the user's folders.
    TEST_METHOD (SortByName_PlacesAShellFoldersFilesWhereItsOrderPutsThem)
    {
        std::vector<CatalogRow>  rows (3);

        rows[0].name = L"Folder";     rows[0].shellOrder = 3;    rows[0].isDirectory = true;
        rows[1].name = L"Shortcut";   rows[1].shellOrder = 2;
        rows[2].name = L"Recycle";    rows[2].shellOrder = 1;    rows[2].isDirectory = true;

        CatalogModel::Sort (rows, CatalogModel::Column::Name, false);
        Assert::IsTrue (rows[0].name == L"Recycle" && rows[1].name == L"Shortcut" && rows[2].name == L"Folder");

        CatalogModel::Sort (rows, CatalogModel::Column::Name, true);
        Assert::IsTrue (rows[0].name == L"Folder" && rows[1].name == L"Shortcut" && rows[2].name == L"Recycle", L"Descending reverses the folder's order whole, as File Explorer's desktop does");
    }



    //  A folder with a name in its desktop.ini sorts by that name, the one it
    //  is listed by, as Explorer's do.
    TEST_METHOD (SortByName_UsesTheNameShown)
    {
        std::vector<CatalogRow>  rows (2);

        rows[0].name  = L"iCloudPhotos";
        rows[1].name  = L"iCloudDrive";
        rows[1].label = L"iCloud Drive";

        CatalogModel::Sort (rows, CatalogModel::Column::Name, false);
        Assert::AreEqual (std::wstring (L"iCloudDrive"), rows[0].name, L"\"iCloud Drive\" sorts ahead of \"iCloudPhotos\" by its space");

        CatalogModel::Sort (rows, CatalogModel::Column::Name, true);
        Assert::AreEqual (std::wstring (L"iCloudPhotos"), rows[0].name);
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


    TEST_METHOD (SortByColumn_TogglesAndKeepsSelection)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        std::wstring  selected;

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        host.browser.GetRowOrder().SortByColumn ((int) CatalogModel::Column::Name);
        Assert::IsTrue (host.browser.GetBrowserModel().GetActiveTab().sortDescending);

        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        selected = host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name;
        Assert::AreEqual (std::wstring (L"NOTES"), selected);

        host.browser.GetRowOrder().SortByColumn ((int) CatalogModel::Column::Size);
        Assert::IsFalse (host.browser.GetBrowserModel().GetActiveTab().sortDescending);
    }


    TEST_METHOD (SetGroupBy_OrdersRowsByGroup_AndKeepsSelection)
    {
        Host                              host;
        std::wstring                      folder = host.OpenDisksFolder();
        std::vector<DxuiListView::Group>  groups;
        std::wstring                      selected;

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        host.browser.GetRowOrder().SetGroupBy (RowGrouping::Field::Name, false);
        groups = host.browser.GetRowOrder().GetListGroups();

        Assert::AreEqual ((size_t) 2, groups.size());
        Assert::AreEqual (std::wstring (L"A ") + s_kpszEnDash + L" H", groups[0].label);
        Assert::AreEqual (std::wstring (L"I ") + s_kpszEnDash + L" P", groups[1].label);
        Assert::AreEqual (0, groups[0].firstRow);

        for (int r = 0; r < groups[1].firstRow; r++)
        {
            Assert::IsTrue (host.browser.GetRows()[(size_t) r].name[0] <= L'H');
        }

        selected = host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name;
        Assert::AreEqual (std::wstring (L"NOTES"), selected);

        host.browser.GetRowOrder().SetGroupBy (RowGrouping::Field::None, false);
        Assert::IsTrue (host.browser.GetRowOrder().GetListGroups().empty());
    }


    //  A column's filter lists the groups of every row in the folder, keeps
    //  the rows in a checked group, and ends when the folder changes.
    TEST_METHOD (ColumnFilter_KeepsTheCheckedGroups_UntilTheFolderChanges)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        size_t        all    = 0;

        Assert::IsTrue (BrowserColumnFilters::GetFilterField ((size_t) CatalogModel::Column::Type) == RowGrouping::Field::Type);
        Assert::IsTrue (BrowserColumnFilters::GetFilterField ((size_t) CatalogModel::Column::Address) ==RowGrouping::Field::None);

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        all = host.browser.GetRows().size();

        Assert::AreEqual ((size_t) 2, host.browser.GetColumnFilters().GetFilterChoices (RowGrouping::Field::Name).size());

        host.browser.GetColumnFilters().ToggleFilter (RowGrouping::Field::Name, std::wstring (L"A ") + s_kpszEnDash + L" H");
        AssertSucceeded (host.browser.Reload (true));

        Assert::IsTrue (host.browser.GetColumnFilters().IsFiltering (RowGrouping::Field::Name));
        Assert::IsTrue (host.browser.GetRows().size() < all);

        for (const CatalogRow & row : host.browser.GetRows())
        {
            Assert::IsTrue (row.name[0] <= L'H');
        }

        Assert::AreEqual ((size_t) 2, host.browser.GetColumnFilters().GetFilterChoices (RowGrouping::Field::Name).size(), L"The choices include the rows filtered out");

        host.browser.GetColumnFilters().ToggleFilter (RowGrouping::Field::Name, std::wstring (L"A ") + s_kpszEnDash + L" H");
        AssertSucceeded (host.browser.Reload (true));
        Assert::IsFalse  (host.browser.GetColumnFilters().IsFiltering (RowGrouping::Field::Name), L"Unchecking the last group ends the filter");
        Assert::AreEqual (all, host.browser.GetRows().size());

        host.browser.GetColumnFilters().ToggleFilter (RowGrouping::Field::Name, std::wstring (L"A ") + s_kpszEnDash + L" H");
        AssertSucceeded (host.browser.SelectTreeNode (folder));
        Assert::IsFalse (host.browser.GetColumnFilters().IsFiltering (RowGrouping::Field::Name), L"Another folder starts unfiltered");
    }


    //  As File Explorer's filter does, a folder's Unspecified size is listed
    //  after the sizes.
    TEST_METHOD (ColumnFilter_ListsUnspecifiedLast)
    {
        Host          host;
        std::wstring  folder;

        AssertSucceeded (host.fs.MakeDirectory (L"C:\\Disks\\Sub"));
        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\Sub\\note.txt", "hi"));
        folder = host.OpenDisksFolder();
        AssertSucceeded (host.browser.SelectTreeNode (folder));

        const std::vector<RowGrouping::Group> &  choices = host.browser.GetColumnFilters().GetFilterChoices (RowGrouping::Field::Size);

        Assert::IsTrue (choices.size() >= 2);
        Assert::AreEqual (std::wstring (L"Unspecified"), choices.back().label);
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
