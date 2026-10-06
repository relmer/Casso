#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineScrubStep
//
//  What to do now for a drag of the history timeline's playhead line, in
//  order: stop the machine; seek to a cycle; run on.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryTimelineScrubStep
{
    bool      pauseFirst = false;
    bool      seek       = false;
    uint64_t  cycle      = 0;
    bool      run        = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineScrub
//
////////////////////////////////////////////////////////////////////////////////

class HistoryTimelineScrub
{
public:
    //  The line dragged to a cycle, isFinal once it is let go; whether the
    //  machine is stopped, and whether a seek asked for earlier has yet to
    //  land.
    HistoryTimelineScrubStep  OnDragged   (uint64_t cycle, bool isFinal, bool isPaused, bool isSeekBusy);

    //  Once a frame.
    HistoryTimelineScrubStep  OnFrame     (bool isPaused, bool isSeekBusy);

    bool                      IsScrubbing () const { return m_isActive; }

private:
    HistoryTimelineScrubStep  TakeStep    (bool isPaused, bool isSeekBusy);

    std::optional<uint64_t>  m_pending;
    uint64_t                 m_lastSought = UINT64_MAX;
    bool                     m_isActive   = false;
    bool                     m_isEnded    = false;
    bool                     m_wasRunning = false;
};
