#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Events/KeyframeStretches.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kStretchStateBytes = 256;
static constexpr uint64_t  s_kStretchInterval   = 1000;
static constexpr uint64_t  s_kStretchStride     = 10;
static constexpr size_t    s_kStretchGapEvery   = 7;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretchesTests
//
//  The stretch list and its runs follow the keyframe store at both ends: a
//  sync makes a stretch only for a keyframe added since the last one, and
//  after every kind of change the lists match a rebuild from the whole
//  store.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeStretchesTests)
{
public:

    //  Hundreds of thousands of keyframes, synced after each, cost one
    //  stretch apiece: a rebuild on each sync would make the sum of the
    //  counts instead. A second sync with nothing new doesn't make any.
    TEST_METHOD (SyncMakesOnlyTheNewStretches)
    {
        constexpr size_t   kCount    = 200000;
        KeyframeStore      store;
        KeyframeStretches  stretches;
        HRESULT            hr        = S_OK;
        size_t             i         = 0;
        size_t             made      = 0;



        Configure (store);

        for (i = 0; i < kCount; i++)
        {
            hr = store.Add (i * s_kStretchStride, i * s_kStretchInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");

            stretches.Sync (&store);
        }

        Assert::IsTrue (store.GetCount() > 1000, L"the store has many keyframes, so a walk over them would be measurable");
        Assert::IsTrue (stretches.GetMadeCount() < kCount + kCount / 100, std::format (L"made {} stretches for {} keyframes", stretches.GetMadeCount(), kCount).c_str());

        made = stretches.GetMadeCount();
        stretches.Sync (&store);
        Assert::AreEqual (made, stretches.GetMadeCount(), L"a sync with nothing new doesn't make a stretch");

        AssertMatchesRebuild (store, stretches, L"after the adds");
    }


    //  Adds with gaps, a budget that drops the oldest, a trim by cycle, a
    //  trim by position and a clear that starts over: the lists match a
    //  rebuild after each.
    TEST_METHOD (SyncMatchesARebuildAfterEveryKindOfChange)
    {
        constexpr size_t   kCount    = 300;
        constexpr size_t   kSmallKb  = 96;
        KeyframeStore      store;
        KeyframeStretches  stretches;
        HRESULT            hr        = S_OK;
        size_t             i         = 0;
        uint64_t           next      = 0;



        Configure (store);

        for (i = 0; i < kCount; i++, next++)
        {
            hr = store.Add (next * s_kStretchStride, next * s_kStretchInterval, MakeState (next));
            AssertSucceeded (hr, L"Add");

            if (i % s_kStretchGapEvery == 0 && i > 0)
            {
                store.MarkNewestGap (next * s_kStretchStride - s_kStretchStride / 2);
            }

            stretches.Sync (&store);
            AssertMatchesRebuild (store, stretches, L"after an add");
        }

        hr = store.ChangeBudget (kSmallKb * 1024);
        AssertSucceeded (hr, L"ChangeBudget");
        stretches.Sync (&store);
        AssertMatchesRebuild (store, stretches, L"after the budget dropped the oldest");

        hr = store.TruncateAfter (store.GetInfo (store.GetCount() - 5).cycle);
        AssertSucceeded (hr, L"TruncateAfter");
        stretches.Sync (&store);
        AssertMatchesRebuild (store, stretches, L"after a trim by cycle");

        hr = store.DropAfterPosition (store.GetInfo (store.GetCount() - 3).position);
        AssertSucceeded (hr, L"DropAfterPosition");
        stretches.Sync (&store);
        AssertMatchesRebuild (store, stretches, L"after a trim by position");

        //  A trim followed by adds before the next sync: the newest stretches
        //  change in place.
        hr = store.DropAfterPosition (store.GetInfo (store.GetCount() - 2).position);
        AssertSucceeded (hr, L"DropAfterPosition");

        for (i = 0; i < 4; i++, next++)
        {
            hr = store.Add (next * s_kStretchStride + 3, next * s_kStretchInterval, MakeState (next));
            AssertSucceeded (hr, L"Add");
        }

        stretches.Sync (&store);
        AssertMatchesRebuild (store, stretches, L"after a trim and adds");

        store.Clear();
        stretches.Sync (&store);
        Assert::IsTrue (stretches.GetStretches().empty() && stretches.GetRuns().empty(), L"a cleared store has no stretches");

        for (i = 0; i < 20; i++, next++)
        {
            hr = store.Add (next * s_kStretchStride, next * s_kStretchInterval, MakeState (next));
            AssertSucceeded (hr, L"Add");
        }

        stretches.Sync (&store);
        AssertMatchesRebuild (store, stretches, L"after a clear and a new start");
    }


    TEST_METHOD (FindFirstEndingAfterIsTheFirstStretchPastAPosition)
    {
        KeyframeStore      store;
        KeyframeStretches  stretches;
        HRESULT            hr        = S_OK;



        Configure (store);

        for (uint64_t i = 0; i < 5; i++)
        {
            hr = store.Add (i * s_kStretchStride, i * s_kStretchInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }

        stretches.Sync (&store);

        Assert::AreEqual ((size_t) 0, stretches.FindFirstEndingAfter (0),                    L"before the first end");
        Assert::AreEqual ((size_t) 1, stretches.FindFirstEndingAfter (s_kStretchStride),     L"at the first end");
        Assert::AreEqual ((size_t) 1, stretches.FindFirstEndingAfter (s_kStretchStride + 1), L"inside the second");
        Assert::AreEqual ((size_t) 4, stretches.FindFirstEndingAfter (4 * s_kStretchStride), L"past the last");
    }

private:

    static void Configure (KeyframeStore & store)
    {
        KeyframeSettings  settings;



        settings.intervalCycles = s_kStretchInterval;
        store.Configure (settings);
    }


    static std::vector<Byte> MakeState (uint64_t seed)
    {
        std::vector<Byte>  state (s_kStretchStateBytes, 0);



        state[seed % s_kStretchStateBytes] = (Byte) (seed + 1);
        return state;
    }


    //  Every stretch made from the store, and the runs joined from them where
    //  no gap lies between.
    static void AssertMatchesRebuild (const KeyframeStore & store, const KeyframeStretches & stretches, LPCWSTR when)
    {
        std::vector<DebugEventBounds>  rebuilt;
        std::vector<DebugEventBounds>  runs;
        size_t                         i        = 0;



        for (i = 0; i + 1 < store.GetCount(); i++)
        {
            rebuilt.push_back (KeyframeStretches::MakeStretch (store, i));
        }

        for (const DebugEventBounds & each : rebuilt)
        {
            if (!runs.empty() && !runs.back().hasGapAfter)
            {
                runs.back().endPosition = each.endPosition;
                runs.back().hasGapAfter = each.hasGapAfter;
            }
            else
            {
                runs.push_back (each);
            }
        }

        Assert::IsTrue (std::ranges::equal (rebuilt, stretches.GetStretches()), std::format (L"stretches {}", when).c_str());
        Assert::IsTrue (std::ranges::equal (runs,    stretches.GetRuns()),      std::format (L"runs {}", when).c_str());
    }
};
