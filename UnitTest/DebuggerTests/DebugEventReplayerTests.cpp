#include "Pch.h"

#include "Debugger/Events/DebugEventReplayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugEventReplayerTests
{
    static constexpr uint64_t  kStretch = 100;

    using Entry  = DebugEventEntry<int>;
    using Result = DebugEventStretchResult<int>;



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FakeHistory
    //
    //  Stretches of kStretch positions from 0 to end, and jobs that list the
    //  requested stretches by their bounds alone.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FakeHistory : public IDebugEventHistory
    {
    public:
        uint64_t  end         = 1000;
        uint64_t  gapAfter    = UINT64_MAX;     // the start of a stretch that ends at a gap
        uint64_t  generation  = 0;

        void  GetStretches (std::vector<DebugEventBounds> & outStretches) const override
        {
            outStretches.clear();

            for (uint64_t start = 0; start + kStretch <= end; start += kStretch)
            {
                outStretches.push_back ({ start, start + kStretch, start == gapAfter });
            }
        }

        HRESULT  BuildJob (const std::vector<DebugEventBounds> & stretches, DebugEventJob & outJob) override
        {
            outJob            = DebugEventJob();
            outJob.generation = ++generation;

            for (const DebugEventBounds & bounds : stretches)
            {
                DebugEventStretch  stretch;

                stretch.startPosition = bounds.startPosition;
                stretch.endPosition   = bounds.endPosition;
                outJob.stretches.push_back (std::move (stretch));
            }

            return S_OK;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FakeRunner
    //
    //  Stores the jobs submitted, and finishes their stretches on demand with
    //  one event every ten positions, the event its position.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FakeRunner : public IDebugEventRunner<int>
    {
    public:
        std::vector<std::shared_ptr<const DebugEventJob>>  submitted;
        std::deque<Result>                                 results;
        size_t                                             cancels = 0;

        HRESULT  Submit (std::shared_ptr<const DebugEventJob> job) override
        {
            submitted.push_back (std::move (job));
            return S_OK;
        }

        bool  TryTakeResult (Result & outResult) override
        {
            bool  hasOne = !results.empty();

            if (hasOne)
            {
                outResult = std::move (results.front());
                results.pop_front();
            }

            return hasOne;
        }

        void  Cancel() override
        {
            cancels++;
        }

        //  The last job's stretches, all of them, or the first count.
        void  Finish (size_t count = SIZE_MAX, bool isUnavailable = false)
        {
            const DebugEventJob  & job = *submitted.back();

            for (size_t i = 0; i < job.stretches.size() && i < count; i++)
            {
                Result  result;

                result.generation    = job.generation;
                result.startPosition = job.stretches[i].startPosition;
                result.endPosition   = job.stretches[i].endPosition;
                result.isUnavailable = isUnavailable;

                for (uint64_t position = result.startPosition; position < result.endPosition && !isUnavailable; position += 10)
                {
                    result.entries.push_back (Entry { DebugEventEntryKind::Event, position, position, (int) position });
                }

                results.push_back (std::move (result));
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugEventReplayerTests
    //
    //  Spans served from the cache where it is complete and from rebuilt
    //  stretches before it: the stretches a span overlaps queued newest first
    //  with the prefetch, a request already on its way not sent again,
    //  rebuilt stretches reused, gaps and unavailable stretches marked, and a
    //  cut of history dropping what it made stale.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugEventReplayerTests)
    {
    public:

        struct Rig
        {
            DebugEventCache<int>       cache    { 1024 };
            FakeHistory                history;
            FakeRunner                 runner;
            DebugEventReplayer<int>    replayer { cache, history, runner };

            //  The cache complete from 800 on, an event every ten positions.
            Rig()
            {
                cache.Reset (800);

                for (uint64_t position = 800; position < 1050; position += 10)
                {
                    cache.Append (position, position, (int) position);
                }
            }
        };


        TEST_METHOD (ASpanTheCacheCoversNeedsNoReplay)
        {
            Rig                 rig;
            std::vector<Entry>  span;
            HRESULT             hr = S_OK;



            hr = rig.replayer.Request (850, 950);
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::IsTrue   (rig.runner.submitted.empty(), L"nothing to replay");
            Assert::IsTrue   (rig.replayer.GetSpan (850, 950, span));
            Assert::AreEqual ((size_t) 10, span.size());
            Assert::AreEqual (850, span.front().event);
        }


        TEST_METHOD (OlderStretchesAreQueuedNewestFirstWithThePrefetch)
        {
            Rig                    rig;
            std::vector<uint64_t>  starts;
            HRESULT                hr = S_OK;



            hr = rig.replayer.Request (550, 850);
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::AreEqual ((size_t) 1, rig.runner.submitted.size());

            for (const DebugEventStretch & stretch : rig.runner.submitted.back()->stretches)
            {
                starts.push_back (stretch.startPosition);
            }

            Assert::IsTrue (starts == std::vector<uint64_t> { 700, 600, 500, 400, 300 },
                            L"the span's three stretches before the cache, newest first, then the two before them");
        }


        TEST_METHOD (TheSpanIsCompleteOnceItsStretchesAreRebuilt)
        {
            Rig                 rig;
            std::vector<Entry>  span;



            (void) rig.replayer.Request (650, 850);

            Assert::IsFalse (rig.replayer.GetSpan (650, 850, span), L"its stretches are not rebuilt yet");

            rig.runner.Finish();
            Assert::IsTrue (rig.replayer.Pump());

            Assert::IsTrue   (rig.replayer.GetSpan (650, 850, span));
            Assert::AreEqual ((size_t) 20, span.size(), L"650 to 790 rebuilt, 800 to 840 from the cache");
            Assert::AreEqual (650, span.front().event);
            Assert::AreEqual (840, span.back().event);

            for (size_t i = 1; i < span.size(); i++)
            {
                Assert::IsTrue (span[i - 1].position < span[i].position, L"oldest first, no repeats");
            }
        }


        TEST_METHOD (ARequestOnItsWayIsNotSentAgain)
        {
            Rig  rig;



            (void) rig.replayer.Request (650, 850);
            (void) rig.replayer.Request (650, 850);
            (void) rig.replayer.Request (700, 850);

            Assert::AreEqual ((size_t) 1, rig.runner.submitted.size(), L"every stretch requested is in the job sent");
        }


        TEST_METHOD (RebuiltStretchesAreReused)
        {
            Rig  rig;



            (void) rig.replayer.Request (650, 850);
            rig.runner.Finish();
            (void) rig.replayer.Pump();

            (void) rig.replayer.Request (650, 850);
            Assert::AreEqual ((size_t) 1, rig.runner.submitted.size(), L"scrolling back to rebuilt stretches replays nothing");

            (void) rig.replayer.Request (450, 850);
            Assert::AreEqual ((size_t) 2, rig.runner.submitted.size());
            Assert::AreEqual ((uint64_t) 300, rig.runner.submitted.back()->stretches.front().startPosition, L"400 came with the prefetch, so only the prefetch before it");
        }


        TEST_METHOD (AResultOfAJobReplacedIsIgnored)
        {
            Rig                 rig;
            std::vector<Entry>  span;



            (void) rig.replayer.Request (650, 850);
            (void) rig.replayer.Request (150, 250);

            rig.runner.submitted.erase (rig.runner.submitted.end() - 1);
            rig.runner.Finish();

            Assert::IsFalse  (rig.replayer.Pump(), L"the first job was replaced");
            Assert::AreEqual ((size_t) 0, rig.replayer.GetRebuiltCount());
        }


        TEST_METHOD (UnavailableAndGapStretchesAreMarked)
        {
            Rig                 rig;
            std::vector<Entry>  span;



            rig.history.gapAfter = 700;

            (void) rig.replayer.Request (700, 800);
            rig.runner.Finish (1);
            (void) rig.replayer.Pump();

            Assert::IsTrue (rig.replayer.GetSpan (700, 800, span));
            Assert::IsTrue (span.back().kind == DebugEventEntryKind::Event, L"the gap sits where the next stretch, which the cache covers, begins");

            (void) rig.replayer.Request (600, 700);
            rig.runner.Finish (1, true);
            (void) rig.replayer.Pump();

            Assert::IsTrue   (rig.replayer.GetSpan (600, 800, span));
            Assert::IsTrue   (span.front().kind == DebugEventEntryKind::Unavailable, L"600's keyframe was taken with other disks");
            Assert::AreEqual ((uint64_t) 600, span.front().position);
        }


        TEST_METHOD (AGapBetweenRebuiltStretchesIsMarkedWhereRecordingResumed)
        {
            Rig                 rig;
            std::vector<Entry>  span;
            auto                gap = span.end();



            rig.history.gapAfter = 600;

            (void) rig.replayer.Request (600, 800);
            rig.runner.Finish();
            (void) rig.replayer.Pump();

            Assert::IsTrue (rig.replayer.GetSpan (600, 800, span));

            gap = std::ranges::find_if (span, [] (const Entry & entry) { return entry.kind == DebugEventEntryKind::Gap; });

            Assert::IsTrue   (gap != span.end(), L"a gap row");
            Assert::AreEqual ((uint64_t) 700, gap->position);
            Assert::AreEqual (690, (gap - 1)->event, L"after the stretch before it");
        }


        TEST_METHOD (ACutOfHistoryDropsTheStretchesAfterIt)
        {
            Rig  rig;



            (void) rig.replayer.Request (450, 850);
            rig.runner.Finish();
            (void) rig.replayer.Pump();

            Assert::AreEqual ((size_t) 6, rig.replayer.GetRebuiltCount());

            rig.replayer.OnHistoryCut (600);

            Assert::AreEqual ((size_t) 4, rig.replayer.GetRebuiltCount(), L"200 to 500 end at or before the cut");
            Assert::AreEqual ((size_t) 1, rig.runner.cancels);
        }
    };
}
