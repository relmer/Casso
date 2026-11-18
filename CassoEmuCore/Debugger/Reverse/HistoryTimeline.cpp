#include "Pch.h"

#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/HistoryTimeline.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::HistoryTimeline
//
////////////////////////////////////////////////////////////////////////////////

HistoryTimeline::HistoryTimeline (const KeyframeStore & keyframes) :
    m_keyframes (keyframes)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::GetWallTimeAt
//
//  The host's clock when the machine stood at cycle live: the newest
//  keyframe at or before it, counted on by the emulated time since. 0 with
//  no keyframe to count from.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryTimeline::GetWallTimeAt (uint64_t cycle) const
{
    size_t  index   = 0;
    bool    isFound = m_keyframes.TryFindAtOrBefore (cycle, index);



    if (!isFound)
    {
        return 0;
    }

    return HistoryStatus::GetWallTimeAt (m_keyframes.GetInfo (index).wallTime, m_keyframes.GetInfo (index).cycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::GetWallTimeAtPosition
//
//  The host's clock when the machine was at position. Between two keyframes
//  with no gap, the time is spread between theirs by position, since a
//  position never starts again and the CPU's cycle count does at a power
//  cycle. Before the oldest keyframe it is counted back at the pace of the
//  first stretch. After the newest, or before a gap, it is counted on from
//  the keyframe by the emulated time to cycle, given in history's cycles.
//  0 with no keyframe.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryTimeline::GetWallTimeAtPosition (
    uint64_t  position,
    uint64_t  cycle) const
{
    size_t                 count   = m_keyframes.GetCount();
    size_t                 index   = 0;
    bool                   isFound = m_keyframes.TryFindByPosition (position, index);
    const KeyframeInfo   * at      = nullptr;
    const KeyframeInfo   * next    = nullptr;
    double                 span    = 0;
    double                 offset  = 0;



    if (count == 0)
    {
        return 0;
    }

    index = isFound ? index : 0;
    at    = &m_keyframes.GetInfo (index);
    next  = (index + 1 < count) ? &m_keyframes.GetInfo (index + 1) : nullptr;

    if (next == nullptr || next->hasGapBefore || next->position <= at->position || at->wallTime == 0)
    {
        return HistoryStatus::GetWallTimeAt (at->wallTime, at->cycle, cycle);
    }

    span   = (double) next->position - (double) at->position;
    offset = ((double) next->wallTime - (double) at->wallTime) * ((double) position - (double) at->position) / span;

    return (uint64_t) ((int64_t) at->wallTime + std::llround (offset));
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::TryGetSummary
//
//  What the stretch starting at keyframe did, from the keyframe after it:
//  false for the newest, whose stretch runs on to live, and for a stretch
//  with no record. The disk is quiet when the controller's switch count is
//  the same at both ends and its motor was stopped at both.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryTimeline::TryGetSummary (
    size_t            keyframe,
    StretchSummary  & outSummary) const
{
    const KeyframeInfo  * at   = nullptr;
    const KeyframeInfo  * next = nullptr;



    if (keyframe + 1 >= m_keyframes.GetCount())
    {
        return false;
    }

    at   = &m_keyframes.GetInfo (keyframe);
    next = &m_keyframes.GetInfo (keyframe + 1);

    if (!next->activity.isKnown)
    {
        return false;
    }

    outSummary.pagesRun    = next->activity.pagesRun;
    outSummary.isDiskQuiet = at->activity.diskSwitches == next->activity.diskSwitches && !at->activity.isDiskSpinning && !next->activity.isDiskSpinning;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::TryFindStretch
//
//  The stretch that holds position: the one starting at the newest keyframe
//  at or before it. False when position is older than every keyframe.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryTimeline::TryFindStretch (
    uint64_t    position,
    uint64_t    newestEnd,
    Stretch   & outStretch) const
{
    size_t  keyframe = 0;
    bool    isFound  = m_keyframes.TryFindByPosition (position, keyframe);



    if (isFound)
    {
        outStretch.keyframe = keyframe;
        outStretch.start    = m_keyframes.GetInfo (keyframe).position;
        outStretch.end      = GetSegmentEnd (keyframe, newestEnd);
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline::GetSegmentEnd
//
//  The last position a replay from keyframe can reach: the next keyframe,
//  or where recording paused before it, or, for the newest, newestEnd.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryTimeline::GetSegmentEnd (size_t keyframe, uint64_t newestEnd) const
{
    uint64_t              end  = newestEnd;
    const KeyframeInfo  * next = nullptr;



    if (keyframe + 1 < m_keyframes.GetCount())
    {
        next = &m_keyframes.GetInfo (keyframe + 1);
        end  = next->hasGapBefore ? next->gapStart : next->position;
    }

    return end;
}
