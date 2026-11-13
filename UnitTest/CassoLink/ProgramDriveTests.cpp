#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProgramDriveTests
//
//  The program's Disk II driver with the drives it is given: an empty drive,
//  two drives in turn, and a disk never formatted. The program runs on an
//  emulated //c, and the host on the test's thread.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ProgramDriveTests)
{
public:

    static constexpr uint64_t  kClockHz      = 1'020'484;
    static constexpr uint64_t  kRevolution   = 51'200;           // 4-cycle bit cells: 204,800 cycles
    static constexpr uint64_t  kEmptyBudget  = 6 * kClockHz;     // well under a read's 96 attempts, about 20 seconds
    static constexpr Byte      kFastControl  = 0x10;             // 115,200 baud
    static constexpr Byte      kCatalogTrack = 17;
    static constexpr Byte      kVolume       = 254;
    static constexpr int       kTracks       = 35;
    static constexpr int       kWritten[]    = { 0, 17, 34 };
    static constexpr size_t    kNibTrack     = 6656;

    static std::vector<Byte>  Dos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }

    static std::vector<Byte>  AllSectors()
    {
        std::vector<Byte>  sectors (16);



        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        return sectors;
    }

    //  A physical sector's 256 bytes in a DOS-ordered image.
    static std::span<const Byte>  ImageSector (const std::vector<Byte> & image, int track, int physical)
    {
        size_t  offset = (size_t) (track * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)) * 256;



        return std::span<const Byte> (image).subspan (offset, 256);
    }



    //  A drive with no disk has no address fields to find: the read ends
    //  with NOT FOUND, with the motor's start and the seek, well before its
    //  last attempt.
    TEST_METHOD (AReadOfAnEmptyDrive_EndsWellBeforeItsLastAttempt)
    {
        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;
        uint64_t           start    = 0;
        uint64_t           spent    = 0;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        start = rig.stream->GetCycles();
        AssertSucceeded (rig.host->ReadSectors (2, kCatalogTrack, sector, data, status));
        spent = rig.stream->GetCycles() - start;

        Logger::WriteMessage (std::format ("Empty drive: {} cycles, {:.1f} revolutions", spent, (double) spent / (4.0 * kRevolution)).c_str());
        Assert::AreEqual<int> (CassoLinkHost::kErrorNotFound, status.error);
        Assert::IsTrue (spent < kEmptyBudget, std::format (L"the read took {} cycles", spent).c_str());
    }



    //  A PC that takes a few seconds before its next request, as a capture's
    //  analysis between reads does, finds the drive spinning, so the read
    //  doesn't wait for it to come up to speed. A PC gone stops it.
    TEST_METHOD (APauseBetweenRequests_LeavesTheMotorOn_UntilThePCIsGone)
    {
        static constexpr uint64_t  kPause = 2 * kClockHz;
        static constexpr uint64_t  kGone  = 8 * kClockHz;

        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->ReadSectors (1, 0, sector, data, status));

        rig.machine.RunCycles (kPause);
        Assert::IsTrue (rig.machine.GetRefs().diskController->IsMotorOn(), L"the motor runs while the PC pauses");

        rig.machine.RunCycles (kGone - kPause);
        Assert::IsFalse (rig.machine.GetRefs().diskController->IsMotorOn(), L"the motor stops once the PC is gone");
    }



    //  A PC busy longer than the program waits for it, as a capture's analysis
    //  in a Debug build can be, sends a busy Idle after each Ready: the motor
    //  runs on and the PC isn't counted as gone.
    TEST_METHOD (ABusyPC_SendsABusyIdleAfterEachReady_AndTheMotorRunsOn)
    {
        static constexpr uint64_t  kStep  = kClockHz / 4;
        static constexpr int       kSteps = 40;     // ten seconds, twice the program's wait

        ProgramRig         rig (Dos33());
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));
        AssertSucceeded (rig.host->ReadSectors (1, 0, sector, data, status));

        for (int step = 0; step < kSteps; step++)
        {
            rig.machine.RunCycles (kStep);
            AssertSucceeded (rig.host->AnswerBusy (0));
        }

        Assert::IsTrue (rig.machine.GetRefs().diskController->IsMotorOn(), L"the motor runs while the PC is busy");

        AssertSucceeded (rig.host->ReadSectors (1, 0, sector, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"the next request is served");
    }



    //  Drive 1's catalog, then a failed read of the empty drive 2, then drive
    //  1's catalog again. The drive 2 read steps drive 2's head to track 0;
    //  drive 1's position is stored apart from drive 2's, so the second read of
    //  drive 1 finds its track.
    TEST_METHOD (AFailedReadOfOneDrive_LeavesTheOtherDrivesTrack)
    {
        std::vector<Byte>  disk     = Dos33();
        ProgramRig         rig (disk);
        std::vector<Byte>  sectors  = AllSectors();
        const Byte         sector[] = { 0 };
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        AssertSucceeded (rig.host->ReadSectors (1, kCatalogTrack, sectors, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"drive 1's catalog");

        AssertSucceeded (rig.host->ReadSectors (2, 0, sector, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNotFound, status.error, L"the empty drive 2");

        AssertSucceeded (rig.host->ReadSectors (1, kCatalogTrack, sectors, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"drive 1's catalog again");

        for (int s = 0; s < 16; s++)
        {
            Assert::IsTrue (std::ranges::equal (std::span<const Byte> (data).subspan ((size_t) s * 256, 256), ImageSector (disk, kCatalogTrack, s)), std::format (L"sector {}", s).c_str());
        }
    }



    //  A disk never formatted, a nibble image of sync bytes alone with no
    //  address fields: tracks written with Write track, as a whole-disk write
    //  sends a new DOS 3.3 disk, read back as written.
    TEST_METHOD (ADiskNeverFormatted_IsWrittenTrackByTrack_AndReadsBack)
    {
        ProgramRig         rig ({});
        std::vector<Byte>  blank (kTracks * kNibTrack, 0xFF);
        std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  sectors  = AllSectors();
        std::vector<Byte>  track (16 * 256);
        std::vector<Byte>  data;
        CassoLinkStatus    status;



        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Nib, blank));
        AssertSucceeded (Dos33Skeleton::Write (disk, kVolume));
        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        for (int t : kWritten)
        {
            for (int s = 0; s < 16; s++)
            {
                std::ranges::copy (ImageSector (disk, t, s), track.begin() + (ptrdiff_t) (s * 256));
            }

            AssertSucceeded (rig.host->WriteTrack (1, (Byte) t, kVolume, track, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {} written", t).c_str());

            AssertSucceeded (rig.host->ReadSectors (1, (Byte) t, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {} read", t).c_str());
            Assert::IsTrue (data == track, std::format (L"track {} reads back as written", t).c_str());
        }
    }
};
