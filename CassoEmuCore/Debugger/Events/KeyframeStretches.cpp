#include "Pch.h"

#include "Debugger/Events/KeyframeStretches.h"

#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::Sync
//
//  Drops the stretches whose keyframe is gone from the oldest end, then the
//  ones at the newest end that no longer match the store (a trim, or a gap
//  marked on the newest keyframe), then adds one for each keyframe since.
//  A store whose oldest keyframe is not the oldest stretch's start has been
//  cleared and started over, and the lists are made again.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStretches::Sync (const KeyframeStore * keyframes)
{
    size_t    count = (keyframes != nullptr) ? keyframes->GetCount() : 0;
    uint64_t  first = 0;



    if (count < 2)
    {
        Clear();
        return;
    }

    first = keyframes->GetInfo (0).position;

    while (!m_stretches.empty() && m_stretches.front().startPosition < first)
    {
        PopFront();
    }

    if (!m_stretches.empty() && m_stretches.front().startPosition != first)
    {
        Clear();
    }

    while (m_stretches.size() > count - 1)
    {
        PopBack();
    }

    while (!m_stretches.empty() && m_stretches.back() != MakeStretch (*keyframes, m_stretches.size() - 1))
    {
        PopBack();
    }

    while (m_stretches.size() < count - 1)
    {
        PushBack (MakeStretch (*keyframes, m_stretches.size()));
        m_madeCount++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::Clear
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStretches::Clear()
{
    m_stretches.clear();
    m_runs.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::FindFirstEndingAfter
//
//  The index of the first stretch that ends after position, or the count
//  when none does. The stretches' ends rise with their starts.
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeStretches::FindFirstEndingAfter (uint64_t position) const
{
    auto  at = std::ranges::partition_point (m_stretches, [position] (const DebugEventBounds & each) { return each.endPosition <= position; });



    return (size_t) (at - m_stretches.begin());
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::MakeStretch
//
//  From keyframe index to the next, or to where recording paused when a gap
//  lies before the next.
//
////////////////////////////////////////////////////////////////////////////////

DebugEventBounds KeyframeStretches::MakeStretch (
    const KeyframeStore  & keyframes,
    size_t                 index)
{
    const KeyframeInfo  & start = keyframes.GetInfo (index);
    const KeyframeInfo  & next  = keyframes.GetInfo (index + 1);



    return { start.position, next.hasGapBefore ? next.gapStart : next.position, next.hasGapBefore };
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::PushBack
//
//  A stretch after a gap starts a run; otherwise it extends the newest run.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStretches::PushBack (const DebugEventBounds & stretch)
{
    if (m_runs.empty() || m_runs.back().hasGapAfter)
    {
        m_runs.push_back (stretch);
    }
    else
    {
        m_runs.back().endPosition = stretch.endPosition;
        m_runs.back().hasGapAfter = stretch.hasGapAfter;
    }

    m_stretches.push_back (stretch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::PopFront
//
//  The first stretch is removed from the oldest run, and the run is removed
//  when that was its only stretch.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStretches::PopFront()
{
    const DebugEventBounds  & stretch = m_stretches.front();
    DebugEventBounds        & run     = m_runs.front();



    if (run.endPosition == stretch.endPosition)
    {
        m_runs.pop_front();
    }
    else
    {
        run.startPosition = stretch.endPosition;
    }

    m_stretches.pop_front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeStretches::PopBack
//
//  The last stretch is removed from the newest run, and the run is removed
//  when that was its only stretch. The stretch before it had no gap after
//  it, or the run would have ended there.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeStretches::PopBack()
{
    const DebugEventBounds  & stretch = m_stretches.back();
    DebugEventBounds        & run     = m_runs.back();



    if (run.startPosition == stretch.startPosition)
    {
        m_runs.pop_back();
    }
    else
    {
        run.endPosition = stretch.startPosition;
        run.hasGapAfter = false;
    }

    m_stretches.pop_back();
}
