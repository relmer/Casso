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
//  CassoExplorerSortAndGroupTests
//
//  Sorting, grouping and column filters in the browser, with the selection
//  kept.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerSortAndGroupTests)
{
public:

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



    //  A row listed under two groups is one item: it is selected once, and a
    //  second regrouping doesn't multiply the copies.
    TEST_METHOD (GroupedByAValueOfTwoParts_ListsEachRowTwiceAndSelectsItOnce)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        size_t        rows   = 0;
        int           first  = -1;
        int           second = -1;

        AssertSucceeded (host.browser.SelectTreeNode (folder));

        rows = host.browser.GetRows().size();

        host.browser.GetRowOrder().SetPropertyColumns (1, [] (size_t, size_t)
        {
            CatalogModel::PropertyKey  key;

            key.has   = true;
            key.text  = L"a; b";
            key.parts = { L"a", L"b" };

            return key;
        });

        host.browser.GetRowOrder().SetGroupBy (RowGrouping::MakePropertyField (0), false);
        Assert::AreEqual (rows * 2, host.browser.GetRows().size());

        host.browser.GetRowOrder().SetGroupBy (RowGrouping::MakePropertyField (0), false);
        Assert::AreEqual (rows * 2, host.browser.GetRows().size(), L"Regrouping drops the last copies first");

        for (int row = 0; row < (int) host.browser.GetRows().size(); row++)
        {
            if (host.browser.GetRows()[(size_t) row].name == L"readme.txt")
            {
                (first < 0 ? first : second) = row;
            }
        }

        host.browser.GetSelection().SetSelectedRows ({ first, second });
        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());

        host.browser.GetRowOrder().SetGroupBy (RowGrouping::Field::None, false);
        Assert::AreEqual (rows, host.browser.GetRows().size(), L"Ungrouped, each is listed once");
    }
};
