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
//  CassoExplorerTabsTests
//
//  The browser's tabs: each tab's location and selection, the tab menu, and
//  the tabs restored at startup.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerTabsTests)
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



    TEST_METHOD (RestoreTabs_NothingSaved_OpensOneHomeTab)
    {
        Host  host;

        host.browser.GetNavigator().RestoreTabs ({});

        Assert::AreEqual ((size_t) 1, host.browser.GetBrowserModel().GetTabCount(), L"a first run has a tab");
        Assert::IsTrue   (host.browser.GetLocation().kind == Location::Kind::None, L"at Home");
        Assert::IsFalse  (host.browser.GetStatus().selection.empty(), L"and gives the item count");
    }
};
