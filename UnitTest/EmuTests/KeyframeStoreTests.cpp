#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/SnapshotCompressor.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kStateBytes     = 140 * 1024;
static constexpr uint64_t  s_kInterval       = 1000;
static constexpr size_t    s_kChangedPerStep = 16;
static constexpr size_t    s_kChangeStride   = 4099;
static constexpr int       s_kKeyframeDisk   = 6;
static constexpr int       s_kKeyframeDrive  = 0;
static constexpr size_t    s_kDiskPattern    = 11;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStoreTests
//
//  Periodic whole-machine snapshots: every stored keyframe restores to the
//  exact bytes it was given, differences between whole snapshots cost little,
//  the byte budget drops the oldest group first, and each keyframe carries a
//  checksum of the state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeStoreTests)
{
public:

    TEST_METHOD (CompressorRoundTripsAndShrinksRepetitiveData)
    {
        SnapshotCompressor  compressor;
        std::vector<Byte>   state = MakeState (3);
        std::vector<Byte>   packed;
        std::vector<Byte>   unpacked;
        HRESULT             hr    = S_OK;



        hr = compressor.Compress (state.data(), state.size(), packed);
        AssertSucceeded (hr, L"Compress");
        Assert::IsTrue (packed.size() < state.size() / 4, L"a mostly regular state must pack to under a quarter");

        hr = compressor.Decompress (packed, state.size(), unpacked);
        AssertSucceeded (hr, L"Decompress");
        Assert::IsTrue (unpacked == state, L"unpacking must give back the exact bytes");

        hr = compressor.Decompress (packed, state.size() + 1, unpacked);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"a wrong unpacked size is corrupt data");
    }


    TEST_METHOD (EveryKeyframeRestoresExactly)
    {
        constexpr size_t   kCount   = 75;
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr       = S_OK;
        size_t             i        = 0;



        settings.intervalCycles = s_kInterval;
        settings.wholeEvery     = 30;
        store.Configure (settings);

        for (i = 0; i < kCount; i++)
        {
            hr = store.Add (i * 10, i * s_kInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }

        Assert::AreEqual (kCount, store.GetCount(), L"every keyframe must be kept under the default budget");

        for (i = 0; i < kCount; i++)
        {
            hr = store.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");
            Assert::IsTrue (restored == MakeState (i), std::format (L"keyframe {} must restore exactly", i).c_str());
            Assert::AreEqual<uint64_t> (i * s_kInterval, store.GetInfo (i).cycle,    L"cycle");
            Assert::AreEqual<uint64_t> (i * 10,          store.GetInfo (i).position, L"position");
        }
    }


    TEST_METHOD (AWholeSnapshotStartsEachGroupAndDifferencesAreSmall)
    {
        KeyframeStore     store;
        KeyframeSettings  settings;
        HRESULT           hr = S_OK;
        size_t            i  = 0;



        settings.intervalCycles = s_kInterval;
        settings.wholeEvery     = 4;
        store.Configure (settings);

        for (i = 0; i < 9; i++)
        {
            hr = store.Add (i, i * s_kInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }

        for (i = 0; i < 9; i++)
        {
            Assert::AreEqual (i % 4 == 0, store.GetInfo (i).isWhole, std::format (L"keyframe {} whole flag", i).c_str());
        }

        Assert::IsTrue (store.GetInfo (1).storedBytes * 8 < store.GetInfo (0).storedBytes,
                        L"a difference with a few changed bytes must pack far smaller than the whole snapshot");
        Assert::IsTrue (store.GetByteCount() >= store.GetInfo (8).stateBytes,
                        L"the newest whole snapshot is held unpacked and counts against the budget");
    }


    TEST_METHOD (ASizeChangeStartsANewGroup)
    {
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  bigger   = MakeState (2);
        std::vector<Byte>  restored;
        HRESULT            hr       = S_OK;



        settings.intervalCycles = s_kInterval;
        store.Configure (settings);

        bigger.resize (bigger.size() + 300, 0x5A);

        hr = store.Add (0, 0, MakeState (0));
        AssertSucceeded (hr, L"Add 0");
        hr = store.Add (1, s_kInterval, MakeState (1));
        AssertSucceeded (hr, L"Add 1");
        hr = store.Add (2, s_kInterval * 2, bigger);
        AssertSucceeded (hr, L"Add 2");

        Assert::IsTrue (store.GetInfo (2).isWhole, L"a state of another size cannot be a difference");

        hr = store.Restore (2, restored);
        AssertSucceeded (hr, L"Restore 2");
        Assert::IsTrue (restored == bigger, L"the larger state restores exactly");

        hr = store.Restore (1, restored);
        AssertSucceeded (hr, L"Restore 1");
        Assert::IsTrue (restored == MakeState (1), L"and the older group still restores");
    }


    TEST_METHOD (TheBudgetDropsTheOldestGroupFirst)
    {
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr     = S_OK;
        size_t             i      = 0;
        size_t             count  = 0;
        uint64_t           newest = 0;



        settings.intervalCycles = s_kInterval;
        settings.wholeEvery     = 5;
        settings.budgetBytes    = s_kStateBytes * 2;
        store.Configure (settings);

        for (i = 0; i < 40; i++)
        {
            hr = store.Add (i, i * s_kInterval, MakeNoisyState (i));
            AssertSucceeded (hr, L"Add");
        }

        count  = store.GetCount();
        newest = store.GetInfo (count - 1).cycle;

        Assert::IsTrue (count < 40, L"the budget must have dropped keyframes");
        Assert::IsTrue (store.GetByteCount() <= settings.budgetBytes, L"what is left must fit the budget");
        Assert::IsTrue (store.GetInfo (0).isWhole, L"the oldest keyframe left must be a whole snapshot");
        Assert::AreEqual<uint64_t> (39 * s_kInterval, newest, L"the newest keyframe must survive");
        Assert::AreEqual<size_t> (0, (40 - count) % 5, L"keyframes are dropped a whole group at a time");

        for (i = 0; i < count; i++)
        {
            hr = store.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");
            Assert::IsTrue (restored == MakeNoisyState (40 - count + i), L"every keyframe left must restore exactly");
        }
    }


    TEST_METHOD (EachKeyframeCarriesAChecksumOfItsState)
    {
        KeyframeStore      store;
        std::vector<Byte>  state   = MakeState (7);
        std::vector<Byte>  altered = state;
        HRESULT            hr      = S_OK;



        altered[s_kStateBytes / 2] ^= 1;

        hr = store.Add (0, 0, state);
        AssertSucceeded (hr, L"Add");

        Assert::AreEqual (KeyframeStore::ComputeChecksum (state.data(), state.size()), store.GetInfo (0).checksum, L"stored checksum");
        Assert::AreNotEqual (store.GetInfo (0).checksum, KeyframeStore::ComputeChecksum (altered.data(), altered.size()), L"one bit must change it");
        Assert::IsTrue  (store.DoesStateMatch (0, state),   L"the same state matches");
        Assert::IsFalse (store.DoesStateMatch (0, altered), L"a state off by one bit does not");
    }


    TEST_METHOD (KeyframesFallDueOnCycleBoundaries)
    {
        KeyframeStore     store;
        KeyframeSettings  settings;
        size_t            index = 0;
        HRESULT           hr    = S_OK;



        settings.intervalCycles = s_kInterval;
        store.Configure (settings);

        Assert::IsTrue (store.IsDue (0), L"an empty store wants a keyframe at once");

        // Taken a few cycles late, as at the end of an instruction.
        hr = store.Add (0, s_kInterval + 3, MakeState (0));
        AssertSucceeded (hr, L"Add");

        Assert::IsFalse (store.IsDue (2 * s_kInterval - 1), L"not before the next boundary");
        Assert::IsTrue  (store.IsDue (2 * s_kInterval),     L"due on the next multiple of the interval");

        hr = store.Add (1, 2 * s_kInterval + 5, MakeState (1));
        AssertSucceeded (hr, L"Add");

        Assert::IsFalse (store.TryFindAtOrBefore (s_kInterval, index),     L"nothing before the first keyframe");
        Assert::IsTrue  (store.TryFindAtOrBefore (2 * s_kInterval, index), L"the first keyframe");
        Assert::AreEqual<size_t> (0, index);
        Assert::IsTrue  (store.TryFindAtOrBefore (UINT64_MAX, index),      L"the newest");
        Assert::AreEqual<size_t> (1, index);
    }


    TEST_METHOD (TruncatingDropsTheFutureAndKeepsTheRestRestorable)
    {
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        settings.intervalCycles = s_kInterval;
        settings.wholeEvery     = 3;
        store.Configure (settings);

        for (i = 0; i < 8; i++)
        {
            hr = store.Add (i, i * s_kInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }

        // Cut inside the second group, after the newest whole snapshot is gone.
        hr = store.TruncateAfter (2 * s_kInterval);
        AssertSucceeded (hr, L"TruncateAfter");

        Assert::AreEqual<size_t> (3, store.GetCount(), L"keyframes after the cut must go");
        Assert::IsTrue (store.IsDue (3 * s_kInterval), L"the next keyframe falls due after the cut");

        hr = store.Add (3, 3 * s_kInterval, MakeState (30));
        AssertSucceeded (hr, L"Add after the cut");

        Assert::IsTrue (store.GetInfo (3).isWhole, L"the group was full, so a new one starts");

        for (i = 0; i < 3; i++)
        {
            hr = store.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");
            Assert::IsTrue (restored == MakeState (i), L"keyframes before the cut restore exactly");
        }

        hr = store.Restore (3, restored);
        AssertSucceeded (hr, L"Restore 3");
        Assert::IsTrue (restored == MakeState (30), L"the new keyframe restores exactly");
    }


    TEST_METHOD (DifferencesAfterATruncationUseTheRightWholeSnapshot)
    {
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        settings.intervalCycles = s_kInterval;
        settings.wholeEvery     = 4;
        store.Configure (settings);

        for (i = 0; i < 6; i++)
        {
            hr = store.Add (i, i * s_kInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }

        // Back into the first group: its whole snapshot becomes the base again.
        hr = store.TruncateAfter (s_kInterval);
        AssertSucceeded (hr, L"TruncateAfter");

        hr = store.Add (2, 2 * s_kInterval, MakeState (20));
        AssertSucceeded (hr, L"Add after the cut");

        Assert::IsFalse (store.GetInfo (2).isWhole, L"the first group has room, so this is a difference");

        hr = store.Restore (2, restored);
        AssertSucceeded (hr, L"Restore");
        Assert::IsTrue (restored == MakeState (20), L"the difference must be against the first group's whole snapshot");
    }


    TEST_METHOD (CapturedMachineKeyframesLoadBackExactly)
    {
        constexpr uint64_t  kFrames = 120;
        TestMachine         machine ("Apple2e");
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        std::vector<Byte>  reSaved;
        StateWriter        writer;
        LARGE_INTEGER      frequency    = {};
        LARGE_INTEGER      start        = {};
        LARGE_INTEGER      stop         = {};
        int64_t            captureTicks = 0;
        size_t             captures     = 0;
        uint64_t           frame        = 0;
        size_t             i            = 0;
        HRESULT            hr           = S_OK;



        MountPatternDisk (machine);

        settings.intervalCycles = KeyframeSettings::kFrameCycles;
        store.Configure (settings);

        QueryPerformanceFrequency (&frequency);

        for (frame = 0; frame < kFrames; frame++)
        {
            machine.RunCycles (KeyframeSettings::kFrameCycles);

            if (!store.IsDue (machine.GetCpu()->GetTotalCycles()))
            {
                continue;
            }

            QueryPerformanceCounter (&start);
            hr = store.Capture (machine, frame);
            QueryPerformanceCounter (&stop);
            AssertSucceeded (hr, L"Capture");

            captureTicks += stop.QuadPart - start.QuadPart;
            captures++;
        }

        Assert::IsTrue (captures >= kFrames - 1, L"one keyframe per frame");

        Logger::WriteMessage (std::format ("{} keyframes, {} bytes stored, {:.1f} us per capture, state {} bytes\n",
                                           captures,
                                           store.GetByteCount(),
                                           captureTicks * 1e6 / frequency.QuadPart / captures,
                                           store.GetInfo (0).stateBytes).c_str());

        for (i = 0; i < store.GetCount(); i += 17)
        {
            hr = store.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");

            {
                StateReader  reader (restored);

                hr = machine.LoadState (reader);
                AssertSucceeded (hr, L"LoadState of a restored keyframe");
            }

            writer = StateWriter();
            hr     = machine.SaveState (writer);
            AssertSucceeded (hr, L"SaveState");

            Assert::IsTrue (store.DoesStateMatch (i, writer.GetBytes()), L"the loaded machine must match the keyframe's checksum");
            Assert::AreEqual<uint64_t> (store.GetInfo (i).cycle, machine.GetCpu()->GetTotalCycles(), L"cycle count");
        }
    }


    TEST_METHOD (TheBudgetIsFullOnceTheOldestIsDropped)
    {
        constexpr size_t    kBudget    = 4 * 1024 * 1024;
        constexpr uint64_t  kMaxFrames = 200000;
        TestMachine         machine ("Apple2e");
        KeyframeStore       store;
        KeyframeSettings    settings;
        uint64_t            beginCycle = 0;
        uint64_t            frame      = 0;
        size_t              keyframes  = 0;
        size_t              before     = 0;
        bool                isDropping = false;
        HRESULT             hr         = S_OK;



        machine.PowerCycle();

        settings.intervalCycles = KeyframeSettings::kFrameCycles;
        settings.budgetBytes    = kBudget;
        store.Configure (settings);

        for (frame = 0; frame < kMaxFrames && !isDropping; frame++)
        {
            machine.RunCycles (KeyframeSettings::kFrameCycles);

            if (!store.IsDue (machine.GetCpu()->GetTotalCycles()))
            {
                continue;
            }

            before = store.GetByteCount();

            hr = store.Capture (machine, frame);
            AssertSucceeded (hr, L"Capture");

            keyframes++;

            if (keyframes == 1)
            {
                beginCycle = store.GetInfo (0).cycle;
            }

            isDropping = store.GetInfo (0).cycle != beginCycle;
        }

        Assert::IsTrue (isDropping, L"the budget must fill and the oldest group go");

        Logger::WriteMessage (std::format ("first drop after {} keyframes: {} held of {} slots, {} bytes before the drop ({:.1f}% of the budget), "
                                           "{} after, {} reserved, {:.0f} bytes a keyframe\n",
                                           keyframes,
                                           store.GetCount(),
                                           store.GetCapacity(),
                                           before,
                                           100.0 * before / kBudget,
                                           store.GetByteCount(),
                                           store.GetReservedBytes(),
                                           (double) before / (keyframes - 1)).c_str());

        Assert::IsTrue   (before >= kBudget * 2 / 3,     L"the keyframes must use most of the budget before the oldest go, not stop at the table's count");
        Assert::IsTrue   (store.IsFull(),                L"a store that has dropped its oldest for room is full");
        Assert::AreEqual (kBudget, store.GetUsedBytes(), L"a full store reports its whole budget used");
    }


private:

    //  A mostly regular state with a few bytes that depend on step, the way
    //  successive snapshots of a machine differ.
    static std::vector<Byte> MakeState (size_t step)
    {
        std::vector<Byte>  state (s_kStateBytes);
        size_t             i = 0;



        for (i = 0; i < state.size(); i++)
        {
            state[i] = static_cast<Byte> (i / 64);
        }

        for (i = 0; i < s_kChangedPerStep; i++)
        {
            state[(step * s_kChangedPerStep + i) * s_kChangeStride % state.size()] = static_cast<Byte> (step + i + 1);
        }

        return state;
    }


    //  A state that packs poorly, so a small budget fills fast.
    static std::vector<Byte> MakeNoisyState (size_t step)
    {
        std::vector<Byte>  state (s_kStateBytes / 4);
        uint32_t           x = static_cast<uint32_t> (step * 2654435761u + 1);
        size_t             i = 0;



        for (i = 0; i < state.size(); i++)
        {
            x        = x * 1664525u + 1013904223u;
            state[i] = static_cast<Byte> (x >> 24);
        }

        return state;
    }


    static void MountPatternDisk (TestMachine & machine)
    {
        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);
        HRESULT            hr = S_OK;
        size_t             i  = 0;



        machine.PowerCycle();

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (i * s_kDiskPattern);
        }

        hr = machine.GetDiskStore().MountFromBytes (s_kKeyframeDisk, s_kKeyframeDrive, "keyframe.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes");

        machine.GetRefs().diskController->SetExternalDisk (s_kKeyframeDrive, machine.GetDiskStore().GetImage (s_kKeyframeDisk, s_kKeyframeDrive));
    }
};
