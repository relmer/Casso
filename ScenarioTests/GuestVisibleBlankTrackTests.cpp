#include "Pch.h"
#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "KeystrokeInjector.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleBlankTrackTests
//
//  Tracks a WOZ has no data for, from a guest's side: the head steps to
//  track 39 of a forty-track disk and the drive reads it, and DOS 3.3 formats
//  a WOZ with no tracks at all, which saves with every track DOS wrote.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleBlankTrackTests)
{
public:

    static constexpr int   kSlot6  = 6;
    static constexpr int   kDrive1 = 0;
    static constexpr int   kDrive2 = 1;

    //  An address prologue and three bytes no other track has.
    static inline const std::vector<Byte>  s_kMarker = { 0xD5, 0xAA, 0x96, 0xEB, 0xEE, 0xF7 };



    //  A track of sync nibbles, with the marker after each run when
    //  `hasMarker`.
    static std::vector<Byte> MakeTrackNibbles (bool hasMarker)
    {
        constexpr size_t   kTrackBytes = 6400;
        constexpr size_t   kSyncRun    = 10;
        std::vector<Byte>  nibbles;
        size_t             i           = 0;



        while (nibbles.size() + kSyncRun + s_kMarker.size() <= kTrackBytes)
        {
            for (i = 0; i < kSyncRun; i++)
            {
                nibbles.push_back (0xFF);
            }

            if (hasMarker)
            {
                nibbles.insert (nibbles.end(), s_kMarker.begin(), s_kMarker.end());
            }
        }

        nibbles.resize (kTrackBytes, 0xFF);

        return nibbles;
    }



    static bool Contains (const std::vector<Byte> & hay, const std::vector<Byte> & needle)
    {
        return std::search (hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
    }



    //  A forty-track WOZ, track 39 marked, with the head stepped out to it
    //  through the soft switches and the drive read through the data latch.
    TEST_METHOD (AGuestSeeksToTrack39OfAFortyTrackWozAndReadsIt)
    {
        constexpr int                   kTracks            = 40;
        constexpr int                   kTrack39HalfTracks = 78;
        constexpr int                   kReads             = 60000;
        TestMachine                     machine            ("Apple2e");
        std::vector<WozSyntheticTrack>  tracks             (kTracks);
        std::vector<Byte>               woz;
        std::vector<Byte>               seen;
        Disk2Controller               * controller         = nullptr;
        MemoryBus                     & bus                = machine.GetMemoryBus();
        bool                            wasValid           = false;
        Byte                            latch              = 0;
        int                             n                  = 0;
        int                             i                  = 0;
        int                             phase              = 0;



        for (n = 0; n < kTracks; n++)
        {
            tracks[n].data          = MakeTrackNibbles (n == kTracks - 1);
            tracks[n].bitCount      = tracks[n].data.size() * 8;
            tracks[n].quarterTracks = { 4 * n, 4 * n + 1, 4 * n + 2, 4 * n + 3 };
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz), L"the WOZ builds");

        machine.PowerCycle();

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot6, kDrive1, "forty.woz", DiskFormat::Woz, woz),
            L"the WOZ mounts");

        //  The engine is pumped by Tick rather than by the processor.
        controller = machine.GetRefs().diskController;
        controller->SetCpuCycleSource (nullptr);

        (void) bus.ReadByte (0xC0EA);   // drive 1
        (void) bus.ReadByte (0xC0E9);   // motor on
        controller->Tick (Disk2Controller::kMotorSpinupCycles);

        for (i = 0; i < kTrack39HalfTracks; i++)
        {
            phase = ((controller->GetQuarterTrack() / 2) + 1) & 3;

            (void) bus.ReadByte (static_cast<Word> (0xC0E1 + phase * 2));
            (void) bus.ReadByte (static_cast<Word> (0xC0E0 + phase * 2));
        }

        Assert::AreEqual (156, controller->GetQuarterTrack(), L"the head is on track 39");

        (void) bus.ReadByte (0xC0EE);   // Q7 low: read
        (void) bus.ReadByte (0xC0EC);   // Q6 low

        for (i = 0; i < kReads; i++)
        {
            controller->Tick (Disk2NibbleEngine::kCyclesPerBit);
            latch = bus.ReadByte (0xC0EC);

            if ((latch & 0x80) != 0 && !wasValid)
            {
                seen.push_back (latch);
            }

            wasValid = (latch & 0x80) != 0;
        }

        Assert::IsTrue (Contains (seen, s_kMarker), L"the drive reads track 39's marker");
    }



    //  DOS 3.3 formats a WOZ that has no tracks. Every track is created as
    //  DOS writes it, and the saved file has all thirty-five, each at TRK
    //  index N and quarter tracks 4N to 4N+3, with the greeting program DOS
    //  saved.
    TEST_METHOD (DosInitOfAWozWithNoTracksSavesEveryTrack)
    {
        constexpr int             kDosTracks  = 35;
        constexpr uint64_t        kInitCycles = 400'000'000ULL;
        TestMachine               machine    ("Apple2e");
        DiskImageStore          & store      = machine.GetDiskStore();
        std::vector<Byte>         master     = GuestSession::RequireDos33Master();
        std::vector<Byte>         blank;
        std::vector<Byte>         saved;
        std::vector<std::string>  rows;
        DiskImage                 reloaded;
        int                       n          = 0;
        int                       k          = 0;



        AssertSucceeded (WozLoader::BuildSyntheticV21 ({}, blank), L"the WOZ with no tracks builds");

        //  Every save goes to memory, and only the formatted disk's bytes are stored.
        store.SetFlushSink ([&saved] (const std::string & path, const std::vector<Byte> & bytes)
        {
            if (path == "blank.woz")
            {
                saved = bytes;
            }

            return S_OK;
        });

        GuestSession::BootToPrompt (machine, master);

        AssertSucceeded (store.MountFromBytes (kSlot6, kDrive2, "blank.woz", DiskFormat::Woz, blank), L"the blank WOZ mounts");

        //  A format takes longer than a typed line is allowed; the ceiling
        //  is spent only until the machine settles.
        KeystrokeInjector::InjectLine (machine, "INIT HELLO,D2", kInitCycles);
        Assert::IsTrue  (GuestSession::TryPageToPrompt (machine, rows), L"DOS finishes the format");
        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"), L"with no error");

        rows = GuestSession::TypeAndCollect (machine, "CATALOG,D2");
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "HELLO"), L"and catalogs it");

        AssertSucceeded (store.Flush (kSlot6, kDrive2), L"the disk saves");
        Assert::IsFalse (saved.empty(), L"to the file");

        reloaded.LoadFromBytes (DiskFormat::Woz, saved, "blank.woz");
        Assert::IsTrue (reloaded.IsLoaded(), L"which loads");

        for (n = 0; n < kDosTracks; n++)
        {
            for (k = 0; k < 4; k++)
            {
                Assert::AreEqual (n, reloaded.GetMappedSlot (4 * n + k), L"each track is at its own index, at 4N to 4N+3");
            }

            Assert::AreEqual (n, reloaded.ResolveQuarterTrack (4 * n), L"with what DOS wrote");
        }
    }
};
