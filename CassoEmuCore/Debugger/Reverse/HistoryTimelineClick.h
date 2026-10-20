#pragma once

#include "Pch.h"

struct HistoryThumbnailCell;
class HistoryThumbnails;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineClickPlan
//
//  What a click on the history timeline does: stop the machine first and act
//  once it has; or seek to a cycle, or go live. The machine is left stopped
//  where it moved.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryTimelineClickPlan
{
    bool      pauseFirst = false;
    bool      seek       = false;
    uint64_t  cycle      = 0;
    bool      goLive     = false;
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
