#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2WriteHookTests
//
//  The guest-write hook the disk inspector counts from: a nibble the CPU
//  loads into the write latch counts on the record under the head while the
//  motor runs, spin-down included, and not once it has stopped or for the
//  //c's mode-register load; a write a protected disk drops counts nothing
//  and marks the drive; each arrival of the head at a quarter track counts;
//  and the counts start over when a disk goes in or is reloaded, never when
//  it is saved.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2WriteHookTests)
{
public:

    static constexpr Word      kMotorOff     = 0xC0E8;
    static constexpr Word      kMotorOn      = 0xC0E9;
    static constexpr Word      kPhase1On     = 0xC0E3;
    static constexpr Word      kQ6High       = 0xC0ED;
    static constexpr Word      kQ7High       = 0xC0EF;
    static constexpr size_t    kDskBytes     = 143360;
    static constexpr uint32_t  kPastSpindown = 3'000'000;

    static std::unique_ptr<DiskImage> MakeDisk()
    {
        auto  disk = std::make_unique<DiskImage>();



        disk->LoadFromBytes (DiskFormat::Dsk, vector<Byte> (kDskBytes, 0), "blank.dsk");
        Assert::IsTrue (disk->IsLoaded());

        return disk;
    }



    //  Q7 high, then one latch load per nibble through Q6 high.
    static void WriteNibbles (Disk2Controller & ctrl, int count)
    {
        (void) ctrl.Read (kQ7High);

        for (int i = 0; i < count; i++)
        {
            ctrl.Write (kQ6High, 0xFF);
        }
    }



    TEST_METHOD (WritesWithTheMotorRunningCountOnTheRecordUnderTheHead)
    {
        auto                              ctrl = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk = MakeDisk();



        ctrl->SetExternalDisk (0, disk.get());
        (void) ctrl->Read (kMotorOn);
        WriteNibbles (*ctrl, 5);

        Assert::AreEqual (uint64_t (5), disk->GetGuestWriteCount (disk->ResolveQuarterTrack (0)));
        Assert::AreEqual (uint64_t (0), disk->GetGuestWriteCount (disk->ResolveQuarterTrack (8)), L"only the record under the head");
        Assert::IsFalse (ctrl->IsWriteBlocked (0));
    }



    TEST_METHOD (SpindownStillCountsAStoppedMotorDoesNot)
    {
        auto                              ctrl = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk = MakeDisk();
        int                               slot = 0;



        ctrl->SetExternalDisk (0, disk.get());
        slot = disk->ResolveQuarterTrack (0);
        (void) ctrl->Read (kMotorOn);
        (void) ctrl->Read (kMotorOff);
        ctrl->Tick (1000);
        WriteNibbles (*ctrl, 2);
        Assert::AreEqual (uint64_t (2), disk->GetGuestWriteCount (slot), L"spinning down");

        ctrl->Tick (kPastSpindown);
        WriteNibbles (*ctrl, 3);
        Assert::AreEqual (uint64_t (2), disk->GetGuestWriteCount (slot), L"stopped");
    }



    TEST_METHOD (TheIwmModeRegisterLoadIsNotAWrite)
    {
        auto                              ctrl = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk = MakeDisk();



        ctrl->SetIwmMode (true);
        ctrl->SetExternalDisk (0, disk.get());
        WriteNibbles (*ctrl, 4);

        Assert::AreEqual (uint64_t (0), disk->GetGuestWriteCount (disk->ResolveQuarterTrack (0)));
    }



    TEST_METHOD (ADroppedWriteBlocksTheDriveAndCountsNothing)
    {
        auto                              ctrl = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk = MakeDisk();



        disk->SetUserWriteProtected (true);
        ctrl->SetExternalDisk (0, disk.get());
        (void) ctrl->Read (kMotorOn);
        WriteNibbles (*ctrl, 3);

        Assert::AreEqual (uint64_t (0), disk->GetGuestWriteCount (disk->ResolveQuarterTrack (0)));
        Assert::IsTrue (ctrl->IsWriteBlocked (0));
        Assert::IsFalse (ctrl->IsWriteBlocked (1), L"the other drive");
    }



    TEST_METHOD (EachArrivalAtAQuarterTrackCounts)
    {
        auto                              ctrl = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk = MakeDisk();



        ctrl->SetExternalDisk (0, disk.get());
        (void) ctrl->Read (kMotorOn);
        (void) ctrl->Read (kPhase1On);

        Assert::AreEqual (1u, ctrl->GetVisitCount (0, 2), L"phase 1 moves the head two quarter tracks");
        Assert::AreEqual (0u, ctrl->GetVisitCount (1, 2), L"per drive");
        Assert::AreEqual (0u, ctrl->GetVisitCount (0, -1));
    }



    TEST_METHOD (InsertAndReloadStartOverASaveDoesNot)
    {
        auto                              ctrl  = std::make_unique<Disk2Controller> (6);
        std::unique_ptr<DiskImage>        disk  = MakeDisk();
        std::unique_ptr<DiskImage>        other = MakeDisk();
        int                               slot  = 0;



        ctrl->SetExternalDisk (0, disk.get());
        slot = disk->ResolveQuarterTrack (0);
        (void) ctrl->Read (kMotorOn);
        (void) ctrl->Read (kPhase1On);
        WriteNibbles (*ctrl, 2);

        disk->ClearDirty();
        Assert::AreEqual (uint64_t (2), disk->GetGuestWriteCount (slot), L"a save keeps the count");

        disk->LoadFromBytes (DiskFormat::Dsk, vector<Byte> (kDskBytes, 0), "blank.dsk");
        Assert::AreEqual (uint64_t (0), disk->GetGuestWriteCount (slot), L"a reload starts it over");

        ctrl->SetExternalDisk (0, other.get());
        Assert::AreEqual (0u, ctrl->GetVisitCount (0, 2), L"another disk starts the visits over");
    }
};
