#include "Pch.h"

#include "EmuTests/GuestSession.h"
#include "EmuTests/ReverseSessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IdleHistoryRateTests
//
//  History of a machine that changes little: a //e idle at the DOS 3.3
//  prompt changes about a hundred packed bytes between keyframes, so the
//  history it records grows by little more than that, rather than by a whole
//  snapshot, most of it unchanged disk tracks, every few seconds. The meter
//  of how full the budget is counts everything the budget pays for, so it
//  reaches the budget when the oldest start being dropped.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IdleHistoryRateTests)
{
public:

    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint32_t  kSeed            = 0x2545F491;
    static constexpr uint32_t  kLcgMultiplier   = 1664525;
    static constexpr uint32_t  kLcgIncrement    = 1013904223;
    static constexpr int       kHighByteShift   = 24;


    //  Incompressible bytes, the same on every run, so a whole snapshot packs
    //  to about its own size and a difference of one byte to almost nothing.
    static std::vector<Byte> MakeNoise (size_t size)
    {
        std::vector<Byte>  bytes (size, 0);
        uint32_t           state = kSeed;
        size_t             i     = 0;



        for (i = 0; i < size; i++)
        {
            state    = state * kLcgMultiplier + kLcgIncrement;
            bytes[i] = static_cast<Byte> (state >> kHighByteShift);
        }

        return bytes;
    }


    //  Measured: 2 emulated minutes idle at the prompt recorded 2.9 MB while
    //  every 30th keyframe was whole (a 118 KB packed snapshot every five
    //  seconds against differences of about 105 bytes), and 0.08 MB after.
    TEST_METHOD (IdleAtTheDos33PromptRecordsLittle)
    {
        constexpr uint64_t  kSeconds   = 120;
        constexpr size_t    kMostBytes = 512 * 1024;
        TestMachine         machine    ("Apple2eEnhanced");
        ReverseController   controller (machine);
        std::vector<Byte>   master     = GuestSession::RequireDos33Master();
        HRESULT             hr         = S_OK;
        size_t              startBytes = 0;
        size_t              grown      = 0;
        size_t              wholes     = 0;
        size_t              count      = 0;
        size_t              i          = 0;
        uint64_t            frame      = 0;



        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (machine, master);

        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending after the first keyframe");

        startBytes = controller.GetKeyframes().GetByteCount();

        for (frame = 0; frame < kSeconds * kFramesPerSecond; frame++)
        {
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        count = controller.GetKeyframes().GetCount();
        grown = controller.GetKeyframes().GetByteCount() - startBytes;

        for (i = 0; i < count; i++)
        {
            wholes += controller.GetKeyframes().GetInfo (i).isWhole ? 1 : 0;
        }

        Assert::IsTrue (count > kSeconds * 5, L"a keyframe was taken every ten frames");
        Assert::IsTrue (grown < kMostBytes, std::format (L"two idle minutes recorded {} bytes in {} keyframes, {} of them whole", grown, count, wholes).c_str());
    }


    //  A state that changes one byte per keyframe keeps one group going past
    //  wholeEvery, since its differences pack far smaller than its whole.
    TEST_METHOD (AStateThatChangesLittleKeepsOneGroup)
    {
        constexpr size_t   kStateBytes = 64 * 1024;
        constexpr size_t   kCount      = 300;
        constexpr uint64_t kInterval   = 1000;
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  state       = MakeNoise (kStateBytes);
        std::vector<Byte>  restored;
        HRESULT            hr          = S_OK;
        size_t             wholes      = 0;
        size_t             i           = 0;



        settings.intervalCycles = kInterval;
        store.Configure (settings);

        for (i = 0; i < kCount; i++)
        {
            state[i] ^= 1;

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        for (i = 0; i < kCount; i++)
        {
            wholes += store.GetInfo (i).isWhole ? 1 : 0;
        }

        Assert::AreEqual<size_t> (1, wholes, L"one whole snapshot serves every keyframe");

        hr = store.Restore (kCount - 1, restored);
        AssertSucceeded (hr, L"Restore");
        Assert::IsTrue (restored == state, L"and the newest still restores exactly");
    }


    //  A machine whose differences grow ends its group once they pack to its
    //  whole snapshot, and no group outgrows longestGroup.
    TEST_METHOD (AGroupEndsAtLongestGroup)
    {
        constexpr size_t   kStateBytes = 16 * 1024;
        constexpr size_t   kCount      = 40;
        constexpr uint64_t kInterval   = 1000;
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  state       = MakeNoise (kStateBytes);
        HRESULT            hr          = S_OK;
        size_t             i           = 0;



        settings.intervalCycles = kInterval;
        settings.wholeEvery     = 4;
        settings.longestGroup   = 10;
        store.Configure (settings);

        for (i = 0; i < kCount; i++)
        {
            state[i] ^= 1;

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        for (i = 0; i < kCount; i++)
        {
            Assert::AreEqual (i % 10 == 0, store.GetInfo (i).isWhole, std::format (L"keyframe {} whole flag", i).c_str());
        }
    }


    //  A smaller budget leaves a table that still holds twice the newest
    //  group, however long it grew, so the oldest can always be dropped.
    TEST_METHOD (AShrunkBudgetKeepsTheLongGroupInHalfTheTable)
    {
        constexpr size_t   kStateBytes = 64 * 1024;
        constexpr size_t   kLongGroup  = 1100;
        constexpr size_t   kAfter      = 1500;
        constexpr size_t   kSmall      = 1024 * 1024;
        constexpr uint64_t kInterval   = 1000;
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  state       = MakeNoise (kStateBytes);
        std::vector<Byte>  restored;
        HRESULT            hr          = S_OK;
        size_t             i           = 0;



        settings.intervalCycles = kInterval;
        store.Configure (settings);

        for (i = 0; i < kLongGroup; i++)
        {
            state[i % kStateBytes] ^= 1;

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        hr = store.ChangeBudget (kSmall);
        AssertSucceeded (hr, L"ChangeBudget");

        Assert::IsTrue (store.GetCapacity() >= 2 * kLongGroup, L"the table holds the long group twice over");

        for (; i < kLongGroup + kAfter; i++)
        {
            state[i % kStateBytes] ^= 1;

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add after the budget shrank");
        }

        hr = store.Restore (store.GetCount() - 1, restored);
        AssertSucceeded (hr, L"Restore");
        Assert::IsTrue (restored == state, L"the newest restores exactly");
    }


    //  The meter counts the table the budget pays for, so it is at the budget,
    //  give or take a group, when the oldest are first dropped. Before, it
    //  stopped short by the table's size, then jumped to full.
    TEST_METHOD (TheMeterReachesTheBudgetWhenTheOldestAreDropped)
    {
        constexpr size_t   kStateBytes = 32 * 1024;
        constexpr size_t   kBudget     = 4 * 1024 * 1024;
        constexpr size_t   kMostAdds   = 1000;
        constexpr uint64_t kInterval   = 1000;
        constexpr size_t   kGroupSlack = 3 * kStateBytes;
        KeyframeStore      store;
        KeyframeSettings   settings;
        std::vector<Byte>  state       = MakeNoise (kStateBytes);
        HRESULT            hr          = S_OK;
        size_t             lastUsed    = 0;
        size_t             i           = 0;



        settings.intervalCycles = kInterval;
        settings.wholeEvery     = 2;
        settings.longestGroup   = 2;
        settings.budgetBytes    = kBudget;
        store.Configure (settings);

        for (i = 0; i < kMostAdds && !store.IsFull(); i++)
        {
            lastUsed = store.GetUsedBytes();

            state[i % kStateBytes] ^= 1;

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        Assert::IsTrue (store.IsFull(), L"the store filled");
        Assert::IsTrue (kBudget - lastUsed < kGroupSlack, std::format (L"the meter read {} of {} bytes just before the oldest were dropped", lastUsed, kBudget).c_str());
    }
};
