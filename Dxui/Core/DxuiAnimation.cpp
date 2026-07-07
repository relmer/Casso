#include "Pch.h"

#include "DxuiAnimation.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StartTween
//
////////////////////////////////////////////////////////////////////////////////

DxuiTweenHandle DxuiAnimation::StartTween (
    float      startValue,
    float      endValue,
    float      durationSec,
    DxuiTweenEase  ease)
{
    DxuiTweenState   state;
    DxuiTweenHandle  handle;



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

bool DxuiAnimation::SampleTween (
    DxuiTweenHandle  handle,
    float        currentTimeSec,
    float      & outValue) const
{
    for (const DxuiTweenState & s : m_tweens)
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

void DxuiAnimation::AdvanceTime (float currentTimeSec)
{
    m_currentTimeSec = currentTimeSec;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearTweens
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAnimation::ClearTweens()
{
    m_tweens.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishSyncEvent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAnimation::PublishSyncEvent (const DxuiDriveSyncBrokerEvent & ev)
{
    m_pendingSync.push_back (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsumePendingEvents
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDriveSyncBrokerEvent> DxuiAnimation::ConsumePendingEvents()
{
    std::vector<DxuiDriveSyncBrokerEvent>  out = std::move (m_pendingSync);

    m_pendingSync.clear();
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyEase
//
////////////////////////////////////////////////////////////////////////////////

float DxuiAnimation::ApplyEase (DxuiTweenEase ease, float t)
{
    switch (ease)
    {
    case DxuiTweenEase::Linear:
        return t;

    case DxuiTweenEase::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);

    case DxuiTweenEase::EaseInOut:
        if (t < 0.5f)
        {
            return 2.0f * t * t;
        }
        return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }

    return t;
}
