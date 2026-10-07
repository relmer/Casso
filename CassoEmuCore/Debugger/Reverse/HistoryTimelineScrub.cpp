#include "Pch.h"

#include "Debugger/Reverse/HistoryTimelineScrub.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineScrub::OnDragged
//
//  The first move of a drag notes whether the machine was running, and a
//  running one is stopped first. A move only notes where the line is; the
//  frame seeks there, so however many moves a frame brings, it seeks once.
//  Letting go seeks at once, busy or not, so the machine lands exactly
//  where the line was let go.
//
////////////////////////////////////////////////////////////////////////////////

HistoryTimelineScrubStep HistoryTimelineScrub::OnDragged (
    uint64_t  cycle,
    bool      isFinal,
    bool      isPaused,
    bool      isSeekBusy)
{
    HistoryTimelineScrubStep  step;
    bool                      isFirst = !m_isActive;



    if (isFirst)
    {
        m_isActive   = true;
        m_isEnded    = false;
        m_wasRunning = !isPaused;
        m_lastSought = UINT64_MAX;
    }

    m_pending = cycle;
    m_isEnded = m_isEnded || isFinal;

    if (isFinal)
    {
        step = TakeStep (isPaused, isSeekBusy);
    }

    step.pauseFirst = isFirst && !isPaused;

    return step;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineScrub::OnFrame
//
////////////////////////////////////////////////////////////////////////////////

HistoryTimelineScrubStep HistoryTimelineScrub::OnFrame (
    bool  isPaused,
    bool  isSeekBusy)
{
    if (!m_isActive)
    {
        return HistoryTimelineScrubStep();
    }

    return TakeStep (isPaused, isSeekBusy);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimelineScrub::TakeStep
//
//  Once the machine is stopped: a seek to where the line last was, when it
//  has moved since the last seek and that seek has landed, or when it was
//  let go; and once let go, the drag ends, and a machine that was running
//  runs on. Every seek before the end is interim. An end that needs no seek
//  is settled where the last seek went.
//
////////////////////////////////////////////////////////////////////////////////

HistoryTimelineScrubStep HistoryTimelineScrub::TakeStep (
    bool  isPaused,
    bool  isSeekBusy)
{
    HistoryTimelineScrubStep  step;
    bool                      isMoved = m_pending.has_value() && *m_pending != m_lastSought;



    if (!isPaused || (isSeekBusy && !m_isEnded))
    {
        return step;
    }

    if (isMoved)
    {
        step.seek      = true;
        step.isInterim = !m_isEnded;
        step.cycle     = *m_pending;
        m_lastSought   = *m_pending;
    }

    m_pending.reset();

    if (m_isEnded)
    {
        step.isSettled = !step.seek && m_lastSought != UINT64_MAX;
        step.cycle     = m_lastSought;
        step.run       = m_wasRunning;
        m_isActive     = false;
    }

    return step;
}





