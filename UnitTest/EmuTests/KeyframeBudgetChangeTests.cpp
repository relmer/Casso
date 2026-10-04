#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/KeyframeStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kBudgetStateBytes = 32 * 1024;
static constexpr uint64_t  s_kBudgetInterval   = 1000;
static constexpr size_t    s_kBudgetMb         = 1024 * 1024;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeBudgetChangeTests
//
//  A new memory budget applies to a store that is already recording: a
//  smaller one drops the oldest groups until the history fits, a larger one
//  grows the store at once, and every keyframe kept still restores exactly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeBudgetChangeTests)
{
public:

    TEST_METHOD (ASmallerBudgetDropsTheOldestAtOnce)
    {
        constexpr size_t   kCount   = 200;
        KeyframeStore      store;
        size_t             before   = 0;
        size_t             reserved = 0;
        HRESULT            hr       = S_OK;



        Fill (store, 16 * s_kBudgetMb, 0, kCount);

        before   = store.GetCount();
        reserved = store.GetReservedBytes();
        Assert::AreEqual (kCount, before, L"16 MB holds every keyframe");

        hr = store.ChangeBudget (2 * s_kBudgetMb);
        AssertSucceeded (hr, L"ChangeBudget");

        Assert::IsTrue (store.GetByteCount() <= 2 * s_kBudgetMb, L"the history must fit the smaller budget at once");
        Assert::IsTrue (store.GetCount() < before,               L"the oldest keyframes are dropped");
        Assert::IsTrue (store.GetCount() > 0,                    L"the newest group stays");
        Assert::AreEqual<uint64_t> ((kCount - 1) * s_kBudgetInterval, store.GetInfo (store.GetCount() - 1).cycle, L"the newest keyframe stays");
        Assert::IsTrue (store.GetReservedBytes() < reserved,     L"the memory shrinks with the budget");

        AssertEveryKeyframeRestores (store);

        Fill (store, 0, kCount, 50);
        Assert::IsTrue (store.GetByteCount() <= 2 * s_kBudgetMb, L"recording goes on within the new budget");
        AssertEveryKeyframeRestores (store);
    }


    TEST_METHOD (ALargerBudgetGrowsTheStoreAtOnce)
    {
        constexpr size_t  kCount   = 200;
        KeyframeStore     store;
        size_t            held     = 0;
        size_t            reserved = 0;
        HRESULT           hr       = S_OK;



        Fill (store, 2 * s_kBudgetMb, 0, kCount);

        held     = store.GetCount();
        reserved = store.GetReservedBytes();
        Assert::IsTrue (held < kCount, L"2 MB must have dropped some keyframes");

        hr = store.ChangeBudget (16 * s_kBudgetMb);
        AssertSucceeded (hr, L"ChangeBudget");

        Assert::IsTrue  (store.GetReservedBytes() > reserved, L"the memory grows as soon as the budget does");
        Assert::AreEqual (held, store.GetCount(),             L"growing keeps every keyframe");
        AssertEveryKeyframeRestores (store);

        Fill (store, 0, kCount, kCount);
        Assert::AreEqual (held + kCount, store.GetCount(), L"nothing is dropped while the larger budget has room");
        AssertEveryKeyframeRestores (store);
    }


    TEST_METHOD (ABudgetBeforeTheFirstKeyframeIsKept)
    {
        KeyframeStore     store;
        KeyframeSettings  settings;
        HRESULT           hr       = S_OK;



        settings.intervalCycles = s_kBudgetInterval;
        store.Configure (settings);

        hr = store.ChangeBudget (3 * s_kBudgetMb);
        AssertSucceeded (hr, L"ChangeBudget");

        Assert::AreEqual (3 * s_kBudgetMb, store.GetSettings().budgetBytes);
        Assert::AreEqual (size_t (0),      store.GetReservedBytes(), L"nothing is reserved before the first keyframe");
    }


private:

    //  Adds count keyframes from first on; a budget of zero keeps the
    //  store's settings.
    static void Fill (KeyframeStore & store, size_t budgetBytes, size_t first, size_t count)
    {
        KeyframeSettings  settings;
        HRESULT           hr = S_OK;



        if (budgetBytes != 0)
        {
            settings.intervalCycles = s_kBudgetInterval;
            settings.wholeEvery     = 4;
            settings.budgetBytes    = budgetBytes;
            store.Configure (settings);
        }

        for (size_t i = first; i < first + count; i++)
        {
            hr = store.Add (i * 10, i * s_kBudgetInterval, MakeState (i));
            AssertSucceeded (hr, L"Add");
        }
    }


    static void AssertEveryKeyframeRestores (KeyframeStore & store)
    {
        std::vector<Byte>  restored;
        HRESULT            hr = S_OK;



        for (size_t i = 0; i < store.GetCount(); i++)
        {
            size_t  step = static_cast<size_t> (store.GetInfo (i).cycle / s_kBudgetInterval);

            hr = store.Restore (i, restored);
            AssertSucceeded (hr, L"Restore");
            Assert::IsTrue (restored == MakeState (step), std::format (L"keyframe {} must restore exactly", step).c_str());
        }
    }


    //  A state that packs poorly, so the budget fills in a few hundred
    //  keyframes, and that differs from every other step's.
    static std::vector<Byte> MakeState (size_t step)
    {
        std::vector<Byte>  state (s_kBudgetStateBytes);
        uint32_t           x     = static_cast<uint32_t> (step * 2654435761u + 1);



        for (Byte & b : state)
        {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            b  = static_cast<Byte> (x);
        }

        return state;
    }
};
