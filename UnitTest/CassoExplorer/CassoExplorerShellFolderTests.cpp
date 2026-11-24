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
//  CassoExplorerShellFolderTests
//
//  Shell folders, archives and the Recycle Bin in the browser: what each lists,
//  how its items preview and open, and Extract all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerShellFolderTests)
{
public:

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

        host.browser.GetPreviewer().ShowShellPreviewBytes();
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
        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), host.browser.GetPreviewer().GetExtractTarget());

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetExtractTarget().empty(), L"A plain file has none");

        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"notes.zip"), FindRow (host.browser, L"readme.txt") });
        Assert::IsTrue (host.browser.GetPreviewer().GetExtractTarget().empty(), L"nor does a selection of several");

        host.browser.GetNavigator().NavigateToLocation (Location::MakeShellFolder (L"C:\\Disks\\notes.zip\\docs", L"docs"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\notes.zip"), host.browser.GetPreviewer().GetExtractTarget(), L"Inside, the archive open");
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
};
