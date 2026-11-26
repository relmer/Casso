#include "Pch.h"

#include "CassoExplorer/Browser/BrowserNavigator.h"
#include "CassoExplorer/Browser/IBrowserListing.h"
#include "CassoExplorer/Model/HostFolderReader.h"
#include "../UiTests/InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNavigatorTests
//
//  The tabs and Back, Forward and Up over a fake listing, which counts the
//  reloads the navigator calls for.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserNavigatorTests)
{
public:

    class FakeListing : public IBrowserListing
    {
    public:
        void      ReloadAfterNavigation() override                                           { reloads++; }
        void      ReturnToTab           (const std::vector<std::wstring> & rowKeys) override { returns++; lastKeys = rowKeys; }
        bool      IsOnMissingDrive      (const Location & location) const override           { return location.path == L"Q:\\Gone"; }
        bool      IsHomeLocation() const override                                            { return false; }
        void      RequestHomeRead() override                                                 { homeReads++; }
        Location  GetShellParent() const override                                            { return Location(); }

        int                        reloads   = 0;
        int                        returns   = 0;
        int                        homeReads = 0;
        std::vector<std::wstring>  lastKeys;
    };


    struct Rig
    {
        InMemoryFileSystem  fs;
        HostFolderReader    hostFolders;
        BrowserModel        model;
        FakeListing         listing;
        BrowserNavigator    navigator   { model, fs, hostFolders, listing };
    };



    TEST_METHOD (NavigateToLocation_OpensAFirstTab_AndReloads)
    {
        Rig  rig;



        rig.navigator.NavigateToLocation (Location::MakeHostFolder (L"C:\\Disks"));

        Assert::AreEqual ((size_t) 1, rig.model.GetTabCount());
        Assert::AreEqual (1, rig.listing.reloads);
        Assert::AreEqual (std::wstring (L"C:\\Disks"), rig.model.GetActiveTab().location.path);
    }



    TEST_METHOD (GoBackAndForward_MoveThroughHistory)
    {
        Rig  rig;



        rig.navigator.NavigateToLocation (Location::MakeHostFolder (L"C:\\Disks"));
        rig.navigator.NavigateToLocation (Location::MakeHostFolder (L"C:\\Disks\\Sub"));

        Assert::IsTrue   (rig.navigator.GoBack());
        Assert::AreEqual (std::wstring (L"C:\\Disks"), rig.model.GetActiveTab().location.path);
        Assert::IsTrue   (rig.navigator.GoForward());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Sub"), rig.model.GetActiveTab().location.path);
        Assert::IsFalse  (rig.navigator.GoForwardBy (2), L"Nothing ahead to move to");
        Assert::AreEqual (4, rig.listing.reloads);
    }



    TEST_METHOD (TryGetUpTarget_AnImageGoesToItsFolder)
    {
        Rig       rig;
        Location  target;



        rig.navigator.NavigateToLocation (Location::MakeDiskImage (L"C:\\Disks\\a.dsk"));

        Assert::IsTrue   (rig.navigator.CanGoUp());
        Assert::IsTrue   (rig.navigator.TryGetUpTarget (target));
        Assert::AreEqual (std::wstring (L"C:\\Disks"), target.path);
        Assert::IsTrue   (target.kind == Location::Kind::HostFolder);
    }



    TEST_METHOD (SwitchTab_ReturnsToTheTabsSelection)
    {
        Rig  rig;



        rig.navigator.NavigateToLocation (Location::MakeHostFolder (L"C:\\Disks"));
        rig.model.SetSelection ({ L"0:a.dsk" });
        rig.navigator.NewTab();

        Assert::IsTrue   (rig.navigator.SwitchTab (0));
        Assert::AreEqual (1, rig.listing.returns);
        Assert::AreEqual ((size_t) 1, rig.listing.lastKeys.size());
        Assert::AreEqual (std::wstring (L"0:a.dsk"), rig.listing.lastKeys[0]);
    }



    TEST_METHOD (CloseTab_TheLastTabStaysOpen)
    {
        Rig  rig;



        rig.navigator.NewTab();
        Assert::IsFalse (rig.navigator.CloseTab (0));

        rig.navigator.NewTab();
        Assert::IsTrue   (rig.navigator.CloseTab (0));
        Assert::AreEqual ((size_t) 1, rig.model.GetTabCount());
    }



    TEST_METHOD (RestoreTabs_LeavesOutTabsOnAMissingDrive)
    {
        Rig  rig;



        rig.navigator.RestoreTabs ({ Location::MakeHostFolder (L"Q:\\Gone"), Location::MakeHostFolder (L"C:\\Disks") });

        Assert::AreEqual ((size_t) 1, rig.model.GetTabCount());
        Assert::AreEqual (std::wstring (L"C:\\Disks"), rig.model.GetActiveTab().location.path);
    }



    TEST_METHOD (RootOnSystemDrive_KeepsAPathWithADriveOrAUncRoot)
    {
        Assert::AreEqual (std::wstring (L"C:\\Disks"),        BrowserNavigator::RootOnSystemDrive (L"C:\\Disks"));
        Assert::AreEqual (std::wstring (L"\\\\server\\share"), BrowserNavigator::RootOnSystemDrive (L"\\\\server\\share"));
        Assert::AreEqual (std::wstring (L"no variables"),     BrowserNavigator::ExpandVariables   (L"no variables"));
    }
};
