#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Animation
//
//  Tween engine + `DriveSyncEvent` broker. Tweens advance per-frame
//  from a start value to an end value over a fixed duration. The
//  drive-sync broker accepts paired (visual / audio) events and lets
//  the chrome painter and audio mixer pop them within a single frame
//  so the door animation and the door-close sound line up without
//  cross-thread plumbing.
//
////////////////////////////////////////////////////////////////////////////////

enum class TweenEase
{
    Linear     = 0,
    EaseInOut  = 1,
    EaseOut    = 2,
};


struct TweenHandle
{
    uint32_t  id = 0;
};


struct DriveSyncBrokerEvent
{
    int       driveIndex = 0;
    int       tag        = 0;     // caller-defined (e.g. door-open / door-close)
    int64_t   frameTimeMs = 0;
};


class Animation
{
public:
    Animation  () = default;
    ~Animation () = default;

    TweenHandle  StartTween     (float startValue, float endValue, float durationSec, TweenEase ease);
    bool         SampleTween    (TweenHandle handle, float currentTimeSec, float & outValue) const;
    void         AdvanceTime    (float currentTimeSec);
    void         ClearTweens    ();

    void         PublishSyncEvent (const DriveSyncBrokerEvent & ev);
    std::vector<DriveSyncBrokerEvent>  ConsumePendingEvents ();

    static float ApplyEase      (TweenEase ease, float t);

private:
    struct TweenState
    {
        uint32_t   id           = 0;
        float      startValue   = 0.0f;
        float      endValue     = 0.0f;
        float      startTime    = 0.0f;
        float      duration     = 0.0f;
        TweenEase  ease         = TweenEase::Linear;
        bool       started      = false;
    };


    std::vector<TweenState>            m_tweens;
    std::vector<DriveSyncBrokerEvent>  m_pendingSync;
    float                              m_currentTimeSec = 0.0f;
    uint32_t                           m_nextId         = 1;
};
