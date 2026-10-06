#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/HistoryWallTimes.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kWallInterval   = 1000;
static constexpr uint64_t  s_kWallStride     = 100;
static constexpr uint64_t  s_kWallTick       = 9000;         // 100 ns ticks between keyframes, a little under the cycles' time
static constexpr size_t    s_kWallNoiseBytes = 4096;
static constexpr uint64_t  s_kWallProbeStep  = 250;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimesTests
//
//  The host's clock at any cycle, kept beside the keyframe store for the UI
//  thread: it must give what the store itself gives -- the newest keyframe
//  at or before the cycle, counted on by the cycles since -- as keyframes
//  are added, the oldest dropped, the newest cut off, and history begun
//  again.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryWallTimesTests)
{
public:

    TEST_METHOD (TheClockAtACycleMatchesTheStoreAsHistoryChanges)
    {
        constexpr uint32_t  kWholeEvery   = 2;
        constexpr size_t    kBudgetStates = 24;
        constexpr size_t    kFirstAdds    = 10;
        constexpr size_t    kMoreAdds     = 60;
        constexpr uint64_t  kCutAfter     = 5500;
        constexpr uint64_t  kAgainFrom    = 90000;

        KeyframeStore     store;
        KeyframeSettings  settings;
        HistoryWallTimes  wallTimes;
        size_t            added = 0;
        HRESULT           hr    = S_OK;



        settings.intervalCycles = s_kWallInterval;
        settings.wholeEvery     = kWholeEvery;
        settings.budgetBytes    = s_kWallNoiseBytes * kBudgetStates;

        store.Configure    (settings);
        store.SetWallClock ([] () -> uint64_t { s_wall += s_kWallTick; return s_wall; });

        Assert::AreEqual<uint64_t> (0, wallTimes.GetWallTimeAt (500), L"nothing held, no time");

        Add (store, added, kFirstAdds);
        wallTimes.Sync (store);
        Check (store, wallTimes, L"first keyframes");

        //  An hour passes with the machine stopped: a step in the clock that
        //  only the keyframes after it know of.
        s_wall += kHourTicks;

        Add (store, added, kMoreAdds);
        wallTimes.Sync (store);

        Assert::IsTrue (store.GetInfo (0).position > 0, L"the budget dropped the oldest keyframes");
        Check (store, wallTimes, L"after the oldest were dropped");

        hr = store.TruncateAfter (store.GetInfo (store.GetCount() / 2).cycle + s_kWallInterval / 2);
        AssertSucceeded (hr, L"TruncateAfter");

        wallTimes.Sync (store);
        Check (store, wallTimes, L"after the newest were cut off");

        store.Clear();
        added = kAgainFrom / s_kWallInterval;

        Add (store, added, kFirstAdds);
        wallTimes.Sync (store);
        Check (store, wallTimes, L"history begun again");

        wallTimes.Clear();
        Assert::AreEqual<size_t> (0, wallTimes.GetCount(), L"cleared");
    }

private:

    static constexpr uint64_t  kHourTicks = 36000000000ull;

    static inline uint64_t  s_wall = 0x01DC000000000000ull;


    //  Noise, so the snapshots do not pack and the budget binds.
    static void Add (KeyframeStore & store, size_t & added, size_t count)
    {
        constexpr uint32_t  kMul = 1664525u;
        constexpr uint32_t  kAdd = 1013904223u;
        constexpr int       kTop = 24;

        std::vector<Byte>  state (s_kWallNoiseBytes);
        uint32_t           seed  = (uint32_t) added + 1;
        HRESULT            hr    = S_OK;
        size_t             i     = 0;



        for (i = 0; i < count; i++, added++)
        {
            for (Byte & b : state)
            {
                seed = seed * kMul + kAdd;
                b    = (Byte) (seed >> kTop);
            }

            hr = store.Add (added * s_kWallStride, added * s_kWallInterval, state);
            AssertSucceeded (hr, L"Add");
        }
    }


    //  Every keyframe held, and the time at cycles across them and past the
    //  newest, as the store gives it.
    static void Check (const KeyframeStore & store, const HistoryWallTimes & wallTimes, PCWSTR what)
    {
        uint64_t  first    = store.GetInfo (0).cycle;
        uint64_t  last     = store.GetInfo (store.GetCount() - 1).cycle + s_kWallInterval;
        uint64_t  cycle    = 0;
        uint64_t  expected = 0;
        size_t    index    = 0;
        bool      isHeld   = false;



        Assert::AreEqual (store.GetCount(), wallTimes.GetCount(), what);

        for (cycle = (first > s_kWallProbeStep) ? first - s_kWallProbeStep : 0; cycle <= last; cycle += s_kWallProbeStep)
        {
            isHeld   = store.TryFindAtOrBefore (cycle, index);
            expected = isHeld ? HistoryStatus::GetWallTimeAt (store.GetInfo (index).wallTime, store.GetInfo (index).cycle, cycle) : 0;

            Assert::AreEqual (expected, wallTimes.GetWallTimeAt (cycle), what);
        }
    }
};
