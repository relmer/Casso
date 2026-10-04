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

private:

    static HistoryThumbnailCell MakeCell (uint64_t position, bool isLive)
    {
        HistoryThumbnailCell  cell;



        cell.position = position;
        cell.isLive   = isLive;

        return cell;
    }
};
