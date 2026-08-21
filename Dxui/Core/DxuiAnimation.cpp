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
//  Evaluates a running tween at a given time.
//
//  Sampling is a PURE function of the tween's start time, duration, and the
//  time passed in -- nothing is advanced or stored. Two callers sampling the
//  same handle in one frame get the same answer, and a dropped frame does not
//  desynchronize the animation from wall time the way per-frame stepping does.
//
//  Time is passed in rather than read, so animation is deterministic under
//  test.
//
//  The three edge cases are clamped BEFORE the interpolation, which is what
//  lets the interpolation itself assume 0 < t < 1: a zero-duration tween is
//  already at its end, a sample before the start time has not begun, and one
//  past the end is finished. Without those, a zero duration divides by zero
//  and an easing curve is evaluated outside its domain.
//
//  Returning false for an unknown handle lets a caller sample a tween that may
//  already have been retired without checking first.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiAnimation::SampleTween (
    DxuiTweenHandle  handle,
    float        currentTimeSec,
    float      & outValue) const
{
    float  elapsed = 0.0f;
    float  t       = 0.0f;
    float  eased   = 0.0f;
    bool   found   = false;



    // Ids are unique, so the first match is the answer; `found` stops the
    // scan rather than a break, since the sampling below shares the frame.
    for (const DxuiTweenState & s : m_tweens)
    {
        if (!found && s.id == handle.id)
        {
            found   = true;
            elapsed = currentTimeSec - s.startTime;

            // A zero-duration tween is already at its end; a sample taken
            // before the start time has not begun. Clamping both ends here
            // means the interpolation below only ever sees 0 < t < 1.
            if (s.duration <= 0.0f)
            {
                outValue = s.endValue;
            }
            else if (elapsed <= 0.0f)
            {
                outValue = s.startValue;
            }
            else
            {
                t = elapsed / s.duration;

                if (t >= 1.0f)
                {
                    outValue = s.endValue;
                }
                else
                {
                    eased    = ApplyEase (s.ease, t);
                    outValue = s.startValue + (s.endValue - s.startValue) * eased;
                }
            }
        }
    }

    return found;
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
    float  eased = t;      // Linear, and the fallback for an unknown ease



    switch (ease)
    {
    case DxuiTweenEase::Linear:
        break;

    case DxuiTweenEase::EaseOut:
        eased = 1.0f - (1.0f - t) * (1.0f - t);
        break;

    case DxuiTweenEase::EaseInOut:
        // Two mirrored quadratics meeting at the midpoint: ease-in over the
        // first half, ease-out over the second.
        eased = (t < 0.5f) ? (2.0f * t * t)
                           : (1.0f - 2.0f * (1.0f - t) * (1.0f - t));
        break;
    }

    return eased;
}
