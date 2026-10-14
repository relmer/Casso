#include "Pch.h"
#include "EmuTests/InlineWorkQueue.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Events/DebugEventHistory.h"
#include "Debugger/Events/MachineEventFeed.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/ScratchEventReplayer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventLogsTests
{
    static constexpr size_t  kSliceCycles = 17030;



    //  Runs the rig's loop for count slices, the host feeding it input at the
    //  start of each.
    static void  Run (TestMachine & machine, size_t firstSlice, size_t count)
    {
        size_t  slice = 0;
        size_t  spent = 0;



        for (slice = firstSlice; slice < firstSlice + count; slice++)
        {
            ReverseSessionRig::Feed (machine, slice);

            for (spent = 0; spent < kSliceCycles; )
            {
                spent += machine.StepOne();
            }
        }
    }


    //  The rows of a delta that came from the cache: not the journal's.
    static size_t  CountCached (const std::vector<InputLogRow> & rows)
    {
        return std::ranges::count_if (rows, [] (const InputLogRow & row)
        {
            return row.kind == DebugLogRowKind::Gap || (row.kind == DebugLogRowKind::Event && row.event.category == InputEventCategory::Guest);
        });
    }


    template <typename T>
    static bool  IsSameEntry (const DebugEventEntry<T> & a, const DebugEventEntry<T> & b)
    {
        return a.kind == b.kind && a.position == b.position && a.cycle == b.cycle && memcmp (&a.event, &b.event, sizeof (T)) == 0;
    }



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PanelRecorder
    //
    //  The Disk II events on the Panel slot, which gets the same calls as the
    //  History slot, each stored with the machine's position and cycle.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PanelRecorder : public IDisk2EventSink
    {
    public:
        struct Seen
        {
            uint64_t  position = 0;
            uint64_t  cycle    = 0;
        };

        explicit PanelRecorder (MachineHost & machine) : m_machine (machine) {}

        std::vector<Seen>  seen;

        void OnMotorCommandOn() override                    { Add(); }
        void OnMotorEngaged() override                      { Add(); }
        void OnMotorCommandOff() override                   { Add(); }
        void OnMotorDisengaged() override                   { Add(); }
        void OnHeadStep        (int, int) override          { Add(); }
        void OnHeadBump        (int) override               { Add(); }
        void OnAddressMark     (int, int, int) override     { Add(); }
        void OnDataMarkRead    (int, int, int, std::span<const Byte>) override { Add(); }
        void OnDataMarkWrite   (int, int, int, std::span<const Byte>) override { Add(); }
        void OnReadStall       (int) override               { Add(); }
        void OnDriveSelect     (int) override               { Add(); }
        void OnDiskInserted    (int) override               { Add(); }
        void OnDiskEjected     (int) override               { Add(); }

    private:
        void  Add()
        {
            seen.push_back ({ m_machine.GetPosition(), m_machine.GetCpu()->GetTotalCycles() });
        }

        MachineHost  & m_machine;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventLogsTests
    //
    //  The Disk II event log and the input log on the rig's session: the
    //  caches have the entries a rebuild from history returns, take nothing
    //  while the machine replays its own history or while recording is
    //  paused, drop a future a change in the past discarded, and send an
    //  open pane each row once, and older rows rebuilt on request.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventLogsTests)
    {
    public:

        struct Session
        {
            TestMachine        machine    { "Apple2e" };
            ReverseController  controller { machine };
            InlineWorkQueue    queue;
            DebugEventLogs     logs       { machine };

            Session()
            {
                HRESULT  hr = S_OK;



                ReverseSessionRig::Prepare (machine);

                hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
                AssertSucceeded (hr, L"Start");

                logs.SetWorkQueue (&queue);
            }

            ~Session()
            {
                logs.Stop();
            }

            void  WaitForKeyframes()
            {
                HRESULT  hr = controller.GetKeyframes().WaitForPending();



                AssertSucceeded (hr, L"WaitForPending");
            }
        };


        template <typename T>
        static void  AssertCacheIsRebuilt (Session & session, const DebugEventCache<T> & cache, std::unique_ptr<IDebugEventProbe<T>> probe, size_t & outEvents)
        {
            DebugEventHistory                        history  (session.machine);
            std::vector<DebugEventBounds>            stretches;
            DebugEventJob                            job;
            ScratchEventReplayer<T>                  replayer (std::move (probe));
            std::vector<DebugEventStretchResult<T>>  results;
            std::vector<DebugEventEntry<T>>          live;
            HRESULT                                  hr       = S_OK;
            size_t                                   i        = 0;



            history.Attach (&session.controller.GetKeyframes());
            history.GetStretches (stretches);
            std::ranges::reverse (stretches);

            hr = history.BuildJob (stretches, job);
            AssertSucceeded (hr, L"BuildJob");

            replayer.SetMachine (session.machine.GetConfig(), session.machine.GetCurrentMachineName());

            hr = replayer.Rebuild (job, results);
            AssertSucceeded (hr, L"Rebuild");

            for (const DebugEventStretchResult<T> & result : results)
            {
                live.clear();
                cache.CopySpan (result.startPosition, result.endPosition, live);

                Assert::AreEqual (live.size(), result.entries.size(), std::format (L"the stretch from {}", result.startPosition).c_str());

                for (i = 0; i < live.size(); i++)
                {
                    Assert::IsTrue (IsSameEntry (live[i], result.entries[i]), std::format (L"entry {} of the stretch from {}", i, result.startPosition).c_str());
                }

                outEvents += live.size();
            }
        }


        TEST_METHOD (TheCachesHoldWhatARebuildGives)
        {
            Session  session;
            size_t   disk    = 0;
            size_t   input   = 0;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 300);
            session.WaitForKeyframes();

            AssertCacheIsRebuilt<Disk2Event> (session, session.logs.GetDiskCache(),  std::make_unique<DiskEventProbe>(),  disk);
            AssertCacheIsRebuilt<InputEvent> (session, session.logs.GetInputCache(), std::make_unique<InputEventProbe>(), input);

            Assert::IsTrue (disk > 0,  L"the loop ran the disk");
            Assert::IsTrue (input > 0, L"and polled the keyboard and paddle");
        }


        TEST_METHOD (NothingIsCachedWhileTheMachineReplaysItsHistory)
        {
            Session        session;
            uint64_t       diskEnd  = 0;
            uint64_t       inputEnd = 0;
            int            i        = 0;
            HRESULT        hr       = S_OK;
            ReverseResult  result;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 120);
            session.WaitForKeyframes();

            diskEnd  = session.logs.GetDiskCache().GetEndSerial();
            inputEnd = session.logs.GetInputCache().GetEndSerial();

            for (i = 0; i < 3; i++)
            {
                hr = session.controller.StepBack (result);
                AssertSucceeded (hr, L"StepBack");
            }

            hr = session.controller.SeekToPosition (session.machine.GetPosition() - 5000, result);
            AssertSucceeded (hr, L"SeekToPosition");

            Assert::IsTrue   (session.controller.IsInHistory());
            Assert::AreEqual (diskEnd,  session.logs.GetDiskCache().GetEndSerial(),  L"no disk event cached behind live");
            Assert::AreEqual (inputEnd, session.logs.GetInputCache().GetEndSerial(), L"no input event cached behind live");
        }


        //  A change made in the past cuts history at the machine's position:
        //  the cache drops its entries from there, and the next delta drops
        //  an open pane's rows from there.
        TEST_METHOD (ACutDropsTheDiscardedFutureAndTellsThePane)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            uint64_t                             cut     = 0;
            HRESULT                              hr      = S_OK;
            ReverseResult                        result;
            InputLogDelta                        delta;



            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Input, true);
            Run (session.machine, 0, 200);
            session.WaitForKeyframes();

            session.logs.TakeDelta (delta);
            Assert::IsTrue (delta.isReset,        L"the pane's first delta");
            Assert::IsTrue (cache.GetCount() > 4, L"the loop polled the keyboard and paddle");

            cut = cache.GetAt (cache.GetCount() / 2).position;

            hr = session.controller.SeekToPosition (cut, result);
            AssertSucceeded (hr, L"SeekToPosition");

            hr = session.controller.OnMachineChanged();
            AssertSucceeded (hr, L"OnMachineChanged");

            Assert::IsTrue (cache.GetAt (cache.GetCount() - 1).position < cut, L"nothing cached from the cut on");

            session.logs.TakeDelta (delta);
            Assert::AreEqual (cut, delta.cutFrom, L"the pane drops its rows from the cut on");
        }


        //  At Maximum speed recording pauses: nothing is cached until it
        //  resumes, where a gap entry is.
        TEST_METHOD (AGapIsMarkedWhereRecordingResumes)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            uint64_t                             paused  = 0;
            uint64_t                             resumed = 0;
            size_t                               gaps    = 0;
            size_t                               i       = 0;
            HRESULT                              hr      = S_OK;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 60);

            hr = session.controller.SetUserMaximumSpeed (true);
            AssertSucceeded (hr, L"SetUserMaximumSpeed");

            paused = session.machine.GetPosition();
            Run (session.machine, 60, 60);
            resumed = session.machine.GetPosition();

            hr = session.controller.SetUserMaximumSpeed (false);
            AssertSucceeded (hr, L"SetUserMaximumSpeed");

            Run (session.machine, 120, 60);


            for (i = 0; i < cache.GetCount(); i++)
            {
                const DebugEventEntry<InputEvent>  & entry = cache.GetAt (i);

                if (entry.kind == DebugEventEntryKind::Gap)
                {
                    Assert::AreEqual (resumed, entry.position, L"the gap is where recording resumed");
                    gaps++;
                }
                else
                {
                    Assert::IsFalse (entry.position >= paused && entry.position < resumed, L"no event from the gap");
                }
            }

            Assert::AreEqual ((size_t) 1, gaps);
        }


        //  A pane gets everything when it opens, then each new row once.
        TEST_METHOD (AnOpenPaneGetsEachRowOnce)
        {
            Session                              session;
            const DebugEventCache<InputEvent>  & cache   = session.logs.GetInputCache();
            InputLogDelta                        first;
            InputLogDelta                        second;
            InputLogDelta                        third;



            session.logs.Start (&session.controller);
            Run (session.machine, 0, 30);

            session.logs.SetOpen (DebugLogKind::Input, true);
            session.logs.TakeDelta (first);

            Run (session.machine, 30, 30);
            session.logs.TakeDelta (second);
            session.logs.TakeDelta (third);

            Assert::IsTrue   (first.isReset);
            Assert::IsFalse  (second.isReset);
            Assert::IsFalse  (first.newer.empty() || second.newer.empty(), L"rows in both");
            Assert::IsTrue   (first.newer.back().position <= second.newer.front().position, L"the second follows the first");
            Assert::AreEqual (cache.GetCount(), CountCached (first.newer) + CountCached (second.newer), L"every cached entry once");
            Assert::IsTrue   (third.IsEmpty(), L"nothing new");
        }


        //  After a request for the rows before its oldest, the pane gets the
        //  rows of history rebuilt back from there, the events the machine
        //  reported then.
        TEST_METHOD (EarlierRowsAreRebuiltFromHistory)
        {
            Session                            session;
            PanelRecorder                      recorder (session.machine);
            uint64_t                           start    = 0;
            size_t                             tries    = 0;
            std::vector<DiskLogRow>            older;
            std::vector<PanelRecorder::Seen>   reported;
            DiskLogDelta                       delta;
            size_t                             i        = 0;



            session.machine.AttachObservers ({ &recorder, nullptr }, ObserverSlot::Panel);

            Run (session.machine, 0, 200);

            start = session.machine.GetPosition();
            session.logs.Start (&session.controller);
            session.logs.SetOpen (DebugLogKind::Disk, true);

            Run (session.machine, 200, 30);
            session.WaitForKeyframes();

            session.logs.TakeDelta (delta);
            Assert::AreEqual (start, delta.heldFrom, L"the cache starts where the log started");
            Assert::IsFalse  (delta.isAtStart,       L"history starts earlier");

            session.logs.RequestEarlier (DebugLogKind::Disk);

            for (tries = 0; tries < 10 && older.empty(); tries++)
            {
                session.logs.TakeDelta (delta);
                session.queue.WaitAll();
                session.logs.Pump();

                older = delta.older;
            }

            session.logs.TakeDelta (delta);
            older.insert (older.end(), delta.older.begin(), delta.older.end());

            session.machine.AttachObservers ({}, ObserverSlot::Panel);

            Assert::IsFalse (older.empty(), L"rows were rebuilt");

            std::ranges::copy_if (recorder.seen, std::back_inserter (reported), [&delta, start] (const PanelRecorder::Seen & seen)
            {
                return seen.position >= delta.heldFrom && seen.position < start;
            });

            std::erase_if (older, [] (const DiskLogRow & row) { return row.kind != DebugLogRowKind::Event; });

            Assert::AreEqual (reported.size(), older.size(), L"the events the machine reported then");

            for (i = 0; i < older.size(); i++)
            {
                Assert::AreEqual (reported[i].position, older[i].position);
                Assert::AreEqual (reported[i].cycle,    older[i].cycle);
            }
        }
    };
}
