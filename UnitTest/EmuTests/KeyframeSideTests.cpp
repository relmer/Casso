#include "Pch.h"

#include "HResultAssert.h"
#include "EmuTests/InlineWorkQueue.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kSideStateBytes = 64 * 1024;
static constexpr uint64_t  s_kSideInterval   = 1000;
static constexpr size_t    s_kSideBytes      = 5000;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeSideTests
//
//  The bytes a keyframe can be given to keep beside its snapshot, which the
//  heat map uses for the counts made since the keyframe before: they come
//  back while the keyframe is still being packed and after, they count
//  against the budget, they move with the store when its budget changes,
//  and the drop listener can read them before their keyframe goes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeSideTests)
{
public:

    TEST_METHOD (SideBytesComeBackInFlightAndAfter)
    {
        KeyframeStore      store;
        InlineWorkQueue    queue;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr       = S_OK;



        settings.intervalCycles = s_kSideInterval;
        store.Configure    (settings);
        store.SetWorkQueue (&queue);

        hr = store.Add (10, s_kSideInterval, 0, MakeState (1), MakeSide (1));
        AssertSucceeded (hr, L"Add");

        Assert::AreEqual ((size_t) 1, store.GetPendingCount(), L"still being packed");
        Assert::IsTrue   (ReadSide (store, 0) == MakeSide (1), L"in flight, the side bytes come from the job");

        queue.WaitAll();

        hr = store.WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        Assert::IsTrue (ReadSide (store, 0) == MakeSide (1), L"collected, from the arena");

        hr = store.Restore (0, restored);
        AssertSucceeded (hr, L"Restore");
        Assert::IsTrue (restored == MakeState (1), L"the snapshot beside them restores exactly");
    }


    TEST_METHOD (SideBytesCountAgainstTheBudget)
    {
        constexpr size_t   kCount   = 12;
        KeyframeStore      bare;
        KeyframeStore      sided;
        KeyframeSettings   settings;
        std::vector<Byte>  restored;
        HRESULT            hr       = S_OK;
        size_t             i        = 0;



        settings.intervalCycles = s_kSideInterval;
        bare.Configure  (settings);
        sided.Configure (settings);

        for (i = 0; i < kCount; i++)
        {
            hr = bare.Add (i, (i + 1) * s_kSideInterval, 0, MakeState (i));
            AssertSucceeded (hr, L"Add bare");

            hr = sided.Add (i, (i + 1) * s_kSideInterval, 0, MakeState (i), MakeSide (i));
            AssertSucceeded (hr, L"Add with side bytes");
        }

        Assert::AreEqual (kCount * s_kSideBytes, sided.GetSideByteCount(), L"the side bytes held");
        Assert::AreEqual (bare.GetByteCount() + kCount * s_kSideBytes, sided.GetByteCount(), L"are held against the budget");
        Assert::IsTrue   (sided.GetUsedBytes() > bare.GetUsedBytes(), L"and fill the meter");

        for (i = 0; i < kCount; i++)
        {
            hr = sided.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");

            Assert::IsTrue (restored == MakeState (i), std::format (L"keyframe {} restores exactly beside its side bytes", i).c_str());
            Assert::IsTrue (ReadSide (sided, i) == MakeSide (i), std::format (L"keyframe {} keeps its own side bytes", i).c_str());
        }
    }


    TEST_METHOD (SideBytesMoveWithANewBudget)
    {
        constexpr size_t   kCount   = 8;
        KeyframeStore      store;
        KeyframeSettings   settings;
        HRESULT            hr       = S_OK;
        size_t             i        = 0;



        settings.intervalCycles = s_kSideInterval;
        store.Configure (settings);

        for (i = 0; i < kCount; i++)
        {
            hr = store.Add (i, (i + 1) * s_kSideInterval, 0, MakeState (i), MakeSide (i));
            AssertSucceeded (hr, L"Add");
        }

        hr = store.ChangeBudget (settings.budgetBytes * 2);
        AssertSucceeded (hr, L"ChangeBudget");

        for (i = 0; i < kCount; i++)
        {
            Assert::IsTrue (ReadSide (store, i) == MakeSide (i), std::format (L"keyframe {}'s side bytes moved with it", i).c_str());
        }
    }


    TEST_METHOD (TheDropListenerReadsEachKeyframeBeforeItGoes)
    {
        constexpr size_t                 kCount  = 40;
        KeyframeStore                    store;
        KeyframeSettings                 settings;
        std::vector<KeyframeDrop>        drops;
        std::vector<std::vector<Byte>>   sides;
        size_t                           newest  = 0;
        HRESULT                          hr      = S_OK;
        size_t                           i       = 0;



        settings.intervalCycles = s_kSideInterval;
        settings.wholeEvery     = 4;
        settings.longestGroup   = 4;
        settings.budgetBytes    = 512 * 1024;
        store.Configure (settings);

        store.SetDropListener ([&] (KeyframeDrop drop)
        {
            size_t  index = (drop == KeyframeDrop::Newest) ? store.GetCount() - 1 : 0;



            drops.push_back (drop);
            sides.push_back ((drop == KeyframeDrop::All) ? std::vector<Byte>() : ReadSide (store, index));
        });

        for (i = 0; i < kCount; i++)
        {
            hr = store.Add (i * 10, (i + 1) * s_kSideInterval, 0, MakeNoisyState (i), MakeSide (i));
            AssertSucceeded (hr, L"Add");
        }

        Assert::IsTrue (!drops.empty() && drops.front() == KeyframeDrop::Oldest, L"the budget dropped the oldest");
        Assert::IsTrue (sides.front() == MakeSide (0), L"with the oldest's side bytes still there to read");

        drops.clear();
        sides.clear();

        newest = store.GetCount() - 1;

        hr = store.DropAfterPosition (store.GetInfo (newest - 2).position);
        AssertSucceeded (hr, L"DropAfterPosition");

        Assert::AreEqual ((size_t) 2, drops.size(), L"one notice for each of the two newest");
        Assert::IsTrue   (drops[0] == KeyframeDrop::Newest && drops[1] == KeyframeDrop::Newest);
        Assert::IsTrue   (sides[0] == MakeSide (kCount - 1) && sides[1] == MakeSide (kCount - 2), L"newest first, each readable");

        drops.clear();

        store.Clear();

        Assert::AreEqual ((size_t) 1, drops.size());
        Assert::IsTrue   (drops[0] == KeyframeDrop::All, L"a clear drops everything at once");
    }


private:

    static std::vector<Byte> ReadSide (const KeyframeStore & store, size_t index)
    {
        const Byte  * data = nullptr;
        size_t        size = 0;



        store.GetSide (index, data, size);

        return (data != nullptr) ? std::vector<Byte> (data, data + size) : std::vector<Byte>();
    }


    static std::vector<Byte> MakeSide (size_t step)
    {
        std::vector<Byte>  side (s_kSideBytes);



        for (size_t i = 0; i < side.size(); i++)
        {
            side[i] = static_cast<Byte> (i * 7 + step * 13);
        }

        return side;
    }


    static std::vector<Byte> MakeState (size_t step)
    {
        std::vector<Byte>  state (s_kSideStateBytes);



        for (size_t i = 0; i < state.size(); i++)
        {
            state[i] = static_cast<Byte> (i / 64);
        }

        state[(step * 4099) % state.size()] = static_cast<Byte> (step + 1);

        return state;
    }


    static std::vector<Byte> MakeNoisyState (size_t step)
    {
        std::vector<Byte>  state (s_kSideStateBytes);
        uint32_t           x = static_cast<uint32_t> (step * 2654435761u + 1);



        for (size_t i = 0; i < state.size(); i++)
        {
            x        = x * 1664525u + 1013904223u;
            state[i] = static_cast<Byte> (x >> 24);
        }

        return state;
    }
};
