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
//  CassoExplorerSearchResultTests
//
//  Search results in the browser: matches inside an image or its directory,
//  each image of a disk list, and duplicates set together.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerSearchResultTests)
{
public:

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
        Assert::IsTrue  (host.browser.GetPreviewer().AreSelectedRowsSearchMatches());

        host.browser.GetPreviewer().GetSelectedSearchMatches (matches);
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

        host.browser.GetPreviewer().GetSelectedSearchMatches (matches);
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
};
