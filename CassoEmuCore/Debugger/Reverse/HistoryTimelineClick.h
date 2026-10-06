#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnails.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineClickPlan
//
//  What a click on the history timeline does, in order: stop the machine
//  first and act once it has; or seek to a cycle, or go live, and then run
//  on.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryTimelineClickPlan
{
    bool      pauseFirst = false;
    bool      seek       = false;
    uint64_t  cycle      = 0;
    bool      goLive     = false;
    bool      run        = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineClick
//
////////////////////////////////////////////////////////////////////////////////

class HistoryTimelineClick
{
public:
    static HistoryTimelineClickPlan  Plan (const HistoryThumbnailCell & cell, bool isPaused, bool isBehindLive);
};
