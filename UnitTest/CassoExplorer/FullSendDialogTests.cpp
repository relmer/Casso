#include "Pch.h"

#include "CassoExplorer/FullSendDialog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDialogTests
//
//  The Full Send window's text: the state line, a drive's lines, the totals
//  and a disk's status.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendDialogTests)
{
public:

    TEST_METHOD (TheStateLine_IsASentenceForEachActivity)
    {
        FullSendSnapshot  snapshot;



        Assert::AreEqual (std::wstring (L"No Full Send is in progress. Press F on the Apple's CassoLink screen to start one."), FullSendPanel::DescribeActivity (snapshot));

        snapshot.activity = FullSendActivity::Starting;
        Assert::AreEqual (std::wstring (L"The Apple is waiting for disks to send."), FullSendPanel::DescribeActivity (snapshot), L"No model");
        Assert::AreEqual (std::wstring (L"The Apple //c is waiting for disks to send."), FullSendPanel::DescribeActivity (snapshot, L"Apple //c (ROM 3)"), L"The model without its ROM");

        snapshot.activity = FullSendActivity::WaitingForDisk;
        Assert::AreEqual (std::wstring (L"The Apple //e is waiting for more disks to send."), FullSendPanel::DescribeActivity (snapshot, L"Apple //e"));

        snapshot.activity = FullSendActivity::Sending;
        snapshot.drive    = 2;
        Assert::AreEqual (std::wstring (L"Sending the disk in drive 2."), FullSendPanel::DescribeActivity (snapshot));

        snapshot.activity = FullSendActivity::Capturing;
        snapshot.drive    = 1;
        Assert::AreEqual (std::wstring (L"The disk in drive 1 is likely copy-protected; using raw capture mode."), FullSendPanel::DescribeActivity (snapshot));

        snapshot.activity = FullSendActivity::WaitingForApple;
        snapshot.waited   = 7.2;
        Assert::AreEqual (std::wstring (L"Waiting for the Apple to reply, 0:07."), FullSendPanel::DescribeActivity (snapshot));

        snapshot.activity = FullSendActivity::Ended;
        snapshot.reason   = "Q was pressed on the Apple.";
        Assert::AreEqual (std::wstring (L"The session ended. Q was pressed on the Apple."), FullSendPanel::DescribeActivity (snapshot));
    }



    TEST_METHOD (ADrivesLines_HaveItsStateFileSystemAndTimes)
    {
        FullSendDriveStatus  drive;



        Assert::AreEqual (std::wstring (L"No disk read yet"), FullSendPanel::DescribeDrive (drive));
        Assert::AreEqual (std::wstring(), FullSendPanel::DescribeTimes (drive));
        Assert::AreEqual (std::wstring(), FullSendPanel::DescribeCapture (drive));

        drive.state      = FullSendDiskState::Sending;
        drive.verified   = true;
        drive.fileSystem = "DOS 3.3";
        drive.elapsed    = 21.0;
        Assert::AreEqual (std::wstring (L"Sending, with verification, DOS 3.3"), FullSendPanel::DescribeDrive (drive));
        Assert::AreEqual (std::wstring (L"0:21 elapsed"), FullSendPanel::DescribeTimes (drive), L"no time left before there is a rate");

        drive.secondsLeft = 74.0;
        Assert::AreEqual (std::wstring (L"0:21 elapsed, about 1:14 left"), FullSendPanel::DescribeTimes (drive));

        drive.state   = FullSendDiskState::Sent;
        drive.elapsed = 3725.0;
        Assert::AreEqual (std::wstring (L"Took 1:02:05"), FullSendPanel::DescribeTimes (drive));
    }



    TEST_METHOD (ARawCapturesLine_HasThePassTrackAndRead_ThenItsTwoTimes)
    {
        FullSendDriveStatus  drive;



        drive.state = FullSendDiskState::Capturing;
        drive.positions.assign (FullSendDriveStatus::kPositions, FullSendPositionMark::Unread);
        Assert::AreEqual (std::wstring (L"Raw capture: starting"), FullSendPanel::DescribeCapture (drive));

        drive.pass     = 2;
        drive.position = 49;
        drive.read     = 1;
        drive.reads    = 3;
        Assert::AreEqual (std::wstring (L"Raw capture: between tracks, track 12.25, read 1 of 3"), FullSendPanel::DescribeCapture (drive));

        drive.state       = FullSendDiskState::Sent;
        drive.diskSeconds = 143.0;
        drive.lineSeconds = 303.0;
        Assert::AreEqual (std::wstring (L"Raw capture: 2:23 on the disk, 5:03 on the line"), FullSendPanel::DescribeCapture (drive));
    }



    TEST_METHOD (TheStatusBarsLine_HasTheDriveDiskPercentTimeLeftAndDisksDone)
    {
        FullSendSnapshot  snapshot;



        Assert::AreEqual (std::wstring(), FullSendPanel::DescribeSummary (snapshot), L"No session");

        snapshot.activity          = FullSendActivity::Sending;
        snapshot.drive             = 1;
        snapshot.drives[0].title   = "GAMES DISK";
        snapshot.drives[0].percent = 42;
        Assert::AreEqual (std::wstring (L"Full Send: drive 1, GAMES DISK, 42%, 0 disks done"), FullSendPanel::DescribeSummary (snapshot), L"No time left before there is a rate");

        snapshot.activity              = FullSendActivity::Capturing;
        snapshot.drives[0].title       = "0023";
        snapshot.drives[0].secondsLeft = 190.0;
        snapshot.totals.sent           = 1;
        Assert::AreEqual (std::wstring (L"Full Send: drive 1, 0023, raw capture, 42%, about 3:10 left, 1 disk done"), FullSendPanel::DescribeSummary (snapshot));

        snapshot.activity          = FullSendActivity::WaitingForDisk;
        snapshot.totals.duplicates = 1;
        Assert::AreEqual (std::wstring (L"Full Send: waiting for a disk, 2 disks done"), FullSendPanel::DescribeSummary (snapshot));
    }



    TEST_METHOD (AMapCellsTip_HasItsTrackAndWhatItsMarkMeans)
    {
        FullSendDriveStatus  drive;



        Assert::AreEqual (std::wstring (L"Track 12: not read yet"), FullSendPanel::DescribeCell (drive, 48));

        drive.tracks[12] = FullSendTrackMark::Verified;
        Assert::AreEqual (std::wstring (L"Track 12: read and verified"), FullSendPanel::DescribeCell (drive, 48));

        drive.tracks[12] = FullSendTrackMark::Unreadable;
        Assert::AreEqual (std::wstring (L"Track 12: a sector could not be read"), FullSendPanel::DescribeCell (drive, 48));

        //  In a raw capture a cell has its position's mark, not the mark of
        //  the sector read before it.
        drive.positions.assign (FullSendDriveStatus::kPositions, FullSendPositionMark::Unread);
        drive.positions[48] = FullSendPositionMark::Read;
        drive.positions[49] = FullSendPositionMark::Unstable;
        Assert::AreEqual (std::wstring (L"Track 12: read"), FullSendPanel::DescribeCell (drive, 48));
        Assert::AreEqual (std::wstring (L"Track 12.25: the reads differ in length"), FullSendPanel::DescribeCell (drive, 49));
    }



    TEST_METHOD (TheTotals_HaveTheAverageAndSizeOnceADiskIsSent)
    {
        FullSendTotals  totals;



        totals.elapsed = 65.0;
        Assert::AreEqual (std::wstring (L"0 sent, 0 duplicates, 0 blank, 0 raw captures, 0 with bad sectors    1:05 elapsed"), FullSendPanel::DescribeTotals (totals));

        totals.sent        = 2;
        totals.duplicates  = 1;
        totals.rawCaptures = 1;
        totals.average     = 26.0;
        totals.bytes       = 2 * 143360;
        Assert::AreEqual (std::wstring (L"2 sent, 1 duplicate, 0 blank, 1 raw capture, 0 with bad sectors    1:05 elapsed, 0:26 a disk, 0.3 MB"), FullSendPanel::DescribeTotals (totals));
    }



    TEST_METHOD (ADisksStatus_IsItsManifestRowInWords)
    {
        FullSendRow  row;



        row.status = FullSendManifest::kStatusSent;
        Assert::AreEqual (std::wstring (L"Sent"), FullSendPanel::DescribeStatus (row));

        row.verification = FullSendManifest::kVerifyFull;
        Assert::AreEqual (std::wstring (L"Sent, verified"), FullSendPanel::DescribeStatus (row));

        row.unreliable = 1;
        Assert::AreEqual (std::wstring (L"1 unreliable sector"), FullSendPanel::DescribeStatus (row));

        row.unreadable = 3;
        Assert::AreEqual (std::wstring (L"3 unreadable sectors"), FullSendPanel::DescribeStatus (row));

        row.status = FullSendManifest::kStatusRawCapture;
        Assert::AreEqual (std::wstring (L"Raw capture"), FullSendPanel::DescribeStatus (row));

        row.boot = FullSendManifest::kBootBooted;
        Assert::AreEqual (std::wstring (L"Raw capture, boots"), FullSendPanel::DescribeStatus (row));

        row.boot = FullSendManifest::kBootDidNotBoot;
        Assert::AreEqual (std::wstring (L"Raw capture, did not boot"), FullSendPanel::DescribeStatus (row));

        row.status      = FullSendManifest::kStatusDuplicate;
        row.duplicateOf = "0001 HELLO.dsk";
        Assert::AreEqual (std::wstring (L"Duplicate of 0001 HELLO.dsk"), FullSendPanel::DescribeStatus (row));
    }



    TEST_METHOD (EndSession_AppliesOnlyWhileASessionRuns)
    {
        FullSendSnapshot  snapshot;



        Assert::IsFalse (FullSendDialog::IsRunning (snapshot));

        snapshot.activity = FullSendActivity::WaitingForDisk;
        Assert::IsTrue (FullSendDialog::IsRunning (snapshot));

        snapshot.activity = FullSendActivity::Ended;
        Assert::IsFalse (FullSendDialog::IsRunning (snapshot));
    }
};
