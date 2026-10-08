#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Core/AppleSingleCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeRecycleBin
//
//  The shell's verbs with only the Recycle Bin's listing answered.
//
////////////////////////////////////////////////////////////////////////////////

class FakeRecycleBin : public IShellItemVerbs
{
public:
    std::vector<RecycledItem>     items;
    std::vector<ShellFolderItem>  shellItems;

    std::vector<ShellFolderItem>  navRoots;
    std::vector<ShellFolderItem>  pinned;

    HRESULT  ListShellFolder (const std::wstring &, std::vector<ShellFolderItem> & outItems) override  { outItems = shellItems; return S_OK; }
    HRESULT  ListNavigationRoots (std::vector<ShellFolderItem> & outRoots) override                   { outRoots = navRoots; return S_OK; }
    HRESULT  ListPinnedFolders   (std::vector<ShellFolderItem> & outFolders) override                 { outFolders = pinned; return S_OK; }

    HRESULT  Open                (HWND, const std::wstring &) override                                  { return E_NOTIMPL; }
    HRESULT  GetOpenWithHandlers (const std::wstring &, std::vector<Handler> &) override                 { return E_NOTIMPL; }
    HRESULT  OpenWith            (HWND, const std::wstring &, size_t) override                          { return E_NOTIMPL; }
    HRESULT  ChooseOtherApp      (HWND, const std::wstring &) override                                  { return E_NOTIMPL; }
    HRESULT  ShowShellMenu       (HWND, const std::vector<std::wstring> &, POINT) override              { return E_NOTIMPL; }
    HRESULT  Recycle             (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
    HRESULT  RestoreRecycled     (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
    HRESULT  ListRecycled        (std::vector<RecycledItem> & outItems) override                        { outItems = items; return S_OK; }
    HRESULT  RunRecycledVerb     (HWND, const std::vector<std::wstring> &, RecycledVerb) override       { return E_NOTIMPL; }
    HRESULT  ShowRecycledMenu    (HWND, const std::vector<std::wstring> &, POINT) override              { return E_NOTIMPL; }
    HRESULT  EmptyRecycleBin     (HWND) override                                                        { return E_NOTIMPL; }
    HRESULT  RenameItem          (HWND, const std::wstring &, const std::wstring &) override            { return E_NOTIMPL; }
    HRESULT  PlaceOnClipboard    (HWND, const std::vector<std::wstring> &, bool) override               { return E_NOTIMPL; }
    bool     ClipboardHasFiles   () const override                                                      { return false; }
    HRESULT  PasteInto           (HWND, const std::wstring &, PasteResult &) override                   { return E_NOTIMPL; }
    HRESULT  MoveItemsTo         (HWND, const std::vector<std::wstring> &, const std::vector<std::wstring> &) override { return E_NOTIMPL; }
    HRESULT  CreateFolder        (HWND, const std::wstring &, const std::wstring &) override            { return E_NOTIMPL; }
    HRESULT  Share               (HWND, const std::vector<std::wstring> &) override                     { return E_NOTIMPL; }
};





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

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";



    struct Host
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        CassoExplorerBrowser      browser { fs, io };

        Host()
        {
            Seed ("CassoExplorer/dos33.dsk", L"C:\\Disks\\dos33.dsk");
            Seed ("CassoExplorer/prodos.po", L"C:\\Disks\\prodos.po");
            AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\readme.txt", "hello"));

            browser.GetTreeModel().SetKnownFolders ({ kDisks });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0; });
        }

        void Seed (const char * fixture, const wchar_t * path)
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

        //  Expands the Casso root and returns the known folder's node id.
        std::wstring OpenDisksFolder()
        {
            std::vector<DxuiTreeNode>  roots;

            browser.GetTreeRoots (roots);

            std::vector<DxuiTreeNode>  folders = browser.GetTreeChildren (roots[0].id);

            Assert::AreEqual ((size_t) 1, folders.size());

            return folders[0].id;
        }

        std::wstring FindChildId (const std::wstring & parentId, const wchar_t * label)
        {
            for (const DxuiTreeNode & node : browser.GetTreeChildren (parentId))
            {
                if (node.label == label)
                {
                    return node.id;
                }
            }

            Assert::Fail (L"child not found");

            return std::wstring();
        }
    };



    static int FindRow (const CassoExplorerBrowser & browser, const wchar_t * name)
    {
        const std::vector<CatalogRow> &  rows = browser.GetRows();

        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].name == name)
            {
                return (int) i;
            }
        }

        Assert::Fail (L"row not found");

        return -1;
    }



    TEST_METHOD (AppleTypeIcons_FollowDos33AndProDosTypes)
    {
        using Kind = IShellIcons::Kind;

        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"A")   == Kind::AppleApplesoft);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"BAS") == Kind::AppleApplesoft);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"I")   == Kind::AppleInteger);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"T")   == Kind::AppleText);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"BIN") == Kind::AppleBinary);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"SYS") == Kind::AppleSystem);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"REL") == Kind::AppleRelocatable);
        Assert::IsTrue (CassoExplorerBrowser::GetAppleTypeIconKind (L"$F5") == Kind::File, L"Any other type is a plain file");
    }



    TEST_METHOD (TreeRoots_AreCollapsedAndLazy)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeRoots (roots);

        Assert::AreEqual ((size_t) 2, roots.size(), L"Casso and This PC; the bin needs all folders shown");
        Assert::IsFalse  (roots[0].expanded);
        Assert::IsFalse  (roots[0].childrenLoaded);
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

        Assert::IsTrue  (host.browser.NavigateToAddress (L"recycle bin"));
        Assert::IsTrue  (host.browser.GetLocation() == Location::MakeRecycleBin());
        Assert::AreEqual ((size_t) 2, host.browser.GetRows().size());

        row = FindRow (host.browser, L"notes.txt");

        std::vector<DxuiListView::Cell>  cells = CassoExplorerBrowser::ToCells (host.browser.GetRows()[(size_t) row], host.browser.GetLocation());

        Assert::AreEqual (std::wstring (L"C:\\Docs"), cells[(size_t) CatalogModel::Column::OriginalLocation].text);
        Assert::IsFalse  (cells[(size_t) CatalogModel::Column::DateDeleted].text.empty());
        Assert::AreEqual (std::wstring (L"Recycle Bin"), CassoExplorerBrowser::GetLocationLabel (host.browser.GetLocation()));

        host.browser.SetSelectedRows ({ row });

        std::vector<std::wstring>  ids;

        host.browser.GetSelectedRecycledIds (ids);
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

        Assert::IsTrue (host.browser.NavigateToAddress (L"shell:Libraries"));
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::ShellFolder);
        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());

        Assert::IsTrue (host.browser.TryGetRowLocation (FindRow (host.browser, L"Disks"), target));
        Assert::IsTrue (target == Location::MakeHostFolder (L"C:\\Disks"), L"A folder on a disk opens as one");

        Assert::IsTrue (host.browser.TryGetRowLocation (FindRow (host.browser, L"Documents"), target));
        Assert::IsTrue (target.kind == Location::Kind::ShellFolder, L"A library opens through the shell");
        Assert::AreEqual (std::wstring (L"Documents"), target.label);

        Assert::IsTrue (host.browser.TryGetRowLocation (FindRow (host.browser, L"SERVER"), target));
        Assert::IsTrue (target == Location::MakeShellFolder (L"::{NET}\\SERVER", L""));

        saved.tabs.push_back (Location::MakeShellFolder (L"::{NET}\\SERVER", L"SERVER"));
        AssertSucceeded (saved.Save (L"C:\\Prefs", host.fs));
        AssertSucceeded (loaded.Load (L"C:\\Prefs", host.fs));

        Assert::AreEqual ((size_t) 1, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[0].kind == Location::Kind::ShellFolder);
        Assert::AreEqual (std::wstring (L"SERVER"), loaded.tabs[0].label, L"A saved tab keeps the name it shows");
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

        shell.navRoots = { home, oneDrive, pc, libraries, network, bin };
        shell.pinned   = { pin };
        host.browser.SetShellVerbs (&shell);
        host.browser.GetTreeRoots (roots);

        for (const DxuiTreeNode & root : roots)
        {
            labels.push_back (root.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"Casso", L"Home", L"OneDrive", L"Disks", L"This PC", L"Network" }),
                        L"As File Explorer's pane: libraries hidden there, and the bin only with all folders shown");
        Assert::IsTrue (roots[3].dividerAbove, L"A line above the pinned folders");
        Assert::IsTrue (roots[4].dividerAbove, L"and one below them");
        Assert::IsTrue (roots[4].id == TreeModel::kThisPcRootId, L"Casso's own This PC, with its drives");
        Assert::IsTrue (roots[1].childrenLoaded, L"Home has no folders under it");
        Assert::IsFalse (roots[5].childrenLoaded, L"Network does");

        //  The pane's own menu turns each root on and off, apart from File
        //  Explorer's choice once set.
        options.showThisPc     = false;
        options.showNetwork    = false;
        options.showLibraries  = true;
        options.showAllFolders = true;
        host.browser.GetTreeModel().SetNavPaneOptions (options);
        host.browser.GetTreeRoots (roots);
        labels.clear();

        for (const DxuiTreeNode & root : roots)
        {
            labels.push_back (root.label);
        }

        Assert::IsTrue (labels == std::vector<std::wstring> ({ L"Casso", L"Home", L"OneDrive", L"Disks", L"Libraries", L"Recycle Bin" }));
        Assert::IsTrue (roots[4].dividerAbove, L"The line moves to the first root left");
    }


    TEST_METHOD (RecycleBin_ShellNameAndSavedTab_RoundTrip)
    {
        Host                host;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        Assert::IsTrue (host.browser.NavigateToAddress (L"shell:RecycleBinFolder"));
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

        Assert::IsTrue  (host.browser.NavigateToAddress (L"C:\\Disks"));
        Assert::IsFalse (host.browser.NavigateToAddress (L"C:\\Nowhere"), L"A path that goes nowhere navigates nowhere");

        Assert::AreEqual ((size_t) 1, host.browser.GetTypedPaths().GetEntries().size(), L"and is not recorded");
        Assert::AreEqual (std::wstring (L"C:\\Disks"), host.browser.GetTypedPaths().GetEntries()[0]);

        //  What the history dropdown does with a pick: submit the text again.
        Assert::IsTrue (host.browser.NavigateToAddress (L"C:\\Disks\\prodos.po"));
        Assert::IsTrue (host.browser.NavigateToAddress (L"C:\\Disks"));

        Assert::AreEqual ((size_t) 2, host.browser.GetTypedPaths().GetEntries().size(), L"A repeat does not appear twice");
        Assert::AreEqual (std::wstring (L"C:\\Disks"), host.browser.GetTypedPaths().GetEntries()[0], L"and moves to the top");
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
        Assert::IsTrue (CassoExplorerBrowser::ToCells (row)[0].iconGhosted, L"and drawn faded");

        options.showProtected = true;
        host.browser.SetFolderOptions (options);
        AssertSucceeded (host.browser.Reload (true));
        Assert::AreEqual ((size_t) 7, host.browser.GetRows().size(), L"Protected items shown");

        //  A compressed name is colored only when Explorer colors them.
        row = host.browser.GetRows()[(size_t) FindRow (host.browser, L"packed.txt")];
        Assert::AreEqual (0u, CassoExplorerBrowser::GetNameArgb (row, options, true));

        options.colorCompressed = true;
        Assert::AreEqual (CassoExplorerBrowser::kCompressedDarkArgb,  CassoExplorerBrowser::GetNameArgb (row, options, true));
        Assert::AreEqual (CassoExplorerBrowser::kCompressedLightArgb, CassoExplorerBrowser::GetNameArgb (row, options, false));
        Assert::AreEqual (0u, CassoExplorerBrowser::GetNameArgb (host.browser.GetRows()[(size_t) FindRow (host.browser, L"system.txt")], options, true));
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

        cells = CassoExplorerBrowser::ToCells (drive);

        Assert::AreEqual ((size_t) 2, cells[0].tileLines.size());
        Assert::AreEqual (0.75f, cells[0].tileLines[0].meter, 0.001f);
        Assert::AreEqual (CassoExplorerBrowser::FormatSize (drive.freeBytes) + L" free of " + CassoExplorerBrowser::FormatSize (drive.sizeBytes),
                          cells[0].tileLines[1].text);

        folder.name        = L"Games";
        folder.isDirectory = true;

        Assert::IsTrue (CassoExplorerBrowser::ToCells (folder)[0].tileLines.empty(), L"A folder's tile shows its columns");
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
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"GAME.as") });

        Assert::IsTrue (host.browser.GetPreview().kind == PreviewContent::Kind::Details);

        for (const auto & [label, value] : host.browser.GetPreview().details)
        {
            named |= label == L"Name"     && value == L"GAME";
            typed |= label == L"Aux type" && value == L"$6000";
            dated |= label == L"Modified" && value == L"2000-01-02 00:00 UTC";
        }

        Assert::IsTrue (named, L"The real name");
        Assert::IsTrue (typed, L"The aux type");
        Assert::IsTrue (dated, L"The date, counted from 2000");

        host.browser.SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreview().kind == PreviewContent::Kind::Hex, L"Any other file still shows its bytes");
    }


    TEST_METHOD (SelectingFiles_PreviewsByType)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        host.browser.SetSelectedRows ({ FindRow (host.browser, L"HELLO") });
        Assert::IsTrue (host.browser.GetPreview().kind == PreviewContent::Kind::Listing);
        Assert::IsFalse (host.browser.GetPreview().lines.empty());

        host.browser.SetSelectedRows ({ FindRow (host.browser, L"PICTURE") });
        Assert::IsTrue (host.browser.GetPreview().kind == PreviewContent::Kind::Picture);

        host.browser.SetSelectedRows ({ FindRow (host.browser, L"ODD") });
        Assert::IsTrue (host.browser.GetPreview().kind == PreviewContent::Kind::Hex);

        host.browser.SetDisassemble (true);
        Assert::IsFalse (host.browser.GetPreview().lines.empty());
    }


    TEST_METHOD (MultipleSelection_PreviewsNothingAndCounts)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.SetSelectedRows ({ 0, 1 });

        Assert::IsTrue (host.browser.GetPreview().lines.empty());
        Assert::AreEqual (0u, (unsigned) host.browser.GetStatus().selected.find (L"2 items selected"), L"the selection has a field of its own");
    }


    TEST_METHOD (ImageRowInFolder_PreviewsItsCatalog)
    {
        Host  host;

        AssertSucceeded (host.browser.SelectTreeNode (host.OpenDisksFolder()));
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"prodos.po") });

        Assert::IsTrue  (host.browser.GetPreview().kind == PreviewContent::Kind::Catalog);
        Assert::IsFalse (host.browser.GetPreview().rows.empty());
    }


    TEST_METHOD (SortByColumn_TogglesAndKeepsSelection)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        std::wstring  selected;

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        host.browser.SortByColumn ((int) CatalogModel::Column::Name);
        Assert::IsTrue (host.browser.GetBrowserModel().GetActiveTab().sortDescending);

        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        selected = host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name;
        Assert::AreEqual (std::wstring (L"NOTES"), selected);

        host.browser.SortByColumn ((int) CatalogModel::Column::Size);
        Assert::IsFalse (host.browser.GetBrowserModel().GetActiveTab().sortDescending);
    }


    TEST_METHOD (SetGroupBy_OrdersRowsByGroup_AndKeepsSelection)
    {
        Host                              host;
        std::wstring                      folder = host.OpenDisksFolder();
        std::vector<DxuiListView::Group>  groups;
        std::wstring                      selected;

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        host.browser.SetGroupBy (RowGrouping::Field::Name, false);
        groups = host.browser.GetListGroups();

        Assert::AreEqual ((size_t) 2, groups.size());
        Assert::AreEqual (std::wstring (L"A - H"), groups[0].label);
        Assert::AreEqual (std::wstring (L"I - P"), groups[1].label);
        Assert::AreEqual (0, groups[0].firstRow);

        for (int r = 0; r < groups[1].firstRow; r++)
        {
            Assert::IsTrue (host.browser.GetRows()[(size_t) r].name[0] <= L'H');
        }

        selected = host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name;
        Assert::AreEqual (std::wstring (L"NOTES"), selected);

        host.browser.SetGroupBy (RowGrouping::Field::None, false);
        Assert::IsTrue (host.browser.GetListGroups().empty());
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
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"dos33.dsk") });

        Assert::IsTrue (host.browser.GetStatus().freeSpace.ends_with (L" free of 140 KB"));
        Assert::IsTrue (host.browser.GetStatus().freeSpaceTip.starts_with (L"dos33.dsk, DOS 3.3 volume "));

        host.browser.SetSelectedRows ({});
        Assert::IsTrue (host.browser.GetStatus().freeSpace.empty(), L"Nothing for a folder");
    }


    TEST_METHOD (LeaveMissingLocation_MovesToTheFolderAbove)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        Assert::IsFalse (host.browser.LeaveMissingLocation());

        AssertSucceeded (host.fs.Delete (L"C:\\Disks\\dos33.dsk"));
        Assert::IsTrue (host.browser.LeaveMissingLocation());

        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::HostFolder);
        Assert::AreEqual (std::wstring (kDisks), host.browser.GetLocation().path);
    }


    TEST_METHOD (CorruptImage_ShowsItsErrorInTheList)
    {
        Host          host;
        std::wstring  folder;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\bad.dsk", "not a disk"));
        folder = host.OpenDisksFolder();

        host.browser.SelectTreeNode (host.FindChildId (folder, L"bad.dsk"));

        Assert::IsTrue  (host.browser.GetRows().empty());
        Assert::AreEqual (std::wstring (L"bad.dsk is not a valid .dsk image.\nFile size is 10 bytes but should be 143,360 bytes."),
                          host.browser.GetListError(), L"The file by its name, and why; the path is in the address bar");
    }


    TEST_METHOD (ImageError_NamesTheFileNotItsPath)
    {
        Assert::AreEqual (std::wstring (L"a.po is empty."),
                          CassoExplorerBrowser::FormatImageError (L"C:\\Disks\\a.po", "C:\\Disks\\a.po: is empty\n"));
        Assert::AreEqual (std::wstring (L"Something else."),
                          CassoExplorerBrowser::FormatImageError (L"C:\\Disks\\a.po", "Something else"), L"A message of another form is kept");
    }


    TEST_METHOD (ShortenPaths_CutsEachPathToItsName)
    {
        Assert::AreEqual (std::wstring (L"broken.dsk: is 26 bytes but should be 143,360 bytes"),
                          CassoExplorerBrowser::ShortenPaths (L"C:\\Users\\me\\Temp\\t077\\ui\\broken.dsk: is 26 bytes but should be 143,360 bytes"));
        Assert::AreEqual (std::wstring (L"a.po: HELLO: wrote out.bin."),
                          CassoExplorerBrowser::ShortenPaths (L"C:\\My Disks\\a.po: HELLO: wrote D:\\Out Dir\\out.bin."),
                          L"Paths with spaces, mid-sentence, and before a period");
        Assert::AreEqual (std::wstring (L"one\nDisks\ntwo.dsk"),
                          CassoExplorerBrowser::ShortenPaths (L"one\nC:\\Disks\\\nE:\\x\\two.dsk"), L"A folder keeps its own name");
        Assert::AreEqual (std::wstring (L"No path at all, just a colon: see."),
                          CassoExplorerBrowser::ShortenPaths (L"No path at all, just a colon: see."));
    }


    TEST_METHOD (Cells_LeaveBlankWhatTheEntryDoesNotRecord)
    {
        CatalogRow                        row;
        std::vector<DxuiListView::Cell>   cells;

        row.name        = L"SUB";
        row.typeText    = L"DIR";
        row.isDirectory = true;
        row.hasModified = false;

        cells = CassoExplorerBrowser::ToCells (row);

        Assert::AreEqual (CassoExplorerBrowser::GetColumns().size(), cells.size());
        Assert::IsTrue   (cells[(size_t) CatalogModel::Column::Size].text.empty(),     L"a folder has no size");
        Assert::IsTrue   (cells[(size_t) CatalogModel::Column::Modified].text.empty(), L"and no date was recorded");
    }


    TEST_METHOD (KnownFolderNode_ShowsTheFolderName)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeRoots (roots);

        Assert::AreEqual (std::wstring (L"Disks"), host.browser.GetTreeChildren (roots[0].id)[0].label);
    }


    TEST_METHOD (SelectRoot_ListsItsChildrenAndOpensThem)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeRoots (roots);

        AssertSucceeded (host.browser.SelectTreeNode (roots[0].id));
        Assert::AreEqual ((size_t) 1, host.browser.GetRows().size());
        Assert::IsTrue   (host.browser.GetRows()[0].isDirectory);

        Assert::IsTrue   (host.browser.OpenRow (0));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
        Assert::AreEqual ((size_t) 3, host.browser.GetRows().size());
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
        Assert::AreEqual (std::wstring (L"This PC"), host.browser.GetTabLabel (0), L"The tab shows the root's label, not Home");
        Assert::AreEqual (std::wstring (L"This PC"), BrowserModel::FormatAddress (host.browser.GetLocation()));

        segments = BrowserModel::GetAddressSegments (host.browser.GetLocation());
        Assert::AreEqual ((size_t) 1, segments.size());
        Assert::AreEqual (std::wstring (L"This PC"), segments[0].label);

        //  The root is in the history like any other location.
        AssertSucceeded (host.browser.SelectTreeNode (TreeModel::kCassoRootId));
        Assert::AreEqual (std::wstring (L"Casso"), host.browser.GetTabLabel (0));
        Assert::AreEqual ((size_t) 1, host.browser.GetRows().size());

        Assert::IsTrue   (host.browser.GoBack());
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeRoot (TreeModel::kThisPcRootId));
        Assert::IsFalse  (host.browser.CanGoUp());

        //  And its label, typed, goes there.
        Assert::IsTrue   (host.browser.NavigateToAddress (L"casso"));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeRoot (TreeModel::kCassoRootId));
        Assert::AreEqual ((size_t) 1, host.browser.GetRows().size());
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

        Assert::IsTrue (host.browser.GoUp());
        Assert::IsTrue (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));

        Assert::IsTrue (host.browser.GoBack());
        Assert::IsTrue (host.browser.GetLocation().kind == Location::Kind::DiskImage);

        Assert::IsTrue (host.browser.GoForward());
        Assert::IsTrue (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
        Assert::IsTrue (host.browser.CanGoUp());
    }


    TEST_METHOD (ParentFolder_StopsAtTheDriveRoot)
    {
        Assert::AreEqual (std::wstring (L"C:\\Disks"), CassoExplorerBrowser::GetParentFolder (L"C:\\Disks\\Sub"));
        Assert::AreEqual (std::wstring (L"C:\\"),      CassoExplorerBrowser::GetParentFolder (L"C:\\Disks\\"));
        Assert::IsTrue   (CassoExplorerBrowser::GetParentFolder (L"C:\\").empty());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), CassoExplorerBrowser::JoinPath (L"C:\\Disks", L"a.dsk"));
        Assert::AreEqual (std::wstring (L"C:\\a.dsk"),        CassoExplorerBrowser::JoinPath (L"C:\\", L"a.dsk"));
    }


    TEST_METHOD (CatalogPreviewCells_MatchTheirColumns)
    {
        CatalogRow  row;

        row.name      = L"HELLO";
        row.typeText  = L"A";
        row.sizeBytes = 512;

        Assert::AreEqual (CassoExplorerBrowser::GetCatalogPreviewColumns().size(), CassoExplorerBrowser::ToCatalogPreviewCells (row).size());
    }


    TEST_METHOD (Reload_KeepsTheSelectionByName)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        AssertSucceeded (host.browser.Reload (true));

        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        Assert::AreEqual (std::wstring (L"NOTES"), host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name);

        AssertSucceeded (host.browser.Reload (false));
        Assert::IsTrue (host.browser.GetSelectedRows().empty());
    }


    TEST_METHOD (KnownFolders_CanBeRemovedAndOtherFoldersAdded)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;
        std::wstring               known;
        std::wstring               path;

        host.browser.GetTreeRoots (roots);
        known = host.browser.GetTreeChildren (roots[0].id)[0].id;

        Assert::IsTrue  (host.browser.CanRemoveFromCasso (known));
        Assert::IsFalse (host.browser.CanAddToCasso (known));
        Assert::IsTrue  (host.browser.TryGetNodePath (known, path));
        Assert::AreEqual (std::wstring (kDisks), path);

        Assert::IsFalse (host.browser.CanRemoveFromCasso (roots[0].id));
        Assert::IsFalse (host.browser.CanAddToCasso (roots[1].id));
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


    TEST_METHOD (Tabs_KeepTheirOwnLocationAndSelection)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        //  A new tab opens at home, not on a copy of the tab it came from.
        Assert::AreEqual ((size_t) 1, host.browser.NewTab());
        Assert::AreEqual (std::wstring (L"Home"), host.browser.GetTabLabel (1));

        AssertSucceeded  (host.browser.SelectTreeNode (folder));
        Assert::AreEqual (std::wstring (L"Disks"), host.browser.GetTabLabel (1));

        Assert::IsTrue   (host.browser.SwitchTab (0));
        Assert::AreEqual (std::wstring (L"dos33.dsk"), host.browser.GetTabLabel (0));
        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        Assert::AreEqual (std::wstring (L"NOTES"), host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name);

        Assert::IsTrue   (host.browser.CloseTab (0));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
        Assert::IsFalse  (host.browser.CloseTab (0));
    }


    //  The tab menu's commands: open somewhere in a new tab, copy a tab, close
    //  the tabs after one, and close every tab but one. The last tab stays.
    TEST_METHOD (TabMenu_OpenDuplicateAndClose)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        Location      image  = Location::MakeDiskImage (L"C:\\Disks\\prodos.po");

        AssertSucceeded (host.browser.SelectTreeNode (folder));

        Assert::AreEqual ((size_t) 1, host.browser.OpenInNewTab (image));
        Assert::IsTrue   (host.browser.GetLocation() == image, L"A new tab opens where it was asked to, and shows");

        Assert::AreEqual ((size_t) 2, host.browser.DuplicateTab (1));
        Assert::IsTrue   (host.browser.GetBrowserModel().GetTab (2).location == image, L"A copy is at the same place");

        Assert::IsTrue   (host.browser.CloseTabsToRight (1));
        Assert::AreEqual ((size_t) 2, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsFalse  (host.browser.CloseTabsToRight (1), L"The last tab has none to its right");

        Assert::IsTrue   (host.browser.CloseOtherTabs (1));
        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation() == image, L"The tab kept is the one the menu was opened on");
        Assert::IsFalse  (host.browser.CloseOtherTabs (0), L"and with no others, nothing closes");
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

        Assert::IsTrue   (host.browser.TryGetRowLocation (FindRow (host.browser, L"dos33.dsk"), location));
        Assert::IsTrue   (location == Location::MakeDiskImage (L"C:\\Disks\\dos33.dsk"));
        Assert::IsFalse  (host.browser.TryGetRowLocation (FindRow (host.browser, L"readme.txt"), location), L"A plain file leads nowhere");

        Assert::IsTrue   (host.browser.TryGetRowPath (FindRow (host.browser, L"readme.txt"), path));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\readme.txt"), path);

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));

        Assert::IsTrue   (host.browser.TryGetRowPath (FindRow (host.browser, L"NOTES"), path));
        Assert::IsTrue   (path.rfind (L"C:\\Disks\\dos33.dsk", 0) == 0, L"An entry in an image starts at the image");
        Assert::IsTrue   (path.size() > 5 && path.compare (path.size() - 5, 5, L"NOTES") == 0, L"and ends at the entry");
    }


    TEST_METHOD (RestoreTabs_OpensEachAndActivatesTheFirst)
    {
        Host  host;

        host.OpenDisksFolder();
        host.browser.RestoreTabs ({ Location::MakeDiskImage (L"C:\\Disks\\prodos.po"), Location::MakeHostFolder (kDisks) });

        Assert::AreEqual ((size_t) 2, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::DiskImage);
        Assert::IsFalse  (host.browser.GetRows().empty());
    }


    TEST_METHOD (RestoreTabs_NothingSaved_OpensOneHomeTab)
    {
        Host  host;

        host.browser.RestoreTabs ({});

        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount(), L"a first run has a tab");
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::None, L"at Home");
        Assert::IsFalse  (host.browser.GetStatus().selection.empty(), L"and gives the item count");
    }


    TEST_METHOD (LocationLabels)
    {
        Assert::AreEqual (std::wstring (L"C:\\"),    CassoExplorerBrowser::GetLocationLabel (Location::MakeHostFolder (L"C:\\")));
        Assert::AreEqual (std::wstring (L"Disks"),   CassoExplorerBrowser::GetLocationLabel (Location::MakeHostFolder (L"C:\\Disks\\")));
        Assert::AreEqual (std::wstring (L"SUB"),     CassoExplorerBrowser::GetLocationLabel (Location::MakeDiskDirectory (L"C:\\a.po", "/CASSQUE/SUB")));
    }


    TEST_METHOD (Formatting)
    {
        Assert::AreEqual (std::wstring (L"777 bytes"), CassoExplorerBrowser::FormatSize (777));
        Assert::AreEqual (std::wstring (L"8.00 KB"),   CassoExplorerBrowser::FormatSize (8192));

        //  The Size column, as Explorer's: whole kilobytes, rounded up.
        Assert::AreEqual (std::wstring (L"0 KB"),   CassoExplorerBrowser::FormatSizeColumn (0));
        Assert::AreEqual (std::wstring (L"1 KB"),   CassoExplorerBrowser::FormatSizeColumn (187));
        Assert::AreEqual (std::wstring (L"110 KB"), CassoExplorerBrowser::FormatSizeColumn (112537));
        Assert::AreEqual (std::wstring (L"1 item"),    CassoExplorerBrowser::FormatSelection (0, 1));
        Assert::AreEqual (std::wstring (L"7 items, 3 selected"), CassoExplorerBrowser::FormatSelection (3, 7));

        //  Explorer's selection field, the size after two spaces.
        Assert::AreEqual (std::wstring (L"1 item selected"),           CassoExplorerBrowser::FormatSelected (1, 0, false));
        Assert::AreEqual (std::wstring (L"4 items selected  777 bytes"), CassoExplorerBrowser::FormatSelected (4, 777, true));

        //  1984-08-17 12:34, as the scratch ProDOS volume records it.
        //  In the user's own short date and time, whatever they are set to.
        {
            SYSTEMTIME  st       = { 1984, 8, 0, 17, 12, 34, 0, 0 };
            wchar_t     date[80] = {};
            wchar_t     time[80] = {};
            int         dateLen  = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, 80, nullptr);
            int         timeLen  = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, time, 80);

            Assert::IsTrue   (dateLen > 0 && timeLen > 0);
            Assert::AreEqual (std::wstring (date) + L" " + time, CassoExplorerBrowser::FormatModified (461594040, true));
        }
    }
};
