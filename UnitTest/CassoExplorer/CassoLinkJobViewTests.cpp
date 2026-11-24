#include "Pch.h"

#include "CassoExplorer/CassoLinkJobDialog.h"
#include "CassoLink/Session/CassoLinkDiskStatus.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobViewTests
//
//  A disk job's tab in the job window: its status line, its Apple and
//  connection, its drive as the job's Full Send status has it, and what a
//  finished capture found.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkJobViewTests)
{
public:

    TEST_METHOD (ARunningCapture_HasItsDriveAndConnection)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind      = LinkOperationKind::CaptureDisk;
        operation.drive     = 2;
        operation.imagePath = "C:\\Disks\\Adventure.woz";
        operation.state     = LinkOperationState::Running;

        view = CassoLinkJobView::Build (operation, "Serial (any port)", L"Apple //c", LinkTransport::Serial, L"COM3");

        Assert::AreEqual (std::wstring (L"Capturing Slot 6, Drive 2"),        view.heading);
        Assert::AreEqual (std::wstring (L"Apple //c (COM3)"),                 view.tabLabel);
        Assert::AreEqual (std::wstring (L"Apple //c over serial on COM3"),    view.connection);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Adventure.woz"),         view.imagePath);
        Assert::IsTrue   (view.drive.isJob);
        Assert::IsTrue   (view.summary.empty(), L"Nothing found until it ends");
    }




    TEST_METHOD (ATracksMark_ArrivesAsTheJobGoes)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind   = LinkOperationKind::ReadDisk;
        operation.drive  = 2;
        operation.state  = LinkOperationState::Running;
        operation.status = std::make_shared<FullSendStatus> ([] () { return 0.0; }, [] () { return std::string ("0:00"); });

        operation.status->BeginDisk (2, false, std::span<const int>());
        operation.status->MarkTrack (2, 1, FullSendTrackMark::Unreadable);

        view = CassoLinkJobView::Build (operation, "COM3", L"", LinkTransport::Serial, L"");

        Assert::IsTrue (view.drive.snapshot.drives[1].tracks[1] == FullSendTrackMark::Unreadable, L"Marked before the read ends");
        Assert::IsTrue (view.drive.snapshot.drives[1].tracks[2] == FullSendTrackMark::Unread);
    }




    TEST_METHOD (AWrite_HasOnlyItsDrive_WrittenTrackByTrack)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind   = LinkOperationKind::WriteDisk;
        operation.drive  = 1;
        operation.state  = LinkOperationState::Running;
        operation.status = std::make_shared<FullSendStatus> ([] () { return 0.0; }, [] () { return std::string ("0:00"); });

        operation.status->BeginDisk  (1, false, std::span<const int>());
        operation.status->SetWriting (1);
        operation.status->SetTitle   (1, "PRODOS.2.4.3", std::string());
        operation.status->MarkTrack  (1, 0, FullSendTrackMark::Read);

        view = CassoLinkJobView::Build (operation, "COM3", L"", LinkTransport::Serial, L"");

        Assert::IsTrue   (view.drive.snapshot.present[0]);
        Assert::IsFalse  (view.drive.snapshot.present[1], L"drive 2 isn't the job's");
        Assert::IsTrue   (view.drive.snapshot.drives[0].tracks[0] == FullSendTrackMark::Read);
        Assert::AreEqual (std::wstring (L"Writing"), FullSendPanel::DescribeDrive (view.drive.snapshot.drives[0]));
        Assert::AreEqual (std::string ("PRODOS.2.4.3"), view.drive.snapshot.drives[0].title);
    }



    TEST_METHOD (AFinishedCapture_ListsWhatItFound)
    {
        LinkOperation     operation;
        CassoLinkJobView  view;



        operation.kind    = LinkOperationKind::CaptureDisk;
        operation.state   = LinkOperationState::Done;
        operation.summary = { { "Reads", "105" } };

        view = CassoLinkJobView::Build (operation, "COM3", L"", LinkTransport::Serial, L"");

        Assert::AreEqual (std::wstring (L"Captured Slot 6, Drive 1"), view.heading);
        Assert::AreEqual (std::wstring (L"105"), view.summary[0].second);
    }




    TEST_METHOD (ATabLabel_IsTheAppleAndHowItsConnected)
    {
        Assert::AreEqual (std::wstring (L"Apple //e (Ethernet)"),   CassoLinkJobView::DescribeTab (L"Apple //e", "Lab", LinkTransport::Uthernet, L""));
        Assert::AreEqual (std::wstring (L"Lab (serial)"),           CassoLinkJobView::DescribeTab (L"", "Lab", LinkTransport::Serial, L""), L"No model or port known");
        Assert::AreEqual (std::wstring (L"Apple //e over Ethernet on an Uthernet II"), CassoLinkJobView::DescribeConnection (L"Apple //e", LinkTransport::Uthernet, L""));
    }




    TEST_METHOD (OnlyWholeDiskJobs_HaveAJobWindow)
    {
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::ReadDisk));
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::CaptureDisk));
        Assert::IsTrue  (CassoLinkJobView::IsDiskJob (LinkOperationKind::WriteDisk));
        Assert::IsFalse (CassoLinkJobView::IsDiskJob (LinkOperationKind::ReadFile));
    }




    TEST_METHOD (ATracksMark_IsItsWorstRead)
    {
        Assert::IsTrue (CassoLinkDiskStatus::GetTrackMark (1, 1, true)  == FullSendTrackMark::Unreadable);
        Assert::IsTrue (CassoLinkDiskStatus::GetTrackMark (0, 1, true)  == FullSendTrackMark::Unreliable);
        Assert::IsTrue (CassoLinkDiskStatus::GetTrackMark (0, 0, true)  == FullSendTrackMark::Verified);
        Assert::IsTrue (CassoLinkDiskStatus::GetTrackMark (0, 0, false) == FullSendTrackMark::Read);
    }




    TEST_METHOD (ACaptureSummary_HasARowForEachFinding)
    {
        CassoLinkCapture::Result                          result;
        std::vector<std::pair<std::string, std::string>>  rows;



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
