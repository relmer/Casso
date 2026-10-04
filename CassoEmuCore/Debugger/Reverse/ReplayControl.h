#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayControl
//
//  What another thread can see of a reverse command while the thread that
//  runs the machine works through it, and the one thing it can ask: to stop.
//  A reverse run, a step back over and a step back out search history for
//  where they land, and check isStopRequested as they go; a stopped search
//  lands on the last position it reached. progress is the part of history
//  the search has covered so far, from 0 to 1, or -1 for a command that
//  reports none.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayControl
{
    static constexpr float  kNoProgress = -1.0f;

    std::atomic<bool>   isStopRequested { false };
    std::atomic<float>  progress        { kNoProgress };
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayProgress
//
//  A reading of a reverse command in progress, for the debugger window:
//  whether one is running, how long it has run, and the part of history it
//  has covered, or ReplayControl::kNoProgress for one that reports none.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayProgress
{
    bool      isReplaying = false;
    uint64_t  elapsedMs   = 0;
    float     fraction    = ReplayControl::kNoProgress;
};
