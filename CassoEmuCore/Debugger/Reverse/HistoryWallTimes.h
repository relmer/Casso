#pragma once

#include "Pch.h"

class KeyframeStore;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes
//
//  The host's clock at any cycle of history, for a thread that cannot read
//  the keyframe store: the store's cycle and clock time for every keyframe,
//  kept in step with it by the thread that runs the machine, so the time at
//  a cycle is found as ReverseController::GetWallTimeAt finds it, from the
//  newest keyframe at or before it.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryWallTimes
{
public:
    //  Machine thread: the new keyframes taken on, the dropped ones let go.
    void      Sync          (const KeyframeStore & keyframes);
    void      Clear         ();

    //  Any thread. 0 before the oldest keyframe, or with none.
    uint64_t  GetWallTimeAt (uint64_t cycle) const;
    size_t    GetCount      () const;

private:
    struct Mark
    {
        uint64_t  position = 0;
        uint64_t  cycle    = 0;
        uint64_t  wallTime = 0;
    };

    static Mark  MakeMark   (const KeyframeStore & keyframes, size_t index);
    bool         IsPrefixOf (const KeyframeStore & keyframes) const;

    mutable std::mutex  m_lock;
    std::deque<Mark>    m_marks;
};
