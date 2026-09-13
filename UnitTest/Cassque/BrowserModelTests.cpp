#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Cassque/Model/BrowserModel.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModelTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserModelTests)
{
public:

    static Location Folder (const wchar_t * path) { return Location::MakeHostFolder (path); }
    static Location Image  (const wchar_t * path) { return Location::MakeDiskImage (path); }



    TEST_METHOD (Tabs_OpenCloseSwitchAndCycle)
    {
        BrowserModel  model;

        Assert::IsFalse (model.HasTabs());

        Assert::AreEqual ((size_t) 0, model.OpenTab (Folder (L"C:\\A")));
        Assert::AreEqual ((size_t) 1, model.OpenTab (Folder (L"C:\\B")));
        Assert::AreEqual ((size_t) 2, model.OpenTab (Folder (L"C:\\C")));
        Assert::AreEqual ((size_t) 2, model.GetActiveIndex());
        Assert::AreEqual ((size_t) 0, model.GetNextIndex(), L"Ctrl+Tab wraps");

        Assert::IsTrue   (model.SwitchTo (0));
        Assert::AreEqual ((size_t) 1, model.GetNextIndex());
        Assert::IsFalse  (model.SwitchTo (7));

        Assert::IsTrue   (model.CloseTab (0));
        Assert::AreEqual ((size_t) 2, model.GetTabCount());
        Assert::AreEqual ((size_t) 0, model.GetActiveIndex());
        Assert::IsTrue   (model.GetActiveTab().location == Folder (L"C:\\B"));

        Assert::IsTrue   (model.SwitchTo (1));
        Assert::IsTrue   (model.CloseTab (1));
        Assert::AreEqual ((size_t) 0, model.GetActiveIndex(), L"closing the last tab moves to the one before it");

        Assert::IsTrue   (model.CloseTab (0));
        Assert::IsFalse  (model.HasTabs());
        Assert::IsFalse  (model.CloseTab (0));
    }



    TEST_METHOD (Tabs_MoveKeepsTheActiveTabActive)
    {
        BrowserModel  model;

        model.OpenTab (Folder (L"C:\\A"));
        model.OpenTab (Folder (L"C:\\B"));
        model.OpenTab (Folder (L"C:\\C"));

        Assert::IsTrue   (model.MoveTab (0, 2));
        Assert::IsTrue   (model.GetTab (0).location == Folder (L"C:\\B"));
        Assert::IsTrue   (model.GetTab (2).location == Folder (L"C:\\A"));
        Assert::AreEqual ((size_t) 1, model.GetActiveIndex(), L"C, the active tab, moved left one place and stays active");

        Assert::IsTrue   (model.MoveTab (1, 0));
        Assert::AreEqual ((size_t) 0, model.GetActiveIndex(), L"and moving the active tab itself carries the active index with it");
        Assert::IsTrue   (model.GetActiveTab().location == Folder (L"C:\\C"));

        Assert::IsFalse  (model.MoveTab (0, 3));
    }



    TEST_METHOD (History_BackAndForwardPerTab)
    {
        BrowserModel  model;

        model.OpenTab (Folder (L"C:\\A"));

        Assert::IsFalse (model.CanGoBack());
        Assert::IsFalse (model.CanGoForward());

        model.NavigateTo (Image (L"C:\\A\\one.dsk"));
        model.NavigateTo (Image (L"C:\\A\\two.dsk"));

        Assert::IsTrue  (model.CanGoBack());
        Assert::IsFalse (model.CanGoForward());

        Assert::IsTrue  (model.GoBack());
        Assert::IsTrue  (model.GetActiveTab().location == Image (L"C:\\A\\one.dsk"));
        Assert::IsTrue  (model.CanGoForward());

        Assert::IsTrue  (model.GoBack());
        Assert::IsTrue  (model.GetActiveTab().location == Folder (L"C:\\A"));
        Assert::IsFalse (model.CanGoBack());
        Assert::IsFalse (model.GoBack());

        Assert::IsTrue  (model.GoForward());
        Assert::IsTrue  (model.GoForward());
        Assert::IsTrue  (model.GetActiveTab().location == Image (L"C:\\A\\two.dsk"));
        Assert::IsFalse (model.GoForward());

        //  Navigating somewhere new drops the forward stack.
        Assert::IsTrue  (model.GoBack());
        model.NavigateTo (Folder (L"C:\\B"));
        Assert::IsFalse (model.CanGoForward());

        //  A second tab has a history of its own.
        model.OpenTab (Folder (L"C:\\Z"));
        Assert::IsFalse (model.CanGoBack());
    }



    TEST_METHOD (Selection_SurvivesASortAndNotANavigation)
    {
        BrowserModel  model;

        model.OpenTab (Image (L"C:\\A\\one.dsk"));
        model.SetSelection ({ L"HELLO", L"NOTES" });
        model.SetSort (CatalogModel::Column::Size, true);

        Assert::AreEqual ((size_t) 2, model.GetActiveTab().selection.size());
        Assert::IsTrue   (model.GetActiveTab().sortColumn == CatalogModel::Column::Size);
        Assert::IsTrue   (model.GetActiveTab().sortDescending);

        model.NavigateTo (Image (L"C:\\A\\two.dsk"));

        Assert::IsTrue (model.GetActiveTab().selection.empty(), L"a new location has no selection yet");
        Assert::IsTrue (model.GetActiveTab().sortColumn == CatalogModel::Column::Size, L"the sort is the tab's");
    }



    TEST_METHOD (CatalogCache_AnswersUntilAWriteInvalidatesIt)
    {
        BrowserModel   model;
        VolumeListing  listing;
        VolumeListing  cached;
        VolumeKind     kind = VolumeKind::Unknown;

        listing.volumeNumber    = 42;
        listing.hasVolumeNumber = true;

        model.OpenTab (Image (L"C:\\A\\one.dsk"));
        model.CacheCatalog (L"C:\\A\\one.dsk", listing, VolumeKind::Dos33);

        Assert::IsTrue   (model.TryGetCachedCatalog (L"c:\\a\\ONE.DSK", cached, kind), L"paths compare case-insensitively");
        Assert::AreEqual ((Byte) 42, cached.volumeNumber);
        Assert::IsTrue   (kind == VolumeKind::Dos33);

        //  A second tab sees the first tab's cache.
        model.OpenTab (Image (L"C:\\A\\one.dsk"));
        Assert::IsTrue (model.TryGetCachedCatalog (L"C:\\A\\one.dsk", cached, kind));

        model.InvalidateCatalog (L"C:\\A\\one.dsk");
        Assert::IsFalse (model.TryGetCachedCatalog (L"C:\\A\\one.dsk", cached, kind));

        model.CacheCatalog (L"C:\\A\\one.dsk", listing, VolumeKind::Dos33);
        model.InvalidateCatalog (L"C:\\A\\other.dsk");
        Assert::IsTrue (model.TryGetCachedCatalog (L"C:\\A\\one.dsk", cached, kind), L"another image's write leaves this cache alone");

        model.InvalidateAllCatalogs();
        Assert::IsFalse (model.TryGetCachedCatalog (L"C:\\A\\one.dsk", cached, kind));
    }



    TEST_METHOD (Locations_ListEveryTabForPersistence)
    {
        BrowserModel           model;
        std::vector<Location>  locations;

        model.OpenTab (Folder (L"C:\\A"));
        model.OpenTab (Image (L"C:\\B\\x.po"));
        model.GetLocations (locations);

        Assert::AreEqual ((size_t) 2, locations.size());
        Assert::IsTrue   (locations[1] == Image (L"C:\\B\\x.po"));
    }
};
