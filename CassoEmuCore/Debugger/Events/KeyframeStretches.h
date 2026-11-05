#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventJob.h"

class KeyframeStore;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches
//
//  The stretches between a keyframe store's keyframes, oldest first, and the
//  runs they join into where no gap lies between them. Sync updates both at
//  their ends as keyframes are added, dropped and trimmed, so a store of
//  hundreds of thousands of keyframes costs a few compares per call rather
//  than a walk over all of them.
//
//  Used on the thread that runs the machine.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeStretches
{
public:
    void      Sync                  (const KeyframeStore * keyframes);
    void      Clear                 ();

    const std::deque<DebugEventBounds> &  GetStretches () const { return m_stretches; }
    const std::deque<DebugEventBounds> &  GetRuns      () const { return m_runs; }
    size_t                                GetMadeCount () const { return m_madeCount; }

    size_t    FindFirstEndingAfter  (uint64_t position) const;

    static DebugEventBounds  MakeStretch (const KeyframeStore & keyframes, size_t index);

private:
    void      PushBack              (const DebugEventBounds & stretch);
    void      PopFront              ();
    void      PopBack               ();

    std::deque<DebugEventBounds>  m_stretches;
    std::deque<DebugEventBounds>  m_runs;
    size_t                        m_madeCount = 0;      // stretches made since construction, for tests of the cost
};
