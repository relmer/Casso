#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "../UiTests/InMemoryFileSystem.h"
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
        Assert::AreEqual ((size_t) 0, model.GetActiveIndex(), L"and moving the active tab itself moves the active index with it");
        Assert::IsTrue   (model.GetActiveTab().location == Folder (L"C:\\C"));

        Assert::IsFalse  (model.MoveTab (0, 3));
    }



    TEST_METHOD (Address_SegmentsFromTheDriveDown)
    {
        Location                                   inner    = Location::MakeDiskDirectory (L"C:\\Disks\\work.po", "SUB/INNER");
        std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (inner);

        Assert::AreEqual ((size_t) 5, segments.size());
        Assert::AreEqual (std::wstring (L"C:"),      segments[0].label);
        Assert::IsTrue   (segments[0].location == Folder (L"C:\\"));
        Assert::AreEqual (std::wstring (L"Disks"),   segments[1].label);
        Assert::IsTrue   (segments[1].location == Folder (L"C:\\Disks"));
        Assert::AreEqual (std::wstring (L"work.po"), segments[2].label);
        Assert::IsTrue   (segments[2].location == Image (L"C:\\Disks\\work.po"));
        Assert::AreEqual (std::wstring (L"SUB"),     segments[3].label);
        Assert::IsTrue   (segments[3].location == Location::MakeDiskDirectory (L"C:\\Disks\\work.po", "SUB"));
        Assert::AreEqual (std::wstring (L"INNER"),   segments[4].label);
        Assert::IsTrue   (segments[4].location == inner, L"The last segment is where the tab is");
    }



    TEST_METHOD (Address_ProfileFolderIsOneSegmentUnderItsOwnName)
    {
        BrowserModel::AddressRoot                  root    { L"C:\\Users\\relmer", L"Rob Elmer" };
        std::vector<BrowserModel::AddressSegment>  inside  = BrowserModel::GetAddressSegments (Folder (L"C:\\Users\\relmer\\source\\repos"), root);
        std::vector<BrowserModel::AddressSegment>  outside = BrowserModel::GetAddressSegments (Folder (L"C:\\Users\\relmerx"), root);

        Assert::AreEqual ((size_t) 3, inside.size());
        Assert::AreEqual (std::wstring (L"Rob Elmer"), inside[0].label, L"The profile folder reads as its owner's name, as in Explorer");
        Assert::IsTrue   (inside[0].location == Folder (L"C:\\Users\\relmer"));
        Assert::AreEqual (std::wstring (L"source"), inside[1].label);
        Assert::IsTrue   (inside[2].location == Folder (L"C:\\Users\\relmer\\source\\repos"));

        Assert::AreEqual ((size_t) 3, outside.size(), L"A folder whose name only starts the same way is not inside it");
        Assert::AreEqual (std::wstring (L"C:"), outside[0].label);
    }



    TEST_METHOD (Address_HomeAndShareRoots)
    {
        std::vector<BrowserModel::AddressSegment>  home  = BrowserModel::GetAddressSegments (Location());
        std::vector<BrowserModel::AddressSegment>  share = BrowserModel::GetAddressSegments (Folder (L"\\\\server\\share\\Apple"));

        Assert::AreEqual ((size_t) 1, home.size());
        Assert::AreEqual (std::wstring (L"Home"), home[0].label);

        Assert::AreEqual ((size_t) 2, share.size());
        Assert::AreEqual (std::wstring (L"\\\\server\\share"), share[0].label, L"A share's root is its server and share together");
        Assert::IsTrue   (share[1].location == Folder (L"\\\\server\\share\\Apple"));
    }



    TEST_METHOD (Address_FormatsAndParsesBackToTheSameLocation)
    {
        InMemoryFileSystem  fs;
        Location            parsed;
        HRESULT             hr = S_OK;
        Location            folder = Folder (L"C:\\Disks");
        Location            image  = Image  (L"C:\\Disks\\work.po");
        Location            inner  = Location::MakeDiskDirectory (L"C:\\Disks\\work.po", "SUB/INNER");

        hr = fs.WriteAllText (L"C:\\Disks\\work.po", "x");
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::AreEqual (std::wstring (L"C:\\Disks\\work.po\\SUB\\INNER"), BrowserModel::FormatAddress (inner));
        Assert::AreEqual (std::wstring(),                                    BrowserModel::FormatAddress (Location()));

        for (const Location & location : { folder, image, inner })
        {
            Assert::IsTrue (BrowserModel::ParseAddress (fs, BrowserModel::FormatAddress (location), parsed));
            Assert::IsTrue (parsed == location, L"A formatted address reads back as the same location");
        }
    }



    TEST_METHOD (Address_ParseRefusesWhatIsNotThere)
    {
        InMemoryFileSystem  fs;
        Location            parsed;
        HRESULT             hr = S_OK;

        hr = fs.WriteAllText (L"C:\\Disks\\notes.txt", "x");
        Assert::IsTrue (SUCCEEDED (hr));

        Assert::IsFalse (BrowserModel::ParseAddress (fs, L"C:\\Nowhere",          parsed));
        Assert::IsFalse (BrowserModel::ParseAddress (fs, L"C:\\Disks\\notes.txt", parsed), L"A file that is not a disk image is no location");
        Assert::IsFalse (BrowserModel::ParseAddress (fs, L"C:\\Disks\\gone.po",   parsed), L"nor is an image that does not exist");
        Assert::IsFalse (BrowserModel::ParseAddress (fs, L"   ",                  parsed));

        Assert::IsTrue  (BrowserModel::ParseAddress (fs, L"  \"C:\\Disks\\\"  ", parsed), L"Quotes, spaces and a trailing separator are ignored");
        Assert::IsTrue  (parsed == Folder (L"C:\\Disks"));
    }



    TEST_METHOD (Address_FolderChildrenAreFoldersThenImages)
    {
        InMemoryFileSystem                         fs;
        std::vector<BrowserModel::AddressSegment>  children;
        HRESULT                                    hr = S_OK;

        hr = fs.WriteAllText (L"C:\\Disks\\zed\\one.po", "x");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (L"C:\\Disks\\Apple\\two.po", "x");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (L"C:\\Disks\\b.po", "x");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (L"C:\\Disks\\a.dsk", "x");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (L"C:\\Disks\\notes.txt", "x");
        Assert::IsTrue (SUCCEEDED (hr));

        BrowserModel::GetFolderChildren (fs, Folder (L"C:\\Disks"), children);

        Assert::AreEqual ((size_t) 4, children.size(), L"A file that is not a disk image is not listed");
        Assert::AreEqual (std::wstring (L"Apple"), children[0].label);
        Assert::IsTrue   (children[0].location == Folder (L"C:\\Disks\\Apple"));
        Assert::AreEqual (std::wstring (L"zed"),   children[1].label, L"Folders are in name order, without regard to case");
        Assert::AreEqual (std::wstring (L"a.dsk"), children[2].label, L"and the images follow them");
        Assert::IsTrue   (children[2].location == Image (L"C:\\Disks\\a.dsk"));
        Assert::AreEqual (std::wstring (L"b.po"),  children[3].label);

        BrowserModel::GetFolderChildren (fs, Image (L"C:\\Disks\\b.po"), children);
        Assert::IsTrue   (children.empty(), L"An image lists nothing");
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
