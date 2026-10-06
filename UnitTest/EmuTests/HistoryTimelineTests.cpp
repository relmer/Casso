#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/HistoryTimelineClick.h"
#include "Debugger/Reverse/HistoryTimelineScrub.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kTimelinePast  = 400;
static constexpr uint64_t  s_kTimelineLive  = 900;
static constexpr uint64_t  s_kTimelineCycle = 4321;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineTests
//
//  What a click on the history timeline does to the machine, and how the
//  timeline shows whether the machine is live or replaying history, and
//  where in history it stands.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryTimelineTests)
{
public:

    TEST_METHOD (AClickOnThePastSeeksThereAndRunsOn)
    {
        HistoryTimelineClickPlan  plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelinePast, s_kTimelineCycle, false), true, false);



        Assert::IsFalse (plan.pauseFirst, L"already stopped");
        Assert::IsTrue  (plan.seek, L"seeks");
        Assert::AreEqual<uint64_t> (s_kTimelineCycle, plan.cycle, L"to the exact cycle clicked, not a keyframe's position");
        Assert::IsFalse (plan.goLive, L"not live");
        Assert::IsTrue  (plan.run, L"and runs on from there");

        plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelinePast, false), true, true);
        Assert::IsTrue (plan.seek && plan.run, L"from behind live too");
    }


    TEST_METHOD (AClickWhileRunningStopsTheMachineFirst)
    {
        HistoryTimelineClickPlan  plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelinePast, false), false, false);



        Assert::IsTrue  (plan.pauseFirst, L"stops first");
        Assert::IsFalse (plan.seek || plan.goLive || plan.run, L"and acts once it has");

        plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelineLive, true), false, true);
        Assert::IsTrue (plan.pauseFirst, L"replaying, the live end stops the replay first");
    }


    TEST_METHOD (AClickOnTheLiveEndGoesLiveAndRuns)
    {
        HistoryTimelineClickPlan  plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelineLive, true), true, true);



        Assert::IsFalse (plan.seek, L"no seek");
        Assert::IsTrue  (plan.goLive, L"goes live");
        Assert::IsTrue  (plan.run, L"and runs");

        plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelineLive, true), true, false);
        Assert::IsFalse (plan.goLive, L"already live");
        Assert::IsTrue  (plan.run, L"runs");

        plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelineLive, true), false, false);
        Assert::IsFalse (plan.pauseFirst || plan.seek || plan.goLive || plan.run, L"running live already: nothing to do");
    }


    TEST_METHOD (TheModeReadsLiveOrReplay)
    {
        Assert::AreEqual (L"Live",   HistoryThumbnails::GetModeText (false), L"live");
        Assert::AreEqual (L"Replay", HistoryThumbnails::GetModeText (true),  L"behind live");
    }


    TEST_METHOD (TheMarkerIsTheCellAtOrBeforeThePlayhead)
    {
        std::vector<HistoryThumbnailCell>  cells = { MakeCell (0, false), MakeCell (300, false), MakeCell (600, false), MakeCell (s_kTimelineLive, true) };



        Assert::AreEqual (3,  HistoryThumbnails::FindPlayheadCell (cells, 450, false), L"live: the live end");
        Assert::AreEqual (1,  HistoryThumbnails::FindPlayheadCell (cells, 450, true),  L"behind live: the point at or before");
        Assert::AreEqual (2,  HistoryThumbnails::FindPlayheadCell (cells, 600, true),  L"on a point: that point");
        Assert::AreEqual (0,  HistoryThumbnails::FindPlayheadCell (cells, 0,   true),  L"the oldest");
        Assert::AreEqual (3,  HistoryThumbnails::FindPlayheadCell (cells, 950, true),  L"past the newest keyframe: the live end");
        Assert::AreEqual (-1, HistoryThumbnails::FindPlayheadCell ({},    450, true),  L"no cells, no marker");
    }

    //  The playhead line stands between the points it lies between, a cell
    //  each, and past the newest keyframe it runs through the live cell to
    //  the end of history.
    TEST_METHOD (ThePlayheadLiesBetweenThePointsByCycle)
    {
        std::vector<HistoryThumbnailCell>  cells = { MakeCell (0, 0, false), MakeCell (10, 300, false), MakeCell (20, 600, false), MakeCell (30, 900, true) };



        Assert::AreEqual (1.5f, HistoryThumbnails::GetPlayheadOffset (cells, 450,  1200), L"halfway from the second point");
        Assert::AreEqual (2.0f, HistoryThumbnails::GetPlayheadOffset (cells, 600,  1200), L"on a point");
        Assert::AreEqual (0.0f, HistoryThumbnails::GetPlayheadOffset (cells, 0,    1200), L"the oldest");
        Assert::AreEqual (3.5f, HistoryThumbnails::GetPlayheadOffset (cells, 1050, 1200), L"through the live cell");
        Assert::AreEqual (4.0f, HistoryThumbnails::GetPlayheadOffset (cells, 1500, 1200), L"no farther than the end");
        Assert::AreEqual (-1.0f, HistoryThumbnails::GetPlayheadOffset ({}, 450, 1200), L"no cells, no line");
    }


    TEST_METHOD (AnOffsetAlongTheStripIsACycle)
    {
        std::vector<HistoryThumbnailCell>  cells = { MakeCell (0, 0, false), MakeCell (10, 300, false), MakeCell (20, 600, false), MakeCell (30, 900, true) };



        Assert::AreEqual<uint64_t> (450,  HistoryThumbnails::GetCycleAtOffset (cells, 1.5f,  1200));
        Assert::AreEqual<uint64_t> (1050, HistoryThumbnails::GetCycleAtOffset (cells, 3.5f,  1200));
        Assert::AreEqual<uint64_t> (0,    HistoryThumbnails::GetCycleAtOffset (cells, -2.0f, 1200), L"before the strip: the oldest");
        Assert::AreEqual<uint64_t> (1200, HistoryThumbnails::GetCycleAtOffset (cells, 9.0f,  1200), L"past it: the end");
    }


    //  A drag goes wherever the pointer goes, cycle for cycle, but the
    //  pointer moves many times a frame: the moves only remember where it
    //  is, and each frame seeks once, to the latest, and not to where it
    //  has already sought.
    TEST_METHOD (ADragSeeksAtMostOncePerFrameToTheLatestPointer)
    {
        HistoryTimelineScrub      scrub;
        HistoryTimelineScrubStep  step;



        for (uint64_t cycle : { 1001ull, 1002ull, 1003ull, 1017ull })
        {
            step = scrub.OnDragged (cycle, false, true, false);
            Assert::IsFalse (step.seek || step.pauseFirst || step.run, L"a move between frames seeks nothing");
        }

        step = scrub.OnFrame (true, false);
        Assert::IsTrue (step.seek, L"the frame seeks");
        Assert::AreEqual<uint64_t> (1017, step.cycle, L"to the latest pointer, not a whole second");

        step = scrub.OnFrame (true, false);
        Assert::IsFalse (step.seek, L"once: the pointer has not moved since");

        scrub.OnDragged (1018, false, true, false);

        step = scrub.OnFrame (true, true);
        Assert::IsFalse (step.seek, L"not while the last seek is still on its way");

        step = scrub.OnFrame (true, false);
        Assert::IsTrue (step.seek && step.cycle == 1018, L"and once it has landed, to where the pointer is now");
        Assert::IsTrue (scrub.IsScrubbing(), L"still dragging");
    }


    //  Letting go seeks there at once, however busy the last seek is, so the
    //  machine always lands exactly where the line was let go.
    TEST_METHOD (LettingGoLandsExactlyWhereTheLineWasLetGo)
    {
        HistoryTimelineScrub      scrub;
        HistoryTimelineScrubStep  step;



        scrub.OnDragged (2000, false, true, false);
        scrub.OnFrame   (true, false);
        scrub.OnDragged (2345, false, true, false);

        step = scrub.OnDragged (2346, true, true, true);

        Assert::IsTrue  (step.seek, L"the release seeks at once");
        Assert::AreEqual<uint64_t> (2346, step.cycle, L"to the cycle under the pointer when it was let go");
        Assert::IsFalse (step.run, L"a machine stopped before the drag stays stopped");
        Assert::IsFalse (scrub.IsScrubbing(), L"the drag is over");

        step = scrub.OnFrame (true, false);
        Assert::IsFalse (step.seek || step.run, L"nothing more to do");

        scrub.OnDragged (2500, false, true, false);
        scrub.OnFrame   (true, false);

        step = scrub.OnDragged (2500, true, true, false);
        Assert::IsFalse (step.seek, L"let go where it last sought: already there");
        Assert::IsFalse (scrub.IsScrubbing(), L"and the drag is over");
    }


    //  A drag of a running machine stops it first, seeks only once it has
    //  stopped, and when the line is let go, runs on from there.
    TEST_METHOD (ADragOfARunningMachineStopsItAndRunsOnAfter)
    {
        HistoryTimelineScrub      scrub;
        HistoryTimelineScrubStep  step;



        step = scrub.OnDragged (3000, false, false, false);
        Assert::IsTrue  (step.pauseFirst, L"stops the machine first");
        Assert::IsFalse (step.seek, L"and seeks nothing yet");

        step = scrub.OnDragged (3100, false, false, false);
        Assert::IsFalse (step.pauseFirst, L"asked once");

        step = scrub.OnFrame (false, false);
        Assert::IsFalse (step.seek, L"not before it has stopped");

        step = scrub.OnDragged (3200, true, false, false);
        Assert::IsFalse (step.seek || step.run, L"let go before it has stopped: nothing yet");
        Assert::IsTrue  (scrub.IsScrubbing(), L"the drag waits for the stop");

        step = scrub.OnFrame (true, false);
        Assert::IsTrue  (step.seek, L"stopped: seeks");
        Assert::AreEqual<uint64_t> (3200, step.cycle, L"to where it was let go");
        Assert::IsTrue  (step.run, L"and runs on, as it was running");
        Assert::IsFalse (scrub.IsScrubbing(), L"the drag is over");
    }

private:

    static HistoryThumbnailCell MakeCell (uint64_t position, uint64_t cycle, bool isLive)
    {
        HistoryThumbnailCell  cell = MakeCell (position, isLive);



        cell.cycle = cycle;

        return cell;
    }


    static HistoryThumbnailCell MakeCell (uint64_t position, bool isLive)
    {
        HistoryThumbnailCell  cell;



        cell.position = position;
        cell.isLive   = isLive;

        return cell;
    }
};
