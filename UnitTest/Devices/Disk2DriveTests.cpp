#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Disk2Clock.h"
#include "Machines/Apple2/Common/Disk2Drive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DriveTests
//
//  One Disk II drive, apart from a controller: where the energized phases
//  pull its head, a bump at a travel stop, the arrivals it counts, and a disk
//  change starting the counts over. Then the controller's clock: the cycles
//  since the last access, and a count that moved back after a power cycle.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2DriveTests)
{
public:

    static constexpr uint8_t  kPhase0 = 0b0001;
    static constexpr uint8_t  kPhase1 = 0b0010;
    static constexpr uint8_t  kPhase3 = 0b1000;


    TEST_METHOD (ThePullIsTheAverageOfTheEnergizedPhases)
    {
        Assert::AreEqual ( 0, Disk2Drive::GetPull (0b0000, 5),  L"no phase on");
        Assert::AreEqual ( 2, Disk2Drive::GetPull (0b0010, 0),  L"phase 1 from track 0");
        Assert::AreEqual ( 1, Disk2Drive::GetPull (0b0011, 0),  L"phases 0 and 1: the quarter track between");
        Assert::AreEqual ( 1, Disk2Drive::GetPull (0b0010, 1),  L"phase 1 from the quarter track before it");
        Assert::AreEqual (-2, Disk2Drive::GetPull (0b1000, 0),  L"phase 3 pulls back from track 0");
        Assert::AreEqual ( 0, Disk2Drive::GetPull (0b0100, 0),  L"phase 2 is opposite track 0");
        Assert::AreEqual ( 0, Disk2Drive::GetPull (0b1011, 0),  L"phases 3, 0 and 1 balance");
        Assert::AreEqual ( 2, Disk2Drive::GetPull (0b0010, 64), L"the cycle repeats every eight quarter tracks");
    }


    TEST_METHOD (AMoveRecordsWhereTheHeadWentAndCountsTheArrival)
    {
        auto           drive = std::make_unique<Disk2Drive>();
        Disk2HeadMove  move;



        move = drive->MoveHead (kPhase1);

        Assert::AreEqual (0, move.from);
        Assert::AreEqual (2, move.to);
        Assert::IsFalse  (move.isBumped);
        Assert::IsTrue   (move.IsMoved());
        Assert::AreEqual (2, drive->GetQuarterTrack());
        Assert::AreEqual (2, drive->GetEngine().GetCurrentTrack(), L"the engine is on the head's quarter track");
        Assert::AreEqual ((uint32_t) 1, drive->GetVisitCount (2));

        move = drive->MoveHead (kPhase1);

        Assert::IsFalse  (move.IsMoved(), L"the head is on the phase's magnet");
        Assert::AreEqual ((uint32_t) 1, drive->GetVisitCount (2), L"no arrival without a move");
    }


    TEST_METHOD (APullPastTrackZeroIsABumpThatLeavesTheHeadThere)
    {
        auto           drive = std::make_unique<Disk2Drive>();
        Disk2HeadMove  move;



        move = drive->MoveHead (kPhase3);

        Assert::IsTrue   (move.isBumped);
        Assert::IsTrue   (move.IsMoved(), L"a bump is a move for the sinks");
        Assert::AreEqual (0, move.to);
        Assert::AreEqual (0, drive->GetQuarterTrack());
    }


    TEST_METHOD (ADiskChangeStartsTheCountsOver)
    {
        auto       drive = std::make_unique<Disk2Drive>();
        auto       disk  = std::make_unique<DiskImage>();



        (void) drive->MoveHead (kPhase1);
        (void) drive->MoveHead (kPhase0);
        Assert::AreEqual ((uint32_t) 1, drive->GetVisitCount (0));

        drive->SetExternalDisk (disk.get());

        Assert::AreEqual ((uint32_t) 0, drive->GetVisitCount (0));
        Assert::AreEqual ((uint32_t) 0, drive->GetVisitCount (2));
        Assert::IsTrue   (drive->HasExternalDisk());
        Assert::IsTrue   (drive->GetDisk() == disk.get());

        drive->SetExternalDisk (nullptr);

        Assert::IsFalse  (drive->HasExternalDisk(), L"nullptr puts the drive's empty disk back");
    }


    TEST_METHOD (AVisitCountOutOfRangeIsZero)
    {
        auto  drive = std::make_unique<Disk2Drive>();



        Assert::AreEqual ((uint32_t) 0, drive->GetVisitCount (-1));
        Assert::AreEqual ((uint32_t) 0, drive->GetVisitCount (Disk2Drive::kMaxQuarterTrack + 1));
    }


    TEST_METHOD (TheClockReturnsTheCyclesSinceTheLastAccess)
    {
        uint64_t    cycle   = 100;
        uint64_t    elapsed = 0;
        Disk2Clock  clock;



        Assert::IsFalse (clock.TakeElapsed (elapsed), L"no source");

        clock.SetSource (&cycle);
        cycle = 130;

        Assert::AreEqual ((uint64_t) 30, clock.GetPending());
        Assert::IsTrue   (clock.TakeElapsed (elapsed));
        Assert::AreEqual ((uint64_t) 30, elapsed);
        Assert::AreEqual ((uint64_t) 0,  clock.GetPending(), L"taken");
    }


    TEST_METHOD (ACountThatMovedBackTakesTheSyncWithIt)
    {
        uint64_t    cycle   = 5000;
        uint64_t    elapsed = 0;
        Disk2Clock  clock;



        clock.SetSource (&cycle);

        // A power cycle zeroes the CPU's count.
        cycle = 10;

        Assert::IsTrue   (clock.TakeElapsed (elapsed));
        Assert::AreEqual ((uint64_t) 0,  elapsed);
        Assert::AreEqual ((uint64_t) 10, clock.GetLastSync());

        cycle = 25;

        Assert::IsTrue   (clock.TakeElapsed (elapsed));
        Assert::AreEqual ((uint64_t) 15, elapsed, L"the next access runs on from the new count");
    }
};
