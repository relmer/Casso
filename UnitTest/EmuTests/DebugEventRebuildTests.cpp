#include "Pch.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Events/DebugEventCache.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Shell/ScratchEventReplayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventRebuildTests
{
    static constexpr int     kWatchedPage = 3;
    static constexpr size_t  kSlices      = 400;
    static constexpr size_t  kSliceCycles = 17030;
    static constexpr size_t  kCacheSize   = 1 << 20;



    //  The event the tests log: a write to page 3, where the rig's loop
    //  stores each key, paddle time, data latch byte and count.
    struct PageWrite
    {
        Word  address = 0;
        Byte  value   = 0;

        bool  operator== (const PageWrite & other) const = default;
    };

    using Entry = DebugEventEntry<PageWrite>;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LiveSink
    //
    //  The running machine's probe: each write to the watched page appended
    //  to the cache, stamped with the machine's position and cycle.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LiveSink : public IWatchSink
    {
    public:
        LiveSink (MachineHost & machine, DebugEventCache<PageWrite> & cache) : m_machine (machine), m_cache (cache) {}

        void  OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte>) override
        {
            if (access == BusAccess::Write)
            {
                m_cache.Append (m_machine.GetPosition(), m_machine.GetCpu()->GetTotalCycles(), PageWrite { address, value });
            }
        }

    private:
        MachineHost                 & m_machine;
        DebugEventCache<PageWrite>  & m_cache;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PageWriteProbe
    //
    //  The same probe on the machine replaying history, through the watch
    //  sink of the stop test the replay puts on the bus.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PageWriteProbe : public IDebugEventProbe<PageWrite>, public IReverseStopTest, public IWatchSink
    {
    public:
        HRESULT  Attach (MachineHost & machine, std::vector<Entry> & entries) override
        {
            m_machine = &machine;
            m_entries = &entries;

            machine.GetMemoryBus().SetWatchedPage (kWatchedPage, true);

            return S_OK;
        }

        void  Detach (MachineHost & machine) override
        {
            machine.GetMemoryBus().SetWatchedPage (kWatchedPage, false);

            m_machine = nullptr;
            m_entries = nullptr;
        }

        IReverseStopTest  * GetStopTest      () override                   { return this; }
        bool                ShouldStopBefore (MachineHost &, Word) override { return false; }
        bool                TakePendingStop  () override                   { return false; }
        IWatchSink        * GetWatchSink     () override                   { return this; }

        void  OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte>) override
        {
            if (access == BusAccess::Write && m_entries != nullptr)
            {
                m_entries->push_back (Entry { DebugEventEntryKind::Event, m_machine->GetPosition(), m_machine->GetCpu()->GetTotalCycles(), PageWrite { address, value } });
            }
        }

    private:
        MachineHost         * m_machine = nullptr;
        std::vector<Entry>  * m_entries = nullptr;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventRebuildTests
    //
    //  Events rebuilt from history equal the events the running machine
    //  produced live: the rig's session run with history on and the watched
    //  page's writes cached as they happened, then every stretch of history
    //  replayed on a scratch machine with the same probe.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventRebuildTests)
    {
    public:

        struct Session
        {
            TestMachine                 machine    { "Apple2e" };
            ReverseController           controller { machine };
            DebugEventCache<PageWrite>  cache      { kCacheSize };
            LiveSink                    sink       { machine, cache };

            Session()
            {
                HRESULT  hr    = S_OK;
                size_t   slice = 0;
                size_t   spent = 0;



                ReverseSessionRig::Prepare (machine);

                hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
                AssertSucceeded (hr, L"Start");

                cache.Reset (machine.GetPosition());
                machine.GetMemoryBus().SetWatchedPage (kWatchedPage, true);
                machine.GetMemoryBus().SetWatchSink   (&sink);

                for (slice = 0; slice < kSlices; slice++)
                {
                    ReverseSessionRig::Feed (machine, slice);

                    for (spent = 0; spent < kSliceCycles; )
                    {
                        spent += machine.StepOne();
                    }
                }

                machine.GetMemoryBus().SetWatchSink (nullptr);

                hr = controller.GetKeyframes().WaitForPending();
                AssertSucceeded (hr, L"WaitForPending");
            }

            HRESULT  BuildJobForAll (DebugEventHistory & history, DebugEventJob & outJob)
            {
                std::vector<DebugEventBounds>  stretches;



                history.Attach (&controller.GetKeyframes());
                history.GetStretches (stretches);

                Assert::IsTrue (stretches.size() >= 3, L"the session left several stretches of history");

                std::ranges::reverse (stretches);

                return history.BuildJob (stretches, outJob);
            }
        };


        TEST_METHOD (EveryStretchRebuildsTheEventsSeenLive)
        {
            Session                                          session;
            DebugEventHistory                                history  (session.machine);
            DebugEventJob                                    job;
            ScratchEventReplayer<PageWrite>                  replayer (std::make_unique<PageWriteProbe>());
            std::vector<DebugEventStretchResult<PageWrite>>  results;
            std::vector<Entry>                               live;
            size_t                                           events   = 0;
            HRESULT                                          hr       = S_OK;



            hr = session.BuildJobForAll (history, job);
            AssertSucceeded (hr, L"BuildJob");

            replayer.SetMachine (session.machine.GetConfig(), session.machine.GetCurrentMachineName());

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");
            Assert::AreEqual (job.stretches.size(), results.size(), L"one result a stretch");

            for (const DebugEventStretchResult<PageWrite> & result : results)
            {
                live.clear();
                session.cache.CopySpan (result.startPosition, result.endPosition, live);

                Assert::IsFalse  (result.isUnavailable, L"every keyframe was taken with the disks in the bays");
                Assert::AreEqual (live.size(), result.entries.size(), std::format (L"the stretch from {}", result.startPosition).c_str());

                for (size_t i = 0; i < live.size(); i++)
                {
                    Assert::IsTrue (live[i].position == result.entries[i].position && live[i].cycle == result.entries[i].cycle &&
                                    live[i].event == result.entries[i].event,
                                    std::format (L"event {} of the stretch from {}", i, result.startPosition).c_str());
                }

                events += live.size();
            }

            Assert::IsTrue (events > 0, L"the loop wrote the watched page");
        }


        //  Submitted, a job runs on the queue with a result for each stretch;
        //  a second submitted before the first runs replaces it.
        TEST_METHOD (ASubmittedJobReplacesOneWaitingAndHandsBackItsStretches)
        {
            Session                             session;
            DebugEventHistory                   history  (session.machine);
            DebugEventJob                       first;
            DebugEventJob                       second;
            InlineWorkQueue                     queue;
            ScratchEventReplayer<PageWrite>     replayer (std::make_unique<PageWriteProbe>());
            DebugEventStretchResult<PageWrite>  result;
            size_t                              taken    = 0;
            HRESULT                             hr       = S_OK;



            hr = session.BuildJobForAll (history, first);
            AssertSucceeded (hr, L"BuildJob");

            hr = session.BuildJobForAll (history, second);
            AssertSucceeded (hr, L"BuildJob");

            replayer.SetMachine   (session.machine.GetConfig(), session.machine.GetCurrentMachineName());
            replayer.SetWorkQueue (&queue);

            hr = replayer.Submit (std::make_shared<const DebugEventJob> (first));
            AssertSucceeded (hr, L"Submit");

            hr = replayer.Submit (std::make_shared<const DebugEventJob> (second));
            AssertSucceeded (hr, L"Submit");

            replayer.WaitForWork();

            while (replayer.TryTakeResult (result))
            {
                Assert::AreEqual (second.generation, result.generation, L"only the newest job ran");
                taken++;
            }

            Assert::AreEqual (second.stretches.size(), taken);
        }


        TEST_METHOD (AStretchRecordedWithOtherDisksIsUnavailable)
        {
            Session                                          session;
            DebugEventHistory                                history  (session.machine);
            DebugEventJob                                    job;
            ScratchEventReplayer<PageWrite>                  replayer (std::make_unique<PageWriteProbe>());
            std::vector<DebugEventStretchResult<PageWrite>>  results;
            HRESULT                                          hr       = S_OK;



            hr = session.BuildJobForAll (history, job);
            AssertSucceeded (hr, L"BuildJob");
            Assert::IsFalse (job.disks.empty(), L"the rig's disk is in its bay");

            job.disks[0].mediaId++;

            replayer.SetMachine (session.machine.GetConfig(), session.machine.GetCurrentMachineName());

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");

            for (const DebugEventStretchResult<PageWrite> & result : results)
            {
                Assert::IsTrue (result.isUnavailable, L"no keyframe was taken with that disk");
                Assert::IsTrue (result.entries.empty());
            }
        }
    };
}
