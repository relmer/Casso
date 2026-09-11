#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/CassqueBrowser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueBrowserTests
//
//  The browser's view state over an in-memory host holding the two scratch
//  images: tree nodes for the widget, list rows for a folder and for an
//  image, the preview a selection produces, sorting with the selection kept,
//  and the status text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueBrowserTests)
{
public:

    static constexpr const wchar_t *  kDisks = L"C:\\Disks";



    struct Host
    {
        InMemoryFileSystem  fs;
        FakeDiskFileIo      io;
        CassqueBrowser      browser { fs, io };

        Host()
        {
            Seed ("Cassque/dos33.dsk", L"C:\\Disks\\dos33.dsk");
            Seed ("Cassque/prodos.po", L"C:\\Disks\\prodos.po");
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



    static int FindRow (const CassqueBrowser & browser, const wchar_t * name)
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



    TEST_METHOD (TreeRoots_AreCollapsedAndLazy)
    {
        Host                       host;
        std::vector<DxuiTreeNode>  roots;

        host.browser.GetTreeRoots (roots);

        Assert::AreEqual ((size_t) 2, roots.size());
        Assert::IsFalse  (roots[0].expanded);
        Assert::IsFalse  (roots[0].childrenLoaded);
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
        Assert::AreNotEqual (std::wstring::npos, host.browser.GetStatus().selection.find (L"2 selected"));
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


    TEST_METHOD (CorruptImage_ShowsItsErrorInTheList)
    {
        Host          host;
        std::wstring  folder;

        AssertSucceeded (host.fs.WriteAllText (L"C:\\Disks\\bad.dsk", "not a disk"));
        folder = host.OpenDisksFolder();

        host.browser.SelectTreeNode (host.FindChildId (folder, L"bad.dsk"));

        Assert::IsTrue  (host.browser.GetRows().empty());
        Assert::IsFalse (host.browser.GetListError().empty());
    }


    TEST_METHOD (Cells_LeaveBlankWhatTheEntryDoesNotRecord)
    {
        CatalogRow                        row;
        std::vector<DxuiListView::Cell>   cells;

        row.name        = L"SUB";
        row.typeText    = L"DIR";
        row.isDirectory = true;
        row.hasModified = false;

        cells = CassqueBrowser::ToCells (row);

        Assert::AreEqual (CassqueBrowser::GetColumns().size(), cells.size());
        Assert::IsTrue   (cells[2].text.empty());
        Assert::IsTrue   (cells[5].text.empty());
    }


    TEST_METHOD (Formatting)
    {
        Assert::AreEqual (std::wstring (L"777 bytes"), CassqueBrowser::FormatSize (777));
        Assert::AreEqual (std::wstring (L"8.0 KB"),    CassqueBrowser::FormatSize (8192));
        Assert::AreEqual (std::wstring (L"1 item"),    CassqueBrowser::FormatSelection (0, 1));
        Assert::AreEqual (std::wstring (L"7 items, 3 selected"), CassqueBrowser::FormatSelection (3, 7));
    }
};
