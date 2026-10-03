#include "Pch.h"

#include "HResultAssert.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "EmuTests/TestMachine.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReverseController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr size_t    s_kQueueItems       = 64;
static constexpr size_t    s_kQueueRounds      = 200;
static constexpr size_t    s_kHandoffState     = 4096;
static constexpr size_t    s_kHandoffSmall     = 3000;
static constexpr size_t    s_kStressKeyframes  = 600;
static constexpr size_t    s_kStressState      = 65536;
static constexpr size_t    s_kStressBudget     = 300000;
static constexpr size_t    s_kStressResize     = 97;
static constexpr size_t    s_kStressRestore    = 13;
static constexpr size_t    s_kStressTruncate   = 151;
static constexpr uint64_t  s_kStressInterval   = 100;
static constexpr uint64_t  s_kSeekFrames       = 12;
static constexpr uint64_t  s_kSeekBackFrames   = 5;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeWorkQueueTests
//
//  Keyframes are packed off the thread that runs the machine. The thread
//  pool queue runs its items in order, one at a time, off the caller; the
//  keyframe store lists a keyframe at once and collects its packing later,
//  waits rather than allocate when every buffer is in flight, and waits for
//  the work in flight before anything reads what it produces.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyframeWorkQueueTests)
{
public:

    TEST_METHOD (ThePoolRunsItemsInOrderOneAtATimeOffTheCaller)
    {
        ThreadPoolWorkQueue  queue;
        QueueProbe           probe;
        size_t               round = 0;
        size_t               i     = 0;
        HRESULT              hr    = S_OK;



        hr = queue.Create (s_kQueueItems);
        AssertSucceeded (hr, L"Create");

        probe.caller = GetCurrentThreadId();

        for (round = 0; round < s_kQueueRounds; round++)
        {
            probe.order.clear();
            probe.order.reserve (s_kQueueItems);

            for (i = 0; i < s_kQueueItems; i++)
            {
                hr = queue.Submit (QueueProbe::Run, &probe);
                AssertSucceeded (hr, L"Submit");
            }

            queue.WaitAll();

            Assert::AreEqual (s_kQueueItems, probe.order.size(), L"every item ran before WaitAll returned");

            for (i = 0; i < s_kQueueItems; i++)
            {
                Assert::AreEqual (round * s_kQueueItems + i, probe.order[i], L"in the order submitted");
            }
        }

        Assert::IsFalse (probe.hasOverlapped.load(), L"no two items ran at once");
        Assert::IsFalse (probe.ranOnCaller.load(),   L"no item ran on the caller's thread");
    }


    TEST_METHOD (AKeyframeInFlightIsListedAndCollectedLater)
    {
        InlineWorkQueue    queue;
        KeyframeStore      store;
        std::vector<Byte>  state    = MakeState (s_kHandoffState, 1);
        uint64_t           checksum = KeyframeStore::ComputeChecksum (state.data(), state.size());
        HRESULT            hr       = S_OK;



        store.Configure (KeyframeSettings());
        store.SetWorkQueue (&queue);

        hr = store.Add (1, 100, 7, state);
        AssertSucceeded (hr, L"Add");

        Assert::AreEqual<size_t> (1, store.GetCount(), L"the keyframe is listed at once");
        Assert::AreEqual<size_t> (1, store.GetPendingCount(), L"with its packing in flight");
        Assert::AreEqual<size_t> (1, queue.GetPendingCount(), L"handed to the queue");
        Assert::AreEqual<uint64_t> (100, store.GetInfo (0).cycle);
        Assert::AreEqual<size_t> (7, store.GetInfo (0).journalIndex);
        Assert::IsTrue (store.GetInfo (0).isWhole);
        Assert::IsTrue (store.GetNextDueCycle() > 100, L"the next one is scheduled at once");

        Assert::IsTrue (queue.TryRunNext());

        hr = store.WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        Assert::AreEqual<size_t> (0, store.GetPendingCount(), L"collected");
        Assert::AreEqual<uint64_t> (checksum, store.GetInfo (0).checksum, L"with its checksum");
        Assert::IsTrue (store.GetInfo (0).storedBytes > 0, L"and its packed bytes");
    }


    TEST_METHOD (AFullPoolWaitsForItsBuffersRatherThanAllocate)
    {
        InlineWorkQueue  queue;
        KeyframeStore    store;
        size_t           i     = 0;
        HRESULT          hr    = S_OK;



        store.Configure (KeyframeSettings());
        store.SetWorkQueue (&queue);

        for (i = 0; i < KeyframeStore::kBufferCount; i++)
        {
            hr = store.Add (i, (i + 1) * 100, MakeState (s_kHandoffState, i));
            AssertSucceeded (hr, L"Add");
        }

        Assert::AreEqual<size_t> (0, queue.GetWaitCount(), L"every buffer is in flight and nothing waited yet");
        Assert::AreEqual (KeyframeStore::kBufferCount, store.GetPendingCount());

        hr = store.Add (i, (i + 1) * 100, MakeState (s_kHandoffState, i));
        AssertSucceeded (hr, L"Add");

        Assert::AreEqual<size_t> (1, queue.GetWaitCount(), L"the next keyframe waited for the buffers in flight");
        Assert::AreEqual<size_t> (1, store.GetPendingCount(), L"and is in flight in a buffer they gave back");
        Assert::AreEqual (KeyframeStore::kBufferCount + 1, store.GetCount());
    }


    TEST_METHOD (EveryReaderWaitsForTheKeyframesInFlight)
    {
        InlineWorkQueue    queue;
        KeyframeStore      store;
        std::vector<Byte>  first    = MakeState (s_kHandoffState, 1);
        std::vector<Byte>  second   = MakeState (s_kHandoffState, 2);
        std::vector<Byte>  restored;
        HRESULT            hr       = S_OK;



        store.Configure (KeyframeSettings());
        store.SetWorkQueue (&queue);

        AddPending (store, 1, 100, first);
        AddPending (store, 2, 200, second);

        hr = store.Restore (1, restored);
        AssertSucceeded (hr, L"Restore");

        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"Restore ran the work in flight");
        Assert::IsTrue (restored == second, L"and gave the state back");

        AddPending (store, 3, 300, MakeState (s_kHandoffState, 3));

        Assert::IsTrue (store.DoesStateMatch (2, MakeState (s_kHandoffState, 3)), L"DoesStateMatch waited for the checksum");

        AddPending (store, 4, 400, MakeState (s_kHandoffState, 4));

        hr = store.TruncateAfter (300);
        AssertSucceeded (hr, L"TruncateAfter");

        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"TruncateAfter ran the work in flight");
        Assert::AreEqual<size_t> (3, store.GetCount());

        hr = store.Restore (2, restored);
        AssertSucceeded (hr, L"Restore");

        Assert::IsTrue (restored == MakeState (s_kHandoffState, 3), L"the newest kept keyframe restores after the truncation");

        AddPending (store, 5, 500, MakeState (s_kHandoffSmall, 5));

        store.Clear();

        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"Clear ran the work in flight before dropping it");
        Assert::AreEqual<size_t> (0, store.GetCount());
    }


    TEST_METHOD (PackingOnThePoolMatchesPackingInPlace)
    {
        ThreadPoolWorkQueue  queue;
        KeyframeStore        pooled;
        KeyframeStore        inPlace;
        KeyframeSettings     settings;
        std::vector<Byte>    state     (s_kStressState, 0);
        std::vector<Byte>    fromPool;
        std::vector<Byte>    fromPlace;
        size_t               i         = 0;
        size_t               index     = 0;
        size_t               skipped   = 0;
        size_t               other     = 0;
        uint64_t             cycle     = 0;
        HRESULT              hr        = S_OK;



        settings.intervalCycles = s_kStressInterval;
        settings.wholeEvery     = 8;
        settings.budgetBytes    = s_kStressBudget;

        hr = queue.Create (KeyframeStore::kBufferCount);
        AssertSucceeded (hr, L"Create");

        pooled.Configure  (settings);
        inPlace.Configure (settings);
        pooled.SetWorkQueue (&queue);

        for (i = 1; i <= s_kStressKeyframes; i++)
        {
            cycle = i * s_kStressInterval;

            Mutate (state, i);

            hr = pooled.Add (i, cycle, i, state);
            AssertSucceeded (hr, L"Add, pooled");

            hr = inPlace.Add (i, cycle, i, state);
            AssertSucceeded (hr, L"Add, in place");

            if (i % s_kStressRestore == 0)
            {
                index = inPlace.GetCount() - 1 - (i % inPlace.GetCount());

                Assert::IsTrue (pooled.TryFindAtOrBefore (inPlace.GetInfo (index).cycle, other), L"the pooled store holds the keyframe");

                AssertSucceeded (pooled.Restore (other, fromPool),   L"Restore, pooled");
                AssertSucceeded (inPlace.Restore (index, fromPlace), L"Restore, in place");

                Assert::AreEqual<uint64_t> (inPlace.GetInfo (index).cycle, pooled.GetInfo (other).cycle);
                Assert::IsTrue (fromPool == fromPlace, L"a restore mid-run gives the same state");
                Assert::IsTrue (pooled.DoesStateMatch (pooled.GetCount() - 1, state), L"the newest keyframe's checksum is the state's");
            }

            if (i % s_kStressTruncate == 0)
            {
                AssertSucceeded (pooled.TruncateAfter (cycle - 3 * s_kStressInterval),  L"TruncateAfter, pooled");
                AssertSucceeded (inPlace.TruncateAfter (cycle - 3 * s_kStressInterval), L"TruncateAfter, in place");
            }
        }

        hr = pooled.WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        // The pooled store enforces its budget on keyframes it has collected,
        // so it may still hold a group the other has dropped; the newest
        // keyframes of both must be the same ones.
        skipped = pooled.GetCount() - inPlace.GetCount();

        Assert::IsTrue (pooled.GetCount() >= inPlace.GetCount(), L"the pooled store drops no more than the other");
        Assert::IsTrue (inPlace.GetCount() > settings.wholeEvery, L"more than one group was kept to compare");

        for (index = 0; index < inPlace.GetCount(); index++)
        {
            Assert::AreEqual<uint64_t> (inPlace.GetInfo (index).cycle,    pooled.GetInfo (skipped + index).cycle);
            Assert::AreEqual<uint64_t> (inPlace.GetInfo (index).checksum, pooled.GetInfo (skipped + index).checksum);
            Assert::AreEqual (inPlace.GetInfo (index).isWhole, pooled.GetInfo (skipped + index).isWhole);

            AssertSucceeded (pooled.Restore (skipped + index, fromPool), L"Restore, pooled");
            AssertSucceeded (inPlace.Restore (index, fromPlace),         L"Restore, in place");

            Assert::IsTrue (fromPool == fromPlace, L"every keyframe kept restores to the same state");
        }
    }


    TEST_METHOD (ASeekWaitsForTheKeyframesInFlight)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        InlineWorkQueue    queue;
        ReverseResult      result;
        uint64_t           target     = 0;
        uint64_t           expected   = 0;
        HRESULT            hr         = S_OK;



        ReverseSessionRig::Prepare (machine);

        controller.SetWorkQueue (&queue);

        hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (KeyframeSettings::kFrameCycles * (s_kSeekFrames - s_kSeekBackFrames));

        target   = machine.GetPosition();
        expected = ReverseSessionRig::Checksum (machine);

        machine.RunCycles (KeyframeSettings::kFrameCycles * s_kSeekBackFrames);

        Assert::IsTrue (queue.GetPendingCount() > 0, L"keyframes are still in flight");

        hr = controller.SeekToPosition (target, result);
        AssertSucceeded (hr, L"SeekToPosition");

        Assert::AreEqual<size_t> (0, queue.GetPendingCount(), L"the seek ran them first");
        Assert::AreEqual<size_t> (0, controller.GetKeyframes().GetPendingCount());
        Assert::AreEqual<uint64_t> (target, result.position);
        Assert::AreEqual<uint64_t> (expected, ReverseSessionRig::Checksum (machine), L"the machine is as it was there");
    }

