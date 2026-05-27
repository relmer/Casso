#include "Pch.h"

#include "Animation.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartTween
//
////////////////////////////////////////////////////////////////////////////////

TweenHandle Animation::StartTween (
    float      startValue,
    float      endValue,
    float      durationSec,
    TweenEase  ease)
{
    TweenState   state;
    TweenHandle  handle;



    state.id         = m_nextId++;
    state.startValue = startValue;
    state.endValue   = endValue;
    state.startTime  = m_currentTimeSec;
    state.duration   = (durationSec > 0.0f) ? durationSec : 0.0f;
    state.ease       = ease;
    state.started    = true;

    m_tweens.push_back (state);

    handle.id = state.id;
    return handle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SampleTween
//
////////////////////////////////////////////////////////////////////////////////

bool Animation::SampleTween (
    TweenHandle  handle,
    float        currentTimeSec,
    float      & outValue) const
{
    for (const TweenState & s : m_tweens)
    {
        float  elapsed = 0.0f;
        float  t       = 0.0f;
        float  eased   = 0.0f;

        if (s.id != handle.id)
        {
            continue;
        }

        if (s.duration <= 0.0f)
        {
            outValue = s.endValue;
            return true;
        }

        elapsed = currentTimeSec - s.startTime;

        if (elapsed <= 0.0f)
        {
            outValue = s.startValue;
            return true;
        }

        t = elapsed / s.duration;

        if (t >= 1.0f)
        {
            outValue = s.endValue;
            return true;
        }

        eased    = ApplyEase (s.ease, t);
        outValue = s.startValue + (s.endValue - s.startValue) * eased;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceTime
//
////////////////////////////////////////////////////////////////////////////////

void Animation::AdvanceTime (float currentTimeSec)
{
    m_currentTimeSec = currentTimeSec;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearTweens
//
////////////////////////////////////////////////////////////////////////////////

void Animation::ClearTweens ()
{
    m_tweens.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishSyncEvent
//
////////////////////////////////////////////////////////////////////////////////

void Animation::PublishSyncEvent (const DriveSyncBrokerEvent & ev)
{
    m_pendingSync.push_back (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumePendingEvents
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DriveSyncBrokerEvent> Animation::ConsumePendingEvents ()
{
    std::vector<DriveSyncBrokerEvent>  out = std::move (m_pendingSync);

    m_pendingSync.clear();
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyEase
//
////////////////////////////////////////////////////////////////////////////////

float Animation::ApplyEase (TweenEase ease, float t)
{
    switch (ease)
    {
    case TweenEase::Linear:
        return t;

    case TweenEase::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);

    case TweenEase::EaseInOut:
        if (t < 0.5f)
        {
            return 2.0f * t * t;
        }
        return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }

    return t;
}
