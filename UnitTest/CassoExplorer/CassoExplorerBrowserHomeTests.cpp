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
//  CassoExplorerBrowserHomeTests
//
//  The browser's tabs, Home and its groups, and restoring the saved tabs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerBrowserHomeTests)
{
public:

    TEST_METHOD (Tabs_KeepTheirOwnLocationAndSelection)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();

        AssertSucceeded (host.browser.SelectTreeNode (host.FindChildId (folder, L"dos33.dsk")));
        host.browser.GetSelection().SetSelectedRows ({ FindRow (host.browser, L"NOTES") });

        //  A new tab opens at home, not on a copy of the tab it came from.
        Assert::AreEqual ((size_t) 1, host.browser.GetNavigator().NewTab());
        Assert::AreEqual (std::wstring (L"Home"), host.browser.GetCells().GetTabLabel (1));

        AssertSucceeded  (host.browser.SelectTreeNode (folder));
        Assert::AreEqual (std::wstring (L"Disks"), host.browser.GetCells().GetTabLabel (1));

        Assert::IsTrue   (host.browser.GetNavigator().SwitchTab (0));
        Assert::AreEqual (std::wstring (L"dos33.dsk"), host.browser.GetCells().GetTabLabel (0));
        Assert::AreEqual ((size_t) 1, host.browser.GetSelectedRows().size());
        Assert::AreEqual (std::wstring (L"NOTES"), host.browser.GetRows()[host.browser.GetSelectedRows()[0]].name);

        Assert::IsTrue   (host.browser.GetNavigator().CloseTab (0));
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
        Assert::IsFalse  (host.browser.GetNavigator().CloseTab (0));
    }


    //  The tab menu's commands: open somewhere in a new tab, copy a tab, close
    //  the tabs after one, and close every tab but one. The last tab stays.
    TEST_METHOD (TabMenu_OpenDuplicateAndClose)
    {
        Host          host;
        std::wstring  folder = host.OpenDisksFolder();
        Location      image  = Location::MakeDiskImage (L"C:\\Disks\\prodos.po");

        AssertSucceeded (host.browser.SelectTreeNode (folder));

        Assert::AreEqual ((size_t) 1, host.browser.GetNavigator().OpenInNewTab (image));
        Assert::IsTrue   (host.browser.GetLocation() == image, L"A new tab opens where it was asked to, and shows");

        Assert::AreEqual ((size_t) 2, host.browser.GetNavigator().DuplicateTab (1));
        Assert::IsTrue   (host.browser.GetBrowserModel().GetTab (2).location == image, L"A copy is at the same place");

        Assert::IsTrue   (host.browser.GetNavigator().CloseTabsToRight (1));
        Assert::AreEqual ((size_t) 2, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsFalse  (host.browser.GetNavigator().CloseTabsToRight (1), L"The last tab has none to its right");

        Assert::IsTrue   (host.browser.GetNavigator().CloseOtherTabs (1));
        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation() == image, L"The tab kept is the one the menu was opened on");
        Assert::IsFalse  (host.browser.GetNavigator().CloseOtherTabs (0), L"and with no others, nothing closes");
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


    TEST_METHOD (RestoreTabs_OpensEachAndActivatesTheFirst)
    {
        Host  host;

        host.OpenDisksFolder();
        host.browser.GetNavigator().RestoreTabs ({ Location::MakeDiskImage (L"C:\\Disks\\prodos.po"), Location::MakeHostFolder (kDisks) });

        Assert::AreEqual ((size_t) 2, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::DiskImage);
        Assert::IsFalse  (host.browser.GetRows().empty());
    }


    TEST_METHOD (RestoreTabs_DriveNotAttached_LeavesItsTabClosed)
    {
        Host  host;

        host.OpenDisksFolder();
        host.browser.GetNavigator().RestoreTabs ({ Location::MakeHostFolder (L"H:\\Games"), Location::MakeHostFolder (kDisks) });

        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation() == Location::MakeHostFolder (kDisks));
    }


    TEST_METHOD (RestoreTabs_NoDriveAttached_OpensOneHomeTab)
    {
        Host  host;

        host.browser.GetNavigator().RestoreTabs ({ Location::MakeHostFolder (L"H:\\Games") });

        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount());
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::None);
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

    TEST_METHOD (RestoreTabs_NothingSaved_OpensOneHomeTab)
    {
        Host  host;

        host.browser.GetNavigator().RestoreTabs ({});

        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount(), L"a first run has a tab");
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::None, L"at Home");
        Assert::IsFalse  (host.browser.GetStatus().selection.empty(), L"and gives the item count");
    }
};
