#include "Pch.h"

#include "AppleDriveRig.h"
#include "CassoExplorer/Actions/ActionsDriveImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveImageActionsTests
//
//  ActionsDriveImage over a FakeApple: a drive's disk read into an image and
//  written back, formatted, and the file name of a drive's image. Each write is
//  checked on the disk in the FakeApple.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveImageActionsTests)
{
public:

    using Rig = AppleDriveRig;


    TEST_METHOD (ReadDiskToImage_WritesTheDrivesDisk)
    {
        Rig   rig;
        bool  offered = false;

        rig.Open (1);
        AssertSucceeded (rig.actions->GetDriveImages().ReadDiskToImage (L"C:\\Images\\out.dsk", false, [&offered] (const LinkOperation &, const std::string &, int) { offered = true; }).hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsFalse (offered);
        Assert::IsTrue  (rig.io.files["C:\\Images\\out.dsk"] == rig.Drive (1));
    }



    TEST_METHOD (ReadDiskToImage_WithUnreadableSectors_OffersToBlankThem)
    {
        Rig                        rig;
        std::vector<std::wstring>  messages;
        size_t                     offered = 0;
        std::string                link;
        int                        drive   = 0;

        rig.apple.badSectors = { { 4, 2 } };

        rig.Open (1);
        AssertSucceeded (rig.actions->GetDriveImages().ReadDiskToImage (L"C:\\Images\\out.dsk", false,
                                                                        [&] (const LinkOperation & operation, const std::string & readLink, int readDrive)
                                                                        {
                                                                            offered = operation.unreadable.size();
                                                                            link    = readLink;
                                                                            drive   = readDrive;
                                                                        }).hr);
        messages = rig.Finish();

        Assert::IsTrue   (messages.empty(), L"offered instead of reported");
        Assert::AreEqual ((size_t) 1, offered);
        Assert::AreEqual (1, drive, L"the offer is given the drive that was read");
        Assert::IsTrue   (rig.io.files.find ("C:\\Images\\out.dsk") == rig.io.files.end());

        //  Read again by the link and drive the offer was given, not by the location.
        AssertSucceeded (rig.actions->GetDriveImages().ReadDiskToImage (link, drive, L"C:\\Images\\out.dsk", true, false, {}).hr);
        Assert::IsTrue (rig.Finish().empty());
        Assert::IsTrue (rig.io.files.find ("C:\\Images\\out.dsk") != rig.io.files.end(), L"read again with the sector as zeros");
    }



    //  The connections every window shares run a finished job's completion
    //  later; one whose window, and so whose actions, have closed by then
    //  is not run.
    TEST_METHOD (ACompletion_IsDropped_OnceItsActionsAreGone)
    {
        Rig   rig;
        bool  offered = false;

        rig.apple.badSectors = { { 4, 2 } };

        rig.Open (1);
        AssertSucceeded (rig.actions->GetDriveImages().ReadDiskToImage (L"C:\\Images\\out.dsk", false,
                                                                        [&offered] (const LinkOperation &, const std::string &, int) { offered = true; }).hr);
        rig.actions.reset();

        Assert::IsTrue  (rig.Finish().empty());
        Assert::IsFalse (offered, L"the completion of a closed window is not run");
    }



    TEST_METHOD (WriteImageToDisk_ReplacesTheDisk_AndListsItAgain)
    {
        Rig  rig;

        rig.io.files["C:\\Images\\in.dsk"] = Rig::BuildProDos();

        rig.Open (1);
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (DOS 3.3 volume 254)"), rig.actions->DescribeAppleDrive());

        AssertSucceeded (rig.actions->GetDriveImages().WriteImageToDisk (L"C:\\Images\\in.dsk").hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsTrue (rig.Drive (1) == rig.io.files["C:\\Images\\in.dsk"]);
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/LINKED)"), rig.actions->DescribeAppleDrive());
    }



    TEST_METHOD (FormatAppleDrive_WritesANewEmptyDisk_InEitherFormat)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  prodos;
        DiskOperations::NewDiskRequest  dos;

        prodos.formatName = "prodos";
        prodos.volumeName = "BLANK";
        dos.formatName    = "dos33";
        dos.volumeName    = "100";

        rig.Open (1);
        AssertSucceeded (rig.actions->GetDriveImages().FormatAppleDrive (prodos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::IsTrue   (rig.browser->GetRows().empty(), L"no files on the new disk");
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/BLANK)"), rig.actions->DescribeAppleDrive());

        rig.Open (2);
        AssertSucceeded (rig.actions->GetDriveImages().FormatAppleDrive (dos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::IsTrue   (rig.browser->GetRows().empty());
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 2 (DOS 3.3 volume 100)"), rig.actions->DescribeAppleDrive());
    }



    TEST_METHOD (FormatAppleDrive_FormatsADiskWhoseCatalogCannotBeRead)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  prodos;
        auto                            until     = std::chrono::steady_clock::now() + std::chrono::seconds (20);
        HRESULT                         refreshed = S_OK;

        prodos.formatName = "prodos";
        prodos.volumeName = "FRESH";

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            std::fill (rig.apple.drives[0].begin(), rig.apple.drives[0].end(), (Byte) 0);
        }

        rig.browser->GetNavigator().NavigateToLocation (Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (L"Test", 6, 1), std::string()));

        do
        {
            refreshed = rig.browser->Refresh();
            IGNORE_RETURN_VALUE (refreshed, S_OK);
            std::this_thread::yield();
        }
        while (rig.browser->GetListError() == CassoExplorerBrowser::s_kWorkingText && std::chrono::steady_clock::now() < until);

        Assert::IsFalse (rig.browser->IsAppleDriveLocation(), L"no catalog");
        Assert::IsTrue  (rig.browser->IsAppleDriveOpen(), L"but the drive is open, for its whole-disk commands");

        AssertSucceeded (rig.actions->GetDriveImages().FormatAppleDrive (prodos, L"C:\\Temp").hr);
        Assert::IsTrue   (rig.Finish().empty());
        Assert::AreEqual (std::wstring (L"Slot 6, Drive 1 (/FRESH)"), rig.actions->DescribeAppleDrive());
    }



    TEST_METHOD (FormatAppleDrive_RefusesAnUnformattedDisk)
    {
        Rig                             rig;
        DiskOperations::NewDiskRequest  none;
        std::vector<Byte>               before;
        HRESULT                         result = S_OK;

        none.formatName = "none";

        rig.Open (1);
        before = rig.Drive (1);

        result = rig.actions->GetDriveImages().FormatAppleDrive (none, L"C:\\Temp").hr;
        Assert::IsTrue (FAILED (result));
        Assert::IsTrue (rig.Drive (1) == before, L"the disk is untouched");
    }



    TEST_METHOD (CreateImageFromAppleDrive_NamesItForTheVolume_AndMakesTheNameFree)
    {
        Rig  rig;

        rig.Open (1);
        rig.io.files["C:\\Out\\DOS 3.3 volume 254.dsk"] = std::vector<Byte> (10, 0);

        AssertSucceeded (rig.actions->GetDriveImages().CreateImageFromAppleDrive ("Test", 1, L"C:\\Out").hr);
        Assert::IsTrue (rig.Finish().empty());

        Assert::IsTrue (rig.io.files["C:\\Out\\DOS 3.3 volume 254 (2).dsk"] == rig.Drive (1), L"a name no file in the folder has");
    }



    TEST_METHOD (MakeDriveImageName_UsesTheVolume)
    {
        VolumeListing  listing;

        Assert::AreEqual (std::wstring (L"Slot 6 Drive 2.dsk"), ActionsDriveImage::MakeDriveImageName (listing, VolumeKind::Unknown, 6, 2));

        listing.hasVolumeNumber = true;
        listing.volumeNumber    = 254;
        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 254.dsk"), ActionsDriveImage::MakeDriveImageName (listing, VolumeKind::Dos33, 6, 1));

        listing.hasVolumeName = true;
        listing.volumeName    = "GAMES";
        Assert::AreEqual (std::wstring (L"GAMES.po"), ActionsDriveImage::MakeDriveImageName (listing, VolumeKind::ProDos, 6, 1));
    }



    //  GetDriveImageName returns the volume's name once the drive's catalog is
    //  read, and the drive's number before then.
    TEST_METHOD (GetDriveImageName_UsesTheCatalogRead)
    {
        Rig  rig;



        Assert::AreEqual (std::wstring (L"Slot 6 Drive 2.dsk"), ActionsDriveImage::GetDriveImageName (nullptr,          "Test", 6, 2));
        Assert::AreEqual (std::wstring (L"Slot 6 Drive 2.dsk"), ActionsDriveImage::GetDriveImageName (rig.links.get(), "Test", 6, 2), L"no catalog read yet");

        rig.Open (2);
        Assert::AreEqual (std::wstring (L"LINKED.po"), ActionsDriveImage::GetDriveImageName (rig.links.get(), "Test", 6, 2));

        rig.Open (1);
        Assert::AreEqual (std::wstring (L"DOS 3.3 volume 254.dsk"), ActionsDriveImage::GetDriveImageName (rig.links.get(), "Test", 6, 1));
    }
};
