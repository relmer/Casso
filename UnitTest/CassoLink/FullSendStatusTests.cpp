#include "Pch.h"

#include "CassoLink/FullSend/FullSendStatus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendStatusTests
//
//  The Full Send window's model through a scripted session, with a clock
//  the test sets.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendStatusTests)
{
public:

    static constexpr double  kTrackSeconds = 0.75;

    struct Rig
    {
        double          now = 0.0;
        FullSendStatus  status;

        Rig() :
            status ([this] () { return now; }, [this] () { return std::format ("{:.0f}", now); })
        {
        }

        //  A disk's tracks read at kTrackSeconds each, from `first`.
        void  ReadTracks (int drive, int first, int count, FullSendTrackMark mark = FullSendTrackMark::Read)
        {
            for (int track = first; track < first + count; track++)
            {
                now += kTrackSeconds;
                status.MarkTrack (drive, track, mark);
            }
        }

        //  A whole disk sent as a sector image.
        void  SendDisk (int drive, int sequence, bool verified)
        {
            FullSendRow  row;

            row.sequence = sequence;
            row.file     = std::format ("{:04}.dsk", sequence);
            row.status   = FullSendManifest::kStatusSent;

            status.BeginDisk (drive, verified, {});
            ReadTracks (drive, 0, FullSendDriveStatus::kTracks, verified ? FullSendTrackMark::Verified : FullSendTrackMark::Read);
            status.EndDisk (drive, row, 143360);
        }
    };



    TEST_METHOD (ADiskBeingSent_HasItsTitleTracksAndPercent)
    {
        Rig               rig;
        const int         have[] = { 0, 17 };
        FullSendSnapshot  snapshot;



        rig.status.Begin();
        rig.status.SetActivity (FullSendActivity::Sending, 1);
        rig.status.BeginDisk (1, false, have);
        rig.status.SetTitle (1, "GAMES DISK", "DOS 3.3");
        rig.ReadTracks (1, 1, 5);

        snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.activity == FullSendActivity::Sending);
        Assert::AreEqual (1, snapshot.drive);
        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Sending);
        Assert::AreEqual (std::string ("GAMES DISK"), snapshot.drives[0].title);
        Assert::AreEqual (std::string ("DOS 3.3"), snapshot.drives[0].fileSystem);
        Assert::IsTrue (snapshot.drives[0].tracks[17] == FullSendTrackMark::Read, L"a track the check read");
        Assert::IsTrue (snapshot.drives[0].tracks[5] == FullSendTrackMark::Read);
        Assert::IsTrue (snapshot.drives[0].tracks[6] == FullSendTrackMark::Unread);
        Assert::AreEqual (7 * 100 / 35, snapshot.drives[0].percent);
        Assert::IsTrue (snapshot.drives[1].state == FullSendDiskState::None);
    }



    //  The first disk has no time left until two of its tracks are read, and
    //  then the tracks left at their rate. The tracks a check read don't
    //  count toward the rate.
    TEST_METHOD (TheFirstDisksTimeLeft_IsBlankUntilTwoTracks_ThenFromTheirRate)
    {
        Rig               rig;
        const int         have[] = { 0, 17 };
        FullSendSnapshot  snapshot;



        rig.status.Begin();
        rig.status.BeginDisk (1, false, have);
        Assert::IsTrue (rig.status.GetSnapshot().drives[0].secondsLeft < 0.0, L"nothing read");

        rig.ReadTracks (1, 1, 1);
        Assert::IsTrue (rig.status.GetSnapshot().drives[0].secondsLeft < 0.0, L"one track read");

        rig.ReadTracks (1, 2, 1);
        snapshot = rig.status.GetSnapshot();

        Assert::AreEqual (kTrackSeconds * 31, snapshot.drives[0].secondsLeft, 0.001);
        Assert::AreEqual (kTrackSeconds * 2, snapshot.drives[0].elapsed, 0.001);

        rig.now += 0.5;
        Assert::AreEqual (kTrackSeconds * 31 - 0.5, rig.status.GetSnapshot().drives[0].secondsLeft, 0.001, L"less the time since the latest track");
    }



    //  Before two tracks, a later disk's time left is the average of the
    //  disks read the same way, less the time so far.
    TEST_METHOD (ALaterDisksTimeLeft_BeginsFromTheAverageOfDisksReadTheSameWay)
    {
        Rig     rig;
        double  disk = kTrackSeconds * FullSendDriveStatus::kTracks;



        rig.status.Begin();
        rig.SendDisk (1, 1, false);

        rig.status.BeginDisk (2, false, {});
        rig.now += 1.0;
        Assert::AreEqual (disk - 1.0, rig.status.GetSnapshot().drives[1].secondsLeft, 0.001);

        rig.status.BeginDisk (2, true, {});
        Assert::IsTrue (rig.status.GetSnapshot().drives[1].secondsLeft < 0.0, L"no verified disk was sent");
    }



    TEST_METHOD (ARawCapture_HasItsPassReadsAndPositions_AndTheCapturesTimeLeft)
    {
        Rig               rig;
        FullSendSnapshot  snapshot;



        rig.status.Begin();
        rig.status.BeginDisk (1, false, {});
        rig.status.SetTitle (1, "ARCHON", "");
        rig.ReadTracks (1, 0, 35, FullSendTrackMark::Unreadable);

        rig.status.SetActivity (FullSendActivity::Capturing, 1);
        rig.status.BeginCapture (1);
        rig.status.SetCapture (1, 1, 8, 2, 3, 6, 400.0);
        rig.status.MarkPosition (1, 4, FullSendPositionMark::Weak);
        rig.status.MarkPosition (1, 5, FullSendPositionMark::Skipped);
        rig.now += 10.0;

        snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Capturing);
        Assert::AreEqual (std::string ("ARCHON"), snapshot.drives[0].title);
        Assert::AreEqual ((size_t) FullSendDriveStatus::kPositions, snapshot.drives[0].positions.size());
        Assert::IsTrue (snapshot.drives[0].positions[8] == FullSendPositionMark::Read);
        Assert::IsTrue (snapshot.drives[0].positions[4] == FullSendPositionMark::Weak);
        Assert::IsTrue (snapshot.drives[0].positions[5] == FullSendPositionMark::Skipped);
        Assert::IsTrue (snapshot.drives[0].positions[9] == FullSendPositionMark::Unread);
        Assert::AreEqual (1, snapshot.drives[0].pass);
        Assert::AreEqual (2, snapshot.drives[0].read);
        Assert::AreEqual (3, snapshot.drives[0].reads);
        Assert::AreEqual (6, snapshot.drives[0].percent);
        Assert::AreEqual (390.0, snapshot.drives[0].secondsLeft, 0.001);
    }



    //  A disk, its duplicate and a raw capture: the totals, the disks newest
    //  first, and the times each took.
    TEST_METHOD (DisksDone_AreListedNewestFirst_AndCountedInTheTotals)
    {
        Rig               rig;
        FullSendRow       duplicate;
        FullSendRow       raw;
        FullSendSnapshot  snapshot;
        double            disk = kTrackSeconds * FullSendDriveStatus::kTracks;



        duplicate.sequence    = 2;
        duplicate.status      = FullSendManifest::kStatusDuplicate;
        duplicate.duplicateOf = "0001.dsk";
        raw.sequence          = 3;
        raw.file              = "0003.woz";
        raw.status            = FullSendManifest::kStatusRawCapture;

        rig.status.Begin();
        rig.SendDisk (1, 1, false);

        rig.status.BeginDisk (2, false, {});
        rig.now += 4.0;
        rig.status.EndDisk (2, duplicate, 0);

        rig.status.BeginDisk (1, false, {});
        rig.status.BeginCapture (1);
        rig.now += 500.0;
        rig.status.EndDisk (1, raw, 230000);

        snapshot = rig.status.GetSnapshot();

        Assert::AreEqual ((size_t) 3, snapshot.disks.size());
        Assert::AreEqual (3, snapshot.disks[0].row.sequence);
        Assert::AreEqual (2, snapshot.disks[1].row.sequence);
        Assert::AreEqual (1, snapshot.disks[2].row.sequence);
        Assert::AreEqual (500.0, snapshot.disks[0].seconds, 0.001);
        Assert::AreEqual (disk, snapshot.disks[2].seconds, 0.001);

        Assert::AreEqual (2, snapshot.totals.sent);
        Assert::AreEqual (1, snapshot.totals.duplicates);
        Assert::AreEqual (1, snapshot.totals.rawCaptures);
        Assert::AreEqual (0, snapshot.totals.badSectors);
        Assert::AreEqual ((uint64_t) (143360 + 230000), snapshot.totals.bytes);
        Assert::AreEqual ((disk + 500.0) / 2, snapshot.totals.average, 0.001);
        Assert::AreEqual (disk + 504.0, snapshot.totals.elapsed, 0.001);

        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Sent);
        Assert::IsTrue (snapshot.drives[1].state == FullSendDiskState::Duplicate);

        //  A raw capture's time is not a sample for a sector image's.
        rig.status.BeginDisk (2, false, {});
        Assert::AreEqual (disk, rig.status.GetSnapshot().drives[1].secondsLeft, 0.001);
    }



    TEST_METHOD (ADiskWithAnUnreadableSector_IsCountedInTheTotals)
    {
        Rig          rig;
        FullSendRow  row;



        row.sequence   = 1;
        row.status     = FullSendManifest::kStatusDamaged;
        row.unreadable = 2;

        rig.status.Begin();
        rig.status.BeginDisk (1, false, {});
        rig.status.EndDisk (1, row, 143360);

        Assert::AreEqual (1, rig.status.GetSnapshot().totals.badSectors);
    }



    //  A wait for the Apple is the activity until the reply, with its length;
    //  then the activity before it again, and a line.
    TEST_METHOD (AWaitForTheApple_IsTheActivityUntilTheReply_AndAddsALine)
    {
        Rig               rig;
        FullSendSnapshot  snapshot;



        rig.status.Begin();
        rig.status.SetActivity (FullSendActivity::Capturing, 2);
        rig.status.SetWaiting (true);
        rig.now += 7.0;

        snapshot = rig.status.GetSnapshot();
        Assert::IsTrue (snapshot.activity == FullSendActivity::WaitingForApple);
        Assert::AreEqual (7.0, snapshot.waited, 0.001);
        Assert::IsTrue (snapshot.lines.empty());

        rig.status.SetWaiting (false);

        snapshot = rig.status.GetSnapshot();
        Assert::IsTrue (snapshot.activity == FullSendActivity::Capturing);
        Assert::AreEqual (2, snapshot.drive);
        Assert::AreEqual ((size_t) 1, snapshot.lines.size());
        Assert::AreEqual (std::string ("7"), snapshot.lines[0].time);
        Assert::AreEqual (std::string ("The Apple replied after 7 seconds."), snapshot.lines[0].text);
    }



    //  After the end the disks, totals and lines are as they were, the time
    //  elapsed stops, and a disk in progress is cleared.
    TEST_METHOD (ASessionsEnd_StopsItsTime_AndItsDisksAndLinesAreReadable)
    {
        Rig               rig;
        FullSendSnapshot  snapshot;



        rig.status.Begin();
        rig.SendDisk (1, 1, false);
        rig.status.AddLine ("0001.dsk sent.");
        rig.status.BeginDisk (2, false, {});
        rig.status.End ("Q was pressed on the Apple.");
        rig.now += 100.0;

        snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.activity == FullSendActivity::Ended);
        Assert::AreEqual (std::string ("Q was pressed on the Apple."), snapshot.reason);
        Assert::AreEqual ((size_t) 1, snapshot.disks.size());
        Assert::AreEqual ((size_t) 2, snapshot.lines.size());
        Assert::AreEqual (kTrackSeconds * FullSendDriveStatus::kTracks, snapshot.totals.elapsed, 0.001);
        Assert::IsTrue (snapshot.drives[0].state == FullSendDiskState::Sent);
        Assert::IsTrue (snapshot.drives[1].state == FullSendDiskState::None);

        rig.status.Begin();
        snapshot = rig.status.GetSnapshot();

        Assert::IsTrue (snapshot.disks.empty(), L"a new session begins empty");
        Assert::IsTrue (snapshot.lines.empty());
        Assert::AreEqual (0, snapshot.totals.sent);
    }
};
