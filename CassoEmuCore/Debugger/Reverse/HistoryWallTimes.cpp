#include "Pch.h"

#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/HistoryWallTimes.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::Sync
//
//  Once a frame, so only what changed is touched: keyframes dropped from the
//  old end are let go and new ones at the live end taken on. When what is
//  held is no longer the start of what the store holds -- history cut off
//  or begun again -- it is all taken again.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryWallTimes::Sync (const KeyframeStore & keyframes)
{
    std::lock_guard<std::mutex>  held (m_lock);
    size_t                       count  = keyframes.GetCount();
    uint64_t                     oldest = (count > 0) ? keyframes.GetInfo (0).position : 0;
    size_t                       i      = 0;



    while (!m_marks.empty() && m_marks.front().position < oldest)
    {
        m_marks.pop_front();
    }

    if (!IsPrefixOf (keyframes))
    {
        m_marks.clear();
    }

    for (i = m_marks.size(); i < count; i++)
    {
        m_marks.push_back (MakeMark (keyframes, i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::Clear
//
////////////////////////////////////////////////////////////////////////////////

void HistoryWallTimes::Clear()
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_marks.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::GetWallTimeAt
//
//  The newest keyframe at or before the cycle, its clock time counted on by
//  the emulated time since.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryWallTimes::GetWallTimeAt (uint64_t cycle) const
{
    std::lock_guard<std::mutex>        held (m_lock);
    std::deque<Mark>::const_iterator   after;



    after = std::upper_bound (m_marks.begin(), m_marks.end(), cycle,
                              [] (uint64_t value, const Mark & mark) { return value < mark.cycle; });

    if (after == m_marks.begin())
    {
        return 0;
    }

    --after;

    return HistoryStatus::GetWallTimeAt (after->wallTime, after->cycle, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::GetCount
//
////////////////////////////////////////////////////////////////////////////////

size_t HistoryWallTimes::GetCount() const
{
    std::lock_guard<std::mutex>  held (m_lock);



    return m_marks.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::MakeMark
//
////////////////////////////////////////////////////////////////////////////////

HistoryWallTimes::Mark HistoryWallTimes::MakeMark (
    const KeyframeStore  & keyframes,
    size_t                 index)
{
    const KeyframeInfo  & info = keyframes.GetInfo (index);



    return Mark { info.position, info.cycle, info.wallTime };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryWallTimes::IsPrefixOf
//
//  Under the lock: whether the keyframes held are the store's first ones,
//  judged by the first and the last, since keyframes are only ever added at
//  the new end and dropped at the old one.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryWallTimes::IsPrefixOf (const KeyframeStore & keyframes) const
{
    size_t  count = m_marks.size();



    if (count == 0)
    {
        return true;
    }

    if (count > keyframes.GetCount())
    {
        return false;
    }

    return keyframes.GetInfo (0).position == m_marks.front().position &&
           keyframes.GetInfo (count - 1).position == m_marks.back().position &&
           keyframes.GetInfo (count - 1).wallTime == m_marks.back().wallTime;
}





