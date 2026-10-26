#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskTrackSnapshot.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "EmuTests/FluxTestImages.h"

// Disk2Controller carries two DiskImage instances; per-test heap
// allocation would otherwise blow the C6262 stack-frame budget.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2StateTests
//
//  Save and load of the Disk II controller, its two drive engines and the
//  disk media, and the copy-on-write track snapshots keyframes use. Each
//  round trip sets every saved field to a value that differs between source
//  and target, so a field missing from either side fails the compare. The
//  run-ahead tests save, run the drive through reads and guest writes, load,
//  run the same accesses again, and require the same latch values and the
//  same disk bits.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2State
{
    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    static constexpr int      kSlot        = 6;
    static constexpr Word     kMotorOn     = 0xC0E9;
    static constexpr Word     kQ6Low       = 0xC0EC;
    static constexpr Word     kQ6High      = 0xC0ED;
    static constexpr Word     kQ7Low       = 0xC0EE;
    static constexpr Word     kQ7High      = 0xC0EF;


    static std::vector<Byte> SavePart (const IMachineState & part)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = part.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    static std::vector<Byte> SaveWatcher (const Disk2AddressMarkWatcher & watcher)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = watcher.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    static std::vector<Byte> SaveRecord (const Disk2NibbleRecord & record)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = record.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    // Loads a blob into a part; on success the part must have consumed it all.
    static HRESULT LoadFrom (IMachineState & target, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);
        HRESULT      hr = S_OK;



        hr = target.LoadState (reader);

        if (SUCCEEDED (hr))
        {
            Assert::IsTrue (reader.IsAtEnd());
        }

        return hr;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeEngine
    //
    //  A drive engine with its state opened up. The bools take the seed's low
    //  four bits, so seeds whose low nibbles are complements differ in every
    //  field.
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeEngine : public Disk2NibbleEngine
    {
    public:
        void Fill (Byte seed)
        {
            constexpr int       kTrackBase  = 10;
            constexpr size_t    kBitBase    = 1000;
            constexpr uint32_t  kRngBase    = 0x13579000u;
            constexpr uint64_t  kCountBase  = 0x0000000100000000ull;
            constexpr Byte      kStateMask  = 0x0F;
            constexpr int       kClockMask  = 0x07;



            m_currentTrack  = kTrackBase + seed % kTrackBase;
            m_motorOn       = (seed & 0x01) != 0;
            m_writeMode     = (seed & 0x02) != 0;
            m_shiftLoadMode = (seed & 0x04) != 0;
            m_latchIsFresh  = (seed & 0x08) != 0;
            m_bitPos        = kBitBase + seed;
            m_lssState      = static_cast<uint8_t> (seed & kStateMask);
            m_lssClock      = seed & kClockMask;
            m_readLatch     = static_cast<uint8_t> (seed + 1);
            m_bus           = static_cast<uint8_t> (seed + 2);
            m_headWindow    = static_cast<uint8_t> (seed + 3);
            m_weakRngState  = kRngBase + seed;
            m_readNibbles   = kCountBase + seed;
            m_writeNibbles  = kCountBase + seed + 4;

            // The flux fields, with no write held: a held write needs a flux
            // track on an attached disk.
            m_fluxNow              = kCountBase + seed + 5;
            m_fluxDue              = kCountBase + seed + 6;
            m_fluxLastPulse        = kCountBase + seed + 7;
            m_fluxRevUnits         = kCountBase + seed + 8;
            m_fluxCursor.nextIndex = kBitBase + seed + 9;
            m_fluxCursor.tick      = kCountBase + seed + 10;
            m_fluxPulseClock       = kCountBase + seed + 11;
            m_fluxPulseCount       = kCountBase + seed + 12;
            m_burstSlot            = seed % kTrackBase;
            m_burstStartTick       = kCountBase + seed + 13;
            m_burstBits.assign (seed % kTrackBase + 1, static_cast<uint8_t> (seed & 0x01));
        }

        void SetBitPosition (size_t bitPos)
        {
            m_bitPos = bitPos;
        }

        void AssertSameState (const StateProbeEngine & other) const
        {
            Assert::AreEqual (m_currentTrack,         other.m_currentTrack,         L"current track");
            Assert::AreEqual (m_motorOn,              other.m_motorOn,              L"motor on");
            Assert::AreEqual (m_writeMode,            other.m_writeMode,            L"write mode");
            Assert::AreEqual (m_shiftLoadMode,        other.m_shiftLoadMode,        L"shift/load mode");
            Assert::AreEqual (m_latchIsFresh,         other.m_latchIsFresh,         L"latch is fresh");
            Assert::AreEqual (m_bitPos,               other.m_bitPos,               L"bit position");
            Assert::AreEqual (m_lssState,             other.m_lssState,             L"LSS state");
            Assert::AreEqual (m_lssClock,             other.m_lssClock,             L"LSS clock");
            Assert::AreEqual (m_readLatch,            other.m_readLatch,            L"read latch");
            Assert::AreEqual (m_bus,                  other.m_bus,                  L"bus");
            Assert::AreEqual (m_headWindow,           other.m_headWindow,           L"head window");
            Assert::AreEqual (m_weakRngState,         other.m_weakRngState,         L"weak-bit generator");
            Assert::AreEqual (m_readNibbles,          other.m_readNibbles,          L"read nibbles");
            Assert::AreEqual (m_writeNibbles,         other.m_writeNibbles,         L"write nibbles");
            Assert::AreEqual (m_fluxNow,              other.m_fluxNow,              L"flux time");
            Assert::AreEqual (m_fluxDue,              other.m_fluxDue,              L"next flux transition");
            Assert::AreEqual (m_fluxLastPulse,        other.m_fluxLastPulse,        L"last flux pulse");
            Assert::AreEqual (m_fluxRevUnits,         other.m_fluxRevUnits,         L"flux revolution");
            Assert::AreEqual (m_fluxCursor.nextIndex, other.m_fluxCursor.nextIndex, L"flux cursor index");
            Assert::AreEqual (m_fluxCursor.tick,      other.m_fluxCursor.tick,      L"flux cursor tick");
            Assert::AreEqual (m_fluxPulseClock,       other.m_fluxPulseClock,       L"flux pulse clock");
            Assert::AreEqual (m_fluxPulseCount,       other.m_fluxPulseCount,       L"flux pulse count");
            Assert::AreEqual (m_burstSlot,            other.m_burstSlot,            L"held write slot");
            Assert::AreEqual (m_burstStartTick,       other.m_burstStartTick,       L"held write start");
            Assert::IsTrue   (m_burstBits ==          other.m_burstBits,            L"held write cells");
        }
    };


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeController
    //
    //  The controller with its state opened up. Its engines are filled by
    //  loading a blob saved from a filled StateProbeEngine, and compared by
    //  their saved blobs, which match exactly when every engine field does.
    //  The address-mark watcher is filled by feeding it nibbles and compared
    //  the same way.
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeController : public Disk2Controller
    {
    public:
        StateProbeController() : Disk2Controller (kSlot) {}

        void Fill (Byte seed)
        {
            constexpr Byte      kPhaseMask     = 0x0F;
            constexpr int       kPhaseIndex    = 0x03;
            constexpr int       kTrackBase     = 20;
            constexpr uint32_t  kSpindownBase  = 500000;
            constexpr uint32_t  kSpinupBase    = 100;
            constexpr uint32_t  kIdleBase      = 50;
            constexpr uint64_t  kSyncBase      = 0x0000ABCD00000000ull;
            StateProbeEngine    engine;
            HRESULT             hr             = S_OK;
            int                 drive          = 0;



            m_phases                  = static_cast<uint8_t> (seed & kPhaseMask);
            m_phase                   = seed & kPhaseIndex;
            m_quarterTrack[0]         = kTrackBase + seed % kTrackBase;
            m_quarterTrack[1]         = kTrackBase + (seed + 7) % kTrackBase;
            m_motorOn                 = (seed & 0x01) != 0;
            m_activeDrive             = (seed & 0x02) != 0 ? 1 : 0;
            m_q6                      = (seed & 0x04) != 0;
            m_q7                      = (seed & 0x08) != 0;
            m_busySinceIdleCallback   = (seed & 0x01) == 0;
            m_motorSpindownCycles     = kSpindownBase + seed;
            m_motorSpinupRemaining    = kSpinupBase + seed;
            m_iwmModeReg              = static_cast<Byte> (seed + 5);
            m_cyclesSinceIdleCallback = kIdleBase + seed;
            m_lastCpuSync             = kSyncBase + seed;

            for (drive = 0; drive < kDriveCount; drive++)
            {
                engine.Fill (static_cast<Byte> (seed + drive * 0x10));

                hr = LoadFrom (m_engine[drive], SavePart (engine));
                Assert::AreEqual (S_OK, hr);
            }

            FillWatcher (seed);
            FillRecord (seed);
        }

        // Adds the seed's count of entries, each with fields from the seed, and
        // marks a disk change on one drive.
        void FillRecord (Byte seed)
        {
            int  i = 0;



            for (i = 0; i < seed + 3; i++)
            {
                m_nibbleRecord.Add (i & 1, static_cast<Byte> (seed + i), static_cast<Byte> (i & 0x0E), static_cast<Word> (seed * 0x100 + i));
            }

            m_nibbleRecord.NoteDiskChange (seed & 1);
        }

        // Steps the watcher to the seed's quarter track, counts the seed's
        // nibbles toward a stall, and opens a written data field.
        void FillWatcher (Byte seed)
        {
            Byte  i = 0;



            m_addrMarkWatcher.NoteHeadMoved (seed);

            for (i = 0; i < seed; i++)
            {
                m_addrMarkWatcher.ObserveNibble (0xFF);
            }

            m_addrMarkWatcher.ObserveWrittenNibble (Disk2AddressMarkWatcher::kAddrMarkPrologue0);
            m_addrMarkWatcher.ObserveWrittenNibble (Disk2AddressMarkWatcher::kAddrMarkPrologue1);
        }

        const Disk2AddressMarkWatcher & GetWatcher() const
        {
            return m_addrMarkWatcher;
        }

        std::vector<Byte> SaveEngine (int drive) const
        {
            return SavePart (m_engine[drive]);
        }

        void AssertSameState (const StateProbeController & other) const
        {
            int  drive = 0;



            Assert::AreEqual (m_phases,                  other.m_phases,                  L"phases");
            Assert::AreEqual (m_phase,                   other.m_phase,                   L"phase");
            Assert::AreEqual (m_quarterTrack[0],         other.m_quarterTrack[0],         L"drive 1's quarter track");
            Assert::AreEqual (m_quarterTrack[1],         other.m_quarterTrack[1],         L"drive 2's quarter track");
            Assert::AreEqual (m_motorOn,                 other.m_motorOn,                 L"motor on");
            Assert::AreEqual (m_activeDrive,             other.m_activeDrive,             L"active drive");
            Assert::AreEqual (m_q6,                      other.m_q6,                      L"Q6");
            Assert::AreEqual (m_q7,                      other.m_q7,                      L"Q7");
            Assert::AreEqual (m_busySinceIdleCallback,   other.m_busySinceIdleCallback,   L"busy since idle callback");
            Assert::AreEqual (m_motorSpindownCycles,     other.m_motorSpindownCycles,     L"spindown cycles");
            Assert::AreEqual (m_motorSpinupRemaining,    other.m_motorSpinupRemaining,    L"spin-up remaining");
            Assert::AreEqual (m_iwmModeReg,              other.m_iwmModeReg,              L"IWM mode register");
            Assert::AreEqual (m_cyclesSinceIdleCallback, other.m_cyclesSinceIdleCallback, L"cycles since idle callback");
            Assert::AreEqual (m_lastCpuSync,             other.m_lastCpuSync,             L"last CPU sync");

            for (drive = 0; drive < kDriveCount; drive++)
            {
                Assert::IsTrue (SaveEngine (drive) == other.SaveEngine (drive), L"engine state");
            }

            Assert::IsTrue (SaveWatcher (m_addrMarkWatcher) == SaveWatcher (other.m_addrMarkWatcher), L"watcher state");
            Assert::IsTrue (SaveRecord  (m_nibbleRecord)    == SaveRecord  (other.m_nibbleRecord),    L"the recent nibbles");
        }
    };


    // A 140 KB DOS-order image whose bytes vary, nibblized into the disk.
    static void LoadTestDisk (DiskImage & disk)
    {
        constexpr size_t  kMultiplier = 31;
        std::vector<Byte> raw (DiskImage::kDos33ImageSize);
        size_t            i           = 0;



        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * kMultiplier + (i >> CHAR_BIT));
        }

        disk.LoadFromBytes (DiskFormat::Dsk, raw, "");
        Assert::IsTrue (disk.IsLoaded());
    }


    // A 35-track WOZ, each track 51,200 bits of a varied pattern, mapped at
    // 4N to 4N+3, loaded into the disk.
    static void LoadTestWoz (DiskImage & disk)
    {
        constexpr int                   kTracks    = 35;
        constexpr size_t                kTrackBits = 51200;
        std::vector<WozSyntheticTrack>  tracks (kTracks);
        std::vector<Byte>               woz;
        size_t                          i          = 0;
        int                             n          = 0;



        for (n = 0; n < kTracks; n++)
        {
            tracks[n].data.resize (kTrackBits / CHAR_BIT);

            for (i = 0; i < tracks[n].data.size(); i++)
            {
                tracks[n].data[i] = static_cast<Byte> (0x96 + i * 7 + n);
            }

            tracks[n].bitCount      = kTrackBits;
            tracks[n].quarterTracks = { 4 * n, 4 * n + 1, 4 * n + 2, 4 * n + 3 };
        }

        Assert::AreEqual (S_OK, WozLoader::BuildSyntheticV21 (tracks, woz));

        disk.LoadFromBytes (DiskFormat::Woz, woz, "test.woz");
        Assert::IsTrue (disk.IsLoaded());
    }


    // Steps the selected drive's head outward to `quarterTrack`, a half
    // track at a time. The motor must be on.
    static void StepTo (Disk2Controller & controller, int quarterTrack)
    {
        int  phase = 0;



        while (controller.GetQuarterTrack() < quarterTrack)
        {
            phase = ((controller.GetQuarterTrack() / 2) + 1) & 3;

            controller.Write (static_cast<Word> (0xC0E1 + phase * 2), 0x00);
            controller.Write (static_cast<Word> (0xC0E0 + phase * 2), 0x00);
        }
    }


    // A state of a WOZ disk with 35 slots, as a build before the reservation
    // saved one.
    static std::vector<Byte> SaveThirtyFiveSlotWozState()
    {
        auto  source = std::make_unique<DiskImage>();



        source->SetSourceFormat  (DiskFormat::Woz);
        source->SetLoadedForTest (true, false);
        Assert::AreEqual (DiskImage::kDefaultTrackCount, source->GetTrackCount());

        return SavePart (*source);
    }


    // Writes a run of bits into a track, starting at `start`.
    static void WriteRun (DiskImage & disk, int track, size_t start, size_t count, uint8_t bit)
    {
        size_t  i = 0;



        for (i = 0; i < count; i++)
        {
            disk.WriteBit (track, start + i, bit);
        }
    }


    // Compares every saved field of two disks.
    static void AssertSameMedia (const DiskImage & expected, const DiskImage & actual)
    {
        int  track = 0;



        Assert::AreEqual (expected.GetTrackCount(), actual.GetTrackCount(), L"track count");

        for (track = 0; track < expected.GetTrackCount(); track++)
        {
            Assert::AreEqual (expected.GetTrackBitCount (track), actual.GetTrackBitCount (track), L"track bit count");
            Assert::IsTrue   (expected.GetTrackBits (track) == actual.GetTrackBits (track),        L"track bits");
            Assert::IsTrue   (expected.GetTrackKind (track) == actual.GetTrackKind (track),        L"track kind");
            Assert::IsTrue   (expected.GetFluxTrack (track).GetBytes() == actual.GetFluxTrack (track).GetBytes(), L"flux bytes");
        }

        // The dirty flags say what the host file lacks, so a load does not
        // copy them; DiskDirtyFlagsFollowTheLoadedBits covers them.
        Assert::AreEqual (expected.IsImageWriteProtected(), actual.IsImageWriteProtected(), L"image write protect");
        Assert::AreEqual (expected.IsUserWriteProtected(),  actual.IsUserWriteProtected(),  L"user write protect");
    }


    // Copies a disk's saved fields into another disk with the same medium.
    static void CopyMedia (const DiskImage & source, DiskImage & target)
    {
        std::vector<Byte>  bytes = SavePart (source);
        HRESULT            hr    = S_OK;



        hr = LoadFrom (target, bytes);
        Assert::AreEqual (S_OK, hr);
    }


    // Turns a track into the flux track its bits would be at nominal cells.
    static void MakeFluxTrack (DiskImage & disk, int track)
    {
        disk.SetFluxTrack (track, FluxTestImages::BitsToNominalFlux (disk.GetTrackBits (track), disk.GetTrackBitCount (track)));
        Assert::IsTrue (disk.GetTrackKind (track) == TrackKind::Flux);
    }


    // Puts the controller in write mode and writes `count` bytes.
    static void WriteBytes (Disk2Controller & controller, int count)
    {
        constexpr uint32_t  kByteCycles = 32;
        int                 i           = 0;



        for (i = 0; i < count; i++)
        {
            controller.Write (kQ7High, static_cast<Byte> (0x96 + i));
            controller.Tick (kByteCycles);
        }
    }


    // Finishes a write started before a save, then reads, returning every
    // latch value the CPU saw.
    static std::vector<Byte> FinishWriteAndRead (Disk2Controller & controller)
    {
        constexpr int       kWrites     = 100;
        constexpr int       kReads      = 3000;
        constexpr uint32_t  kReadCycles = 7;
        std::vector<Byte>   seen;
        int                 i           = 0;



        WriteBytes (controller, kWrites);

        seen.push_back (controller.Read (kQ7Low));
        seen.push_back (controller.Read (kQ6Low));

        for (i = 0; i < kReads; i++)
        {
            controller.Tick (kReadCycles);
            seen.push_back (controller.Read (kQ6Low));
        }

        return seen;
    }


    // Drives the controller through a fixed script of reads, a guest write,
    // and more reads, returning every latch value the CPU saw.
    static std::vector<Byte> RunScript (Disk2Controller & controller)
    {
        constexpr int       kReads      = 3000;
        constexpr int       kWrites     = 200;
        constexpr uint32_t  kReadCycles = 7;
        constexpr uint32_t  kByteCycles = 32;
        std::vector<Byte>   seen;
        int                 i           = 0;



        for (i = 0; i < kReads; i++)
        {
            controller.Tick (kReadCycles);
            seen.push_back (controller.Read (kQ6Low));
        }

        seen.push_back (controller.Read (kQ6High));
        seen.push_back (controller.Read (kQ7Low));

        for (i = 0; i < kWrites; i++)
        {
            controller.Write (kQ7High, static_cast<Byte> (0x96 + i));
            controller.Tick (kByteCycles);
        }

        seen.push_back (controller.Read (kQ7Low));
        seen.push_back (controller.Read (kQ6Low));

        for (i = 0; i < kReads; i++)
        {
            controller.Tick (kReadCycles);
            seen.push_back (controller.Read (kQ6Low));
        }

        return seen;
    }


    TEST_CLASS (Disk2StateTests)
    {
    public:
        TEST_METHOD (EngineRoundTripsEveryField)
        {
            constexpr Byte     kSeed       = 0x35;
            constexpr Byte     kTargetSeed = 0x4A;
            StateProbeEngine   source;
            StateProbeEngine   target;
            HRESULT            hr          = S_OK;



            source.Fill (kSeed);
            target.Fill (kTargetSeed);

            hr = LoadFrom (target, SavePart (source));
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (ControllerRoundTripsEveryField)
        {
            constexpr Byte                  kSeed       = 0x35;
            constexpr Byte                  kTargetSeed = 0x4A;
            auto                            source      = std::make_unique<StateProbeController>();
            auto                            target      = std::make_unique<StateProbeController>();
            HRESULT                         hr          = S_OK;



            source->Fill (kSeed);
            target->Fill (kTargetSeed);

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (S_OK, hr);

            source->AssertSameState (*target);
        }


        //  A head at the outer stop, quarter track 158, saves and loads.
        TEST_METHOD (ControllerHeadAtTheOuterStopRoundTrips)
        {
            constexpr int  kOuterStopHalfTracks = 79;
            auto           source               = std::make_unique<Disk2Controller> (6);
            auto           target               = std::make_unique<Disk2Controller> (6);
            int            i                    = 0;
            int            phase                = 0;
            HRESULT        hr                   = S_OK;



            source->Write (0xC0E9, 0x00);      // motor on: the stepper works only then

            for (i = 0; i < kOuterStopHalfTracks; i++)
            {
                phase = ((source->GetQuarterTrack() / 2) + 1) & 3;

                source->Write (static_cast<Word> (0xC0E1 + phase * 2), 0x00);
                source->Write (static_cast<Word> (0xC0E0 + phase * 2), 0x00);
            }

            Assert::AreEqual (158, source->GetQuarterTrack(), L"the head is at the outer stop");

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (S_OK, hr, L"the state loads");
            Assert::AreEqual (158, target->GetQuarterTrack(), L"with the head at the outer stop");
        }


        //  A version 1 state has no watcher section: it loads, and the
        //  watcher starts at rest over the loaded head position.
        TEST_METHOD (ControllerVersionOneLoadsWithTheWatcherAtRest)
        {
            constexpr uint16_t        kVersionOne   = 1;
            constexpr Byte            kQuarterTrack = 24;
            constexpr Byte            kTargetSeed   = 0x4A;
            auto                      target        = std::make_unique<StateProbeController>();
            Disk2NibbleEngine         engine;
            Disk2AddressMarkWatcher   expected;
            StateWriter               writer;
            HRESULT                   hr            = S_OK;
            int                       drive         = 0;



            target->Fill (kTargetSeed);

            writer.BeginSection (Disk2Controller::kStateTag, kVersionOne);
            writer.WriteByte    (0);                // phases
            writer.WriteByte    (0);                // phase
            writer.WriteByte    (kQuarterTrack);
            writer.WriteBool    (false);            // motor on
            writer.WriteUInt32  (0);                // spin-down cycles
            writer.WriteUInt32  (0);                // spin-up remaining
            writer.WriteByte    (0);                // active drive
            writer.WriteBool    (false);            // Q6
            writer.WriteBool    (false);            // Q7
            writer.WriteBool    (false);            // IWM mode
            writer.WriteByte    (0);                // IWM mode register
            writer.WriteUInt32  (0);                // cycles since idle callback
            writer.WriteBool    (false);            // busy since idle callback
            writer.WriteUInt64  (0);                // last CPU sync

            for (drive = 0; drive < Disk2Controller::kDriveCount; drive++)
            {
                hr = engine.SaveState (writer);
                Assert::AreEqual (S_OK, hr);
            }

            hr = writer.EndSection();
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (*target, writer.GetBytes());
            Assert::AreEqual (S_OK, hr);

            expected.NoteHeadMoved (kQuarterTrack);
            Assert::IsTrue (SaveWatcher (expected) == SaveWatcher (target->GetWatcher()), L"watcher at rest");
        }


        //  Before version 3 one head position served both drives: each drive
        //  takes it, and the drive not selected has its engine moved there.
        TEST_METHOD (ControllerVersionsBeforeThreeLoadBothHeadsAtTheOnePosition)
        {
            constexpr Byte  kQuarterTrack = 24;
            constexpr Byte  kTargetSeed   = 0x4A;



            for (uint16_t version : { (uint16_t) 1, (uint16_t) 2 })
            {
                for (Byte activeDrive : { (Byte) 0, (Byte) 1 })
                {
                    auto     target = std::make_unique<StateProbeController>();
                    HRESULT  hr     = S_OK;



                    target->Fill (kTargetSeed);

                    hr = LoadFrom (*target, MakeOldState (version, kQuarterTrack, activeDrive));
                    Assert::AreEqual (S_OK, hr);

                    Assert::AreEqual ((int) kQuarterTrack, target->GetQuarterTrack (0), L"drive 1's head");
                    Assert::AreEqual ((int) kQuarterTrack, target->GetQuarterTrack (1), L"drive 2's head");
                    Assert::AreEqual ((int) kQuarterTrack, target->GetEngine (activeDrive ^ 1).GetCurrentTrack(), L"the other drive's engine");
                    Assert::AreEqual ((uint64_t) 0,        target->GetNibbleRecord().GetCount(), L"no recent nibbles");
                }
            }
        }


        //  A state of an earlier version, with every field but the head and
        //  the drive select at rest, and the watcher's section from version 2.
        static std::vector<Byte> MakeOldState (uint16_t version, Byte quarterTrack, Byte activeDrive)
        {
            constexpr uint16_t        kFirstWithWatcher = 2;
            Disk2NibbleEngine         engine;
            Disk2AddressMarkWatcher   watcher;
            StateWriter               writer;
            HRESULT                   hr                = S_OK;
            int                       drive             = 0;



            writer.BeginSection (Disk2Controller::kStateTag, version);
            writer.WriteByte    (0);                // phases
            writer.WriteByte    (0);                // phase
            writer.WriteByte    (quarterTrack);
            writer.WriteBool    (false);            // motor on
            writer.WriteUInt32  (0);                // spin-down cycles
            writer.WriteUInt32  (0);                // spin-up remaining
            writer.WriteByte    (activeDrive);
            writer.WriteBool    (false);            // Q6
            writer.WriteBool    (false);            // Q7
            writer.WriteBool    (false);            // IWM mode
            writer.WriteByte    (0);                // IWM mode register
            writer.WriteUInt32  (0);                // cycles since idle callback
            writer.WriteBool    (false);            // busy since idle callback
            writer.WriteUInt64  (0);                // last CPU sync

            for (drive = 0; drive < Disk2Controller::kDriveCount; drive++)
            {
                hr = engine.SaveState (writer);
                Assert::AreEqual (S_OK, hr);
            }

            if (version >= kFirstWithWatcher)
            {
                hr = watcher.SaveState (writer);
                Assert::AreEqual (S_OK, hr);
            }

            hr = writer.EndSection();
            Assert::AreEqual (S_OK, hr);

            return writer.GetBytes();
        }


        TEST_METHOD (ControllerWithOtherIwmWiringFails)
        {
            auto     source = std::make_unique<Disk2Controller> (kSlot);
            auto     target = std::make_unique<Disk2Controller> (kSlot);
            HRESULT  hr     = S_OK;



            source->SetIwmMode (true);

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (EngineBitCursorPastTrackFails)
        {
            constexpr size_t   kPastEnd = 0x7FFFFFFF;
            auto               disk     = std::make_unique<DiskImage>();
            StateProbeEngine   source;
            StateProbeEngine   target;
            HRESULT            hr       = S_OK;



            LoadTestDisk (*disk);
            target.SetDiskImage (disk.get());

            // No disk on the source, so its cursor is unchecked when saved;
            // the target's track is far shorter than the cursor.
            source.Fill (0);
            source.SetBitPosition (kPastEnd);

            hr = LoadFrom (target, SavePart (source));
            Assert::AreEqual (kInvalidData, hr);
        }

        TEST_METHOD (DiskRoundTripsEveryField)
        {
            constexpr int       kWrittenTrack = 3;
            constexpr int       kResizedTrack = 7;
            constexpr size_t    kResizedBits  = 50000;
            constexpr size_t    kRunStart     = 100;
            constexpr size_t    kRunLength    = 64;
            auto                source        = std::make_unique<DiskImage>();
            auto                target        = std::make_unique<DiskImage>();
            HRESULT             hr            = S_OK;



            LoadTestDisk (*source);
            LoadTestDisk (*target);

            // Source: guest writes on one track, a track of another length,
            // and both write-protect flags set. Target: a different track
            // dirty and no flags, so every field differs.
            WriteRun (*source, kWrittenTrack, kRunStart, kRunLength, 1);
            source->ResizeTrack (kResizedTrack, kResizedBits);
            source->SetImageWriteProtected (true);
            source->SetUserWriteProtected  (true);

            WriteRun (*target, kResizedTrack, kRunStart, kRunLength, 0);
            target->ClearDirty();

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (S_OK, hr);

            AssertSameMedia (*source, *target);
        }


        //  A flux track's kind and bytes are saved, so a load puts back a
        //  guest write spliced into one and turns a track that became flux
        //  since the save back into bits.
        TEST_METHOD (DiskRoundTripsFluxTracks)
        {
            constexpr int       kFluxTrack  = 2;
            constexpr int       kOtherTrack = 4;
            constexpr uint64_t  kSpliceTick = 5000;
            constexpr size_t    kSpliceBits = 64;
            auto                source      = std::make_unique<DiskImage>();
            auto                target      = std::make_unique<DiskImage>();
            std::vector<Byte>   unspliced;
            HRESULT             hr          = S_OK;



            LoadTestDisk (*source);
            LoadTestDisk (*target);

            MakeFluxTrack (*source, kFluxTrack);
            unspliced = source->GetFluxTrack (kFluxTrack).GetBytes();
            source->SpliceFluxBulk (kFluxTrack, kSpliceTick, std::vector<uint8_t> (kSpliceBits, 1));
            Assert::IsFalse (source->GetFluxTrack (kFluxTrack).GetBytes() == unspliced, L"the splice changed the flux");

            MakeFluxTrack (*target, kOtherTrack);
            target->ClearDirty();

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (S_OK, hr);

            AssertSameMedia (*source, *target);
            Assert::IsTrue (target->IsTrackDirty (kFluxTrack),  L"the load changed this track to flux");
            Assert::IsTrue (target->IsTrackDirty (kOtherTrack), L"and this one back to bits");
        }


        //  Saved in the middle of a guest write to a flux track, the drive
        //  runs the same from the load: the flux timeline and the write held
        //  for the track are part of the state.
        TEST_METHOD (FluxDriveRunsTheSameAfterLoadMidWrite)
        {
            constexpr int       kWritesBeforeSave = 100;
            auto                controller        = std::make_unique<Disk2Controller> (kSlot);
            DiskImage         * disk              = controller->GetDisk (0);
            std::vector<Byte>   savedDisk;
            std::vector<Byte>   savedController;
            std::vector<Byte>   firstRun;
            std::vector<Byte>   secondRun;
            std::vector<Byte>   firstMedia;
            std::vector<Byte>   secondMedia;
            HRESULT             hr                = S_OK;



            LoadTestDisk (*disk);
            MakeFluxTrack (*disk, 0);

            controller->Read (kMotorOn);
            RunScript (*controller);

            controller->Read (kQ6High);
            controller->Read (kQ7Low);
            WriteBytes (*controller, kWritesBeforeSave);

            Assert::IsTrue (controller->GetEngine (0).IsOnFluxTrack(), L"the head is over the flux track");

            savedDisk       = SavePart (*disk);
            savedController = SavePart (*controller);

            firstRun   = FinishWriteAndRead (*controller);
            firstMedia = SavePart (*disk);

            hr = LoadFrom (*disk, savedDisk);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (*controller, savedController);
            Assert::AreEqual (S_OK, hr);

            secondRun   = FinishWriteAndRead (*controller);
            secondMedia = SavePart (*disk);

            Assert::IsFalse (firstMedia == savedDisk,   L"the write after the save reached the disk");
            Assert::IsTrue  (firstRun   == secondRun,   L"latch values after load");
            Assert::IsTrue  (firstMedia == secondMedia, L"disk flux after load");
        }


        //  A load marks dirty every track whose bits it changes, since the host
        //  file holds what the disk held before it, and clears no flag.
        TEST_METHOD (DiskDirtyFlagsFollowTheLoadedBits)
        {
            constexpr int       kWrittenTrack = 3;
            constexpr int       kTargetTrack  = 7;
            constexpr int       kCleanTrack   = 11;
            constexpr size_t    kRunStart     = 100;
            constexpr size_t    kRunLength    = 64;
            auto                source        = std::make_unique<DiskImage>();
            auto                target        = std::make_unique<DiskImage>();
            HRESULT             hr            = S_OK;



            LoadTestDisk (*source);
            LoadTestDisk (*target);

            WriteRun (*source, kWrittenTrack, kRunStart, kRunLength, 1);
            source->ClearDirty();

            WriteRun (*target, kTargetTrack, kRunStart, kRunLength, 0);

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (S_OK, hr);

            Assert::IsTrue  (target->IsTrackDirty (kWrittenTrack), L"the load changed this track's bits");
            Assert::IsTrue  (target->IsTrackDirty (kTargetTrack),  L"a dirty flag is never cleared by a load");
            Assert::IsFalse (target->IsTrackDirty (kCleanTrack),   L"a track the load left alone stays clean");
            Assert::IsTrue  (target->IsDirty());
        }


        TEST_METHOD (DiskLoadIntoEmptyDriveFails)
        {
            auto     source = std::make_unique<DiskImage>();
            auto     target = std::make_unique<DiskImage>();
            HRESULT  hr     = S_OK;



            LoadTestDisk (*source);

            hr = LoadFrom (*target, SavePart (*source));
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (SnapshotSharesUnchangedTracks)
        {
            constexpr int       kWrittenTrack = 5;
            auto                disk          = std::make_unique<DiskImage>();
            DiskTrackSnapshot   first;
            DiskTrackSnapshot   second;
            int                 track         = 0;



            LoadTestDisk (*disk);

            first.Capture (*disk, nullptr);
            WriteRun (*disk, kWrittenTrack, 0, 8, 1);
            second.Capture (*disk, &first);

            for (track = 0; track < disk->GetTrackCount(); track++)
            {
                Assert::AreEqual (track != kWrittenTrack, second.IsTrackSharedWith (track, first));
            }
        }


        TEST_METHOD (SnapshotRestoresEveryField)
        {
            constexpr int       kWrittenTrack = 5;
            constexpr int       kResizedTrack = 9;
            constexpr size_t    kResizedBits  = 40000;
            auto                disk          = std::make_unique<DiskImage>();
            auto                expected      = std::make_unique<DiskImage>();
            DiskTrackSnapshot   snapshot;
            DiskTrackSnapshot   after;
            HRESULT             hr            = S_OK;
            int                 track         = 0;



            LoadTestDisk (*disk);
            LoadTestDisk (*expected);
            WriteRun (*disk, 2, 0, 8, 1);
            CopyMedia (*disk, *expected);

            snapshot.Capture (*disk, nullptr);

            // Change every field the snapshot holds.
            WriteRun (*disk, kWrittenTrack, 0, 64, 1);
            disk->ResizeTrack (kResizedTrack, kResizedBits);
            disk->ClearDirty();
            disk->SetImageWriteProtected (true);
            disk->SetUserWriteProtected  (true);

            hr = snapshot.Restore (*disk);
            Assert::AreEqual (S_OK, hr);

            AssertSameMedia (*expected, *disk);

            // Restored tracks take back their generation, so the next
            // keyframe shares them all again.
            after.Capture (*disk, &snapshot);

            for (track = 0; track < disk->GetTrackCount(); track++)
            {
                Assert::IsTrue (after.IsTrackSharedWith (track, snapshot));
            }
        }


        TEST_METHOD (SnapshotIntoEmptyDriveFails)
        {
            auto                disk  = std::make_unique<DiskImage>();
            auto                empty = std::make_unique<DiskImage>();
            DiskTrackSnapshot   snapshot;
            HRESULT             hr    = S_OK;



            LoadTestDisk (*disk);
            snapshot.Capture (*disk, nullptr);

            hr = snapshot.Restore (*empty);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (ControllerAndDiskRunTheSameAfterLoad)
        {
            auto                controller = std::make_unique<Disk2Controller> (kSlot);
            DiskImage         * disk       = controller->GetDisk (0);
            DiskTrackSnapshot   snapshot;
            std::vector<Byte>   saved;
            std::vector<Byte>   firstRun;
            std::vector<Byte>   secondRun;
            std::vector<Byte>   firstMedia;
            std::vector<Byte>   secondMedia;
            HRESULT             hr         = S_OK;



            LoadTestDisk (*disk);

            controller->Read (kMotorOn);
            RunScript (*controller);

            snapshot.Capture (*disk, nullptr);
            saved = SavePart (*controller);

            firstRun   = RunScript (*controller);
            firstMedia = SavePart (*disk);

            hr = snapshot.Restore (*disk);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (*controller, saved);
            Assert::AreEqual (S_OK, hr);

            secondRun   = RunScript (*controller);
            secondMedia = SavePart (*disk);

            Assert::IsTrue (controller->GetEngine (0).GetWriteNibbles() > 0, L"the script wrote to the disk");
            Assert::IsTrue (firstRun   == secondRun,   L"latch values after load");
            Assert::IsTrue (firstMedia == secondMedia, L"disk bits after load");
        }


        //  A step back across the first write to a blank track puts the track
        //  back to no data, and the disk is dirty, because the file differs.
        TEST_METHOD (DiskStepBackAcrossTheFirstWriteToABlankTrack)
        {
            constexpr int       kTrack36   = 4 * 36;
            auto                controller = std::make_unique<Disk2Controller> (kSlot);
            DiskImage         * disk       = controller->GetDisk (0);
            std::vector<Byte>   saved;
            HRESULT             hr         = S_OK;



            LoadTestWoz (*disk);

            controller->Read (kMotorOn);
            StepTo (*controller, kTrack36);

            saved = SavePart (*disk);

            controller->Read (kQ6High);
            WriteBytes (*controller, 10);

            Assert::AreEqual (36, disk->ResolveQuarterTrack (kTrack36), L"the guest's write created track 36");

            disk->ClearDirty();

            hr = LoadFrom (*disk, saved);
            Assert::AreEqual (S_OK, hr, L"the state from before the write loads");

            Assert::AreEqual (-1, disk->ResolveQuarterTrack (kTrack36), L"track 36 has no data again");
            Assert::IsTrue   (disk->IsDirty(), L"and the disk is dirty");
        }


        //  From a keyframe taken before a track is created, the controller
        //  and the disk run the same again, the creation included.
        TEST_METHOD (ControllerAndDiskRunTheSameAcrossATrackCreation)
        {
            constexpr int       kTrack36   = 4 * 36;
            auto                controller = std::make_unique<Disk2Controller> (kSlot);
            DiskImage         * disk       = controller->GetDisk (0);
            DiskTrackSnapshot   snapshot;
            std::vector<Byte>   saved;
            std::vector<Byte>   before;
            std::vector<Byte>   firstRun;
            std::vector<Byte>   secondRun;
            std::vector<Byte>   firstMedia;
            std::vector<Byte>   secondMedia;
            HRESULT             hr         = S_OK;



            LoadTestWoz (*disk);

            controller->Read (kMotorOn);
            StepTo (*controller, kTrack36);

            snapshot.Capture (*disk, nullptr);
            saved  = SavePart (*controller);
            before = SavePart (*disk);

            firstRun   = RunScript (*controller);
            firstMedia = SavePart (*disk);

            hr = snapshot.Restore (*disk);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (*controller, saved);
            Assert::AreEqual (S_OK, hr);

            secondRun   = RunScript (*controller);
            secondMedia = SavePart (*disk);

            Assert::IsFalse (firstMedia == before,      L"the script created track 36");
            Assert::IsTrue  (firstRun   == secondRun,   L"latch values after load");
            Assert::IsTrue  (firstMedia == secondMedia, L"disk bits after load");
        }


        //  A WOZ state from before the reservation has fewer slots. It loads
        //  when every surplus slot of the disk is an empty bit slot.
        TEST_METHOD (DiskLoadsAWozStateWithFewerBlankSlots)
        {
            auto     target = std::make_unique<DiskImage>();
            HRESULT  hr     = S_OK;



            LoadTestWoz (*target);
            target->EnsureTrackSlots (DiskImage::kMaxTracks);

            hr = LoadFrom (*target, SaveThirtyFiveSlotWozState());
            Assert::AreEqual (S_OK, hr, L"the state loads");
        }


        TEST_METHOD (DiskRejectsFewerSlotsWhenTheSurplusHoldsData)
        {
            auto     target = std::make_unique<DiskImage>();
            HRESULT  hr     = S_OK;



            LoadTestWoz (*target);
            target->EnsureTrackSlots (DiskImage::kMaxTracks);
            target->ResizeTrack (38, 512);

            hr = LoadFrom (*target, SaveThirtyFiveSlotWozState());
            Assert::AreEqual (kInvalidData, hr, L"a surplus slot with data is another disk");
        }


        TEST_METHOD (DiskRejectsFewerSlotsOnASectorImage)
        {
            auto               source = std::make_unique<DiskImage>();
            auto               target = std::make_unique<DiskImage>();
            std::vector<Byte>  saved;
            HRESULT            hr     = S_OK;



            LoadTestDisk (*source);
            saved = SavePart (*source);

            LoadTestDisk (*target);
            target->EnsureTrackSlots (DiskImage::kMaxTracks);

            hr = LoadFrom (*target, saved);
            Assert::AreEqual (kInvalidData, hr, L"a sector image's slot count is fixed");
        }


        //  A snapshot restored over a created track takes it back to no data,
        //  and the engine looks the slot up again, so the next write creates
        //  the track again and lands.
        TEST_METHOD (SnapshotRestoreAcrossATrackCreation_RefreshesTheEngine)
        {
            constexpr int       kTrack36   = 4 * 36;
            auto                controller = std::make_unique<Disk2Controller> (kSlot);
            DiskImage         * disk       = controller->GetDisk (0);
            DiskTrackSnapshot   snapshot;
            HRESULT             hr         = S_OK;



            LoadTestWoz (*disk);

            controller->Read (kMotorOn);
            StepTo (*controller, kTrack36);

            snapshot.Capture (*disk, nullptr);

            controller->Read (kQ6High);
            WriteBytes (*controller, 10);
            Assert::AreEqual (36, disk->ResolveQuarterTrack (kTrack36), L"the write created track 36");

            hr = snapshot.Restore (*disk);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (-1, disk->ResolveQuarterTrack (kTrack36), L"the restore took it back to no data");

            WriteBytes (*controller, 10);
            Assert::AreEqual (36, disk->ResolveQuarterTrack (kTrack36), L"and the next write creates it again");
        }
    };
}
