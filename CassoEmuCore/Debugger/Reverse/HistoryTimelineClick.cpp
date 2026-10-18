#include "Pch.h"

#include "Debugger/Reverse/HistoryTimelineClick.h"
#include "Debugger/Reverse/HistoryThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineClick::Plan
//
//  History moves only under a stopped machine, so a click while it runs
//  stops it first, and the plan is made again once it has. Once stopped,
//  the machine moves to the exact cycle clicked -- the live end goes live,
//  which a machine already live has no need of -- and runs on from there. A
//  click on the live end of a machine running live has nothing to do.
//
////////////////////////////////////////////////////////////////////////////////

HistoryTimelineClickPlan HistoryTimelineClick::Plan (
    const HistoryThumbnailCell  & cell,
    bool                          isPaused,
    bool                          isBehindLive)
{
    HistoryTimelineClickPlan  plan;
    bool                      isRunningLive = !isPaused && !isBehindLive;



    if (cell.isLive && isRunningLive)
    {
        return plan;
    }

    if (!isPaused)
    {
        plan.pauseFirst = true;
        return plan;
    }

    if (cell.isLive)
    {
        plan.goLive = isBehindLive;
    }
    else
    {
        plan.seek  = true;
        plan.cycle = cell.cycle;
    }

    plan.run = true;

    return plan;
}