private:

    //  Counts the items it runs and notices any that overlap or run on the
    //  caller's thread.
    struct QueueProbe
    {
        DWORD                caller        = 0;
        std::vector<size_t>  order;
        size_t               next          = 0;
        std::atomic<bool>    isRunning     = false;
        std::atomic<bool>    hasOverlapped = false;
        std::atomic<bool>    ranOnCaller   = false;


        static void Run (void * context)
        {
            QueueProbe  * probe = static_cast<QueueProbe *> (context);



            if (probe->isRunning.exchange (true))
            {
                probe->hasOverlapped = true;
            }

            if (GetCurrentThreadId() == probe->caller)
            {
                probe->ranOnCaller = true;
            }

            // Long enough that another pool thread would get in if it could.
            Sleep (0);

            probe->order.push_back (probe->next++);
            probe->isRunning = false;
        }
    };


    static std::vector<Byte> MakeState (size_t size, size_t seed)
    {
        std::vector<Byte>  state (size, 0);
        size_t             i     = 0;



        for (i = 0; i < size; i++)
        {
            state[i] = static_cast<Byte> ((i * 31 + seed * 17) >> 3);
        }

        return state;
    }


    static void AddPending (KeyframeStore & store, uint64_t position, uint64_t cycle, const std::vector<Byte> & state)
    {
        HRESULT  hr = S_OK;



        hr = store.Add (position, cycle, state);
        AssertSucceeded (hr, L"Add");

        Assert::IsTrue (store.GetPendingCount() > 0, L"left in flight");
    }


    //  A few bytes change every time, and now and then the size, which
    //  starts a new group.
    static void Mutate (std::vector<Byte> & state, size_t step)
    {
        size_t  i = 0;



        if (step % s_kStressResize == 0)
        {
            state.resize ((state.size() == s_kStressState) ? s_kStressState / 2 : s_kStressState, static_cast<Byte> (step));
        }

        for (i = 0; i < 16; i++)
        {
            state[(step * 1031 + i * 4099) % state.size()] ^= static_cast<Byte> (step + i);
        }
    }
};
