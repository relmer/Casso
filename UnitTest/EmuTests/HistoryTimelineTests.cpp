#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/HistoryTimelineClick.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kTimelinePast = 400;
static constexpr uint64_t  s_kTimelineLive = 900;





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
        HistoryTimelineClickPlan  plan = HistoryTimelineClick::Plan (MakeCell (s_kTimelinePast, false), true, false);



        Assert::IsFalse (plan.pauseFirst, L"already stopped");
        Assert::IsTrue  (plan.seek, L"seeks");
        Assert::AreEqual<uint64_t> (s_kTimelinePast, plan.position, L"to the cell's keyframe");
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


    //  A drag moves in whole seconds of emulated time, never out of history.
    TEST_METHOD (ADragSnapsToWholeSecondsWithinHistory)
    {
        constexpr uint64_t  kSecond = 1000;

        Assert::AreEqual<uint64_t> (5000, HistoryThumbnails::SnapToSecond (5400, kSecond, 1500, 9000), L"down to the nearer second");
        Assert::AreEqual<uint64_t> (6000, HistoryThumbnails::SnapToSecond (5600, kSecond, 1500, 9000), L"up to the nearer second");
        Assert::AreEqual<uint64_t> (1500, HistoryThumbnails::SnapToSecond (1200, kSecond, 1500, 9000), L"no earlier than history begins");
        Assert::AreEqual<uint64_t> (9000, HistoryThumbnails::SnapToSecond (9400, kSecond, 1500, 9000), L"no later than it ends");
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
