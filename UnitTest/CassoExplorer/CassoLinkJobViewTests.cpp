#include "Pch.h"

#include "CassoExplorer/CassoLinkJobDialog.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobViewTests
//
//  A disk job's tab in the job window: its heading, its track map and times,
//  and what a finished capture found.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkJobViewTests)
{
public:

    TEST_METHOD (ARunningCapture_HasItsDriveAndFile_AndTracksReadSoFar)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind      = LinkOperationKind::CaptureDisk;
        operation.drive     = 2;
        operation.imagePath = "C:\\Disks\\Adventure.woz";
        operation.state     = LinkOperationState::Running;
        operation.progress  = 40;

        view = CassoLinkJobView::Build (operation, "COM3", L"Apple ][ plus", 20.0);

        Assert::AreEqual (std::wstring (L"Capturing Slot 6, Drive 2"), view.heading);
        Assert::AreEqual (std::wstring (L"Apple ][ plus, COM3"), view.tabLabel);
        Assert::IsTrue  (view.tracks[13] == FullSendTrackMark::Read, L"40% of 35 tracks is 14");
        Assert::IsTrue  (view.tracks[14] == FullSendTrackMark::Unread);
        Assert::AreEqual (30.0, view.left, 0.001, L"20 s for 40%, so 30 s for the rest");
        Assert::IsTrue  (view.summary.empty(), L"Nothing found until it ends");
    }




    TEST_METHOD (AFinishedCapture_ListsWhatItFound_AndMarksWeakTracks)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind          = LinkOperationKind::CaptureDisk;
        operation.imagePath     = "Adventure.woz";
        operation.state         = LinkOperationState::Done;
        operation.weakPositions = { 108 };
        operation.summary       = { { "Reads", "105" } };

        view = CassoLinkJobView::Build (operation, "COM3", L"", 254.0);

        Assert::AreEqual (std::wstring (L"Captured Slot 6, Drive 1"), view.heading);
        Assert::AreEqual (std::wstring (L"COM3"), view.tabLabel, L"No model known");
        Assert::AreEqual ((size_t) 1, view.weakTracks.size());
        Assert::AreEqual (27, view.weakTracks[0], L"Position 108 is track 27");
        Assert::AreEqual (std::wstring (L"105"), view.summary[0].second);
        Assert::IsTrue  (view.tracks[34] == FullSendTrackMark::Read, L"Every track read");
    }




    TEST_METHOD (AFinishedRead_MarksItsUnreadableTracks)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind       = LinkOperationKind::ReadDisk;
        operation.imagePath  = "Disk.dsk";
        operation.state      = LinkOperationState::Done;
        operation.unreadable = { { 17, 3 } };

        view = CassoLinkJobView::Build (operation, "COM3", L"", 30.0);

        Assert::IsTrue   (view.tracks[17] == FullSendTrackMark::Unreadable);
        Assert::AreEqual (std::wstring (L"1, written as zeros"), view.summary[0].second);
    }




    TEST_METHOD (OnlyWholeDiskJobs_HaveAJobWindow)
    {
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::ReadDisk));
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::CaptureDisk));
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::WriteDisk));
        Assert::IsFalse (CassoLinkJobView::IsDiskJob (LinkOperationKind::ReadFile));
    }




    TEST_METHOD (ACaptureSummary_HasARowForEachFinding)
    {
        CassoLinkCapture::Result  result;
        auto                      rows = std::vector<std::pair<std::string, std::string>>();



        result.weak         = { 108 };
        result.reads        = 105;
        result.diskMs       = 74000.0;
        result.lineMs       = 180000.0;
        result.sentBytes    = 791 * 1024;
        result.storedBytes  = 1443 * 1024;
        result.betweenTotal = 102;

        rows = CassoLinkDiskTransfer::MakeCaptureSummary (result);

        Assert::AreEqual (std::string ("27"),          rows[0].second, L"Weak tracks");
        Assert::AreEqual (std::string ("None"),        rows[1].second, L"No unstable tracks");
        Assert::AreEqual (std::string ("105"),         rows[2].second);
        Assert::AreEqual (std::string ("1 min 14 s"),  rows[3].second);
        Assert::AreEqual (std::string ("3 min 0 s, 1443 KB compressed to 791 KB, at 4.4 KB/s"), rows[4].second);
        Assert::AreEqual (std::string ("0 of 102"),    rows[5].second);
    }
};
