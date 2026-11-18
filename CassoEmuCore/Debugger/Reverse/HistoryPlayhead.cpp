#include "Pch.h"

#include "Debugger/Reverse/HistoryPlayhead.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::SetPosition
//
//  Machine thread, every turn: the machine's position, and whether it is
//  behind live.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::SetPosition (
    uint64_t  position,
    bool      isBehindLive)
{
    m_position.store     (position,     std::memory_order_release);
    m_isBehindLive.store (isBehindLive, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::SetTime
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::SetTime (
    uint64_t  cycle,
    uint64_t  wallTime,
    uint64_t  endCycle)
{
    m_cycle.store    (cycle,    std::memory_order_release);
    m_wallTime.store (wallTime, std::memory_order_release);
    m_endCycle.store (endCycle, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::SetBegin
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::SetBegin (
    uint64_t  cycle,
    uint64_t  wallTime)
{
    m_beginCycle.store (cycle,    std::memory_order_release);
    m_beginWall.store  (wallTime, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::NoteSeekLanded
//
//  Machine thread, after SetTime has given where the seek landed, so the UI
//  thread, seeing the landing, reads that cycle and not the one before it.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::NoteSeekLanded (uint64_t cycle)
{
    m_landedCycle.store (cycle, std::memory_order_release);
    m_landings.fetch_add (1, std::memory_order_acq_rel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::IsAtDrop
//
//  UI thread: whether the playhead line remains where it was dragged. That
//  ends once a seek to that cycle has run since the line last moved; a
//  seek elsewhere landing late, as one made while the line was moving, does
//  not end it. A seek that never runs, as one dropped with the machine it
//  was for, ends it after kSeekPatienceMs.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryPlayhead::IsAtDrop (double nowMs)
{
    uint64_t  landings  = 0;
    bool      hasLanded = false;
    bool      isOverdue = false;



    if (!m_isDropped)
    {
        return false;
    }

    landings  = m_landings.load (std::memory_order_acquire);
    hasLanded = landings != m_dropLandings && m_landedCycle.load (std::memory_order_acquire) == m_dropCycle;
    isOverdue = nowMs - m_dropAtMs >= kSeekPatienceMs;

    m_isDropped = !hasLanded && !isOverdue;

    return m_isDropped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::GetLineTime
//
//  UI thread: the cycle and the host's clock at the line. A line dragged and
//  let go remains where it was dropped until the seek there lands, so it
//  never springs back to the machine's earlier position. IsAtDrop is
//  checked first, so a landing seen comes with the cycle it landed on.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::GetLineTime (
    double      nowMs,
    uint64_t  & outCycle,
    uint64_t  & outWallTime)
{
    bool  isDropped = IsAtDrop (nowMs);



    outCycle    = isDropped ? m_dropCycle : m_cycle.load (std::memory_order_acquire);
    outWallTime = isDropped ? m_wallTimes.GetWallTimeAt (outCycle) : m_wallTime.load (std::memory_order_acquire);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::DropLine
//
//  UI thread: the line remains at `cycle`, where it was dragged, from this
//  move on, until a seek there lands.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryPlayhead::DropLine (
    uint64_t  cycle,
    bool      isFinal,
    double    nowMs)
{
    m_isDropped    = true;
    m_dropCycle    = cycle;
    m_dropLandings = m_landings.load (std::memory_order_acquire);
    m_dropAtMs     = nowMs;

    if (m_onScrub)
    {
        m_onScrub (cycle, isFinal);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::Label
//
//  The labeler's lines for a cycle and the host's clock there. False with
//  no labeler or no lines.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryPlayhead::Label (
    uint64_t        cycle,
    uint64_t        wallTime,
    std::wstring  & outTop,
    std::wstring  & outBottom) const
{
    outTop.clear();
    outBottom.clear();

    if (!m_labeler)
    {
        return false;
    }

    m_labeler (cycle, wallTime, outTop, outBottom);

    return !outTop.empty() || !outBottom.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::LabelAt
//
//  Label at a cycle on the strip's scale, with the host's clock that the
//  wall times return for it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryPlayhead::LabelAt (
    uint64_t        cycle,
    std::wstring  & outTop,
    std::wstring  & outBottom) const
{
    return Label (cycle, m_wallTimes.GetWallTimeAt (cycle), outTop, outBottom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryPlayhead::GetLeadingLabel
//
//  Where history begins, by the labeler's top line, which moves on as the
//  oldest history is dropped.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HistoryPlayhead::GetLeadingLabel() const
{
    std::wstring  top;
    std::wstring  bottom;



    (void) Label (m_beginCycle.load (std::memory_order_acquire), m_beginWall.load (std::memory_order_acquire), top, bottom);

    return top;
}
