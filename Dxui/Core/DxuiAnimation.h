#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAnimation
//
//  Tween engine + `DriveSyncEvent` broker. Tweens advance per-frame
//  from a start value to an end value over a fixed duration. The
//  drive-sync broker accepts paired (visual / audio) events and lets
//  the chrome painter and audio mixer pop them within a single frame
//  so the door animation and the door-close sound line up without
//  cross-thread plumbing.
//
////////////////////////////////////////////////////////////////////////////////

enum class DxuiTweenEase
{
    Linear     = 0,
    EaseInOut  = 1,
    EaseOut    = 2,
};


struct DxuiTweenHandle
{
    uint32_t  id = 0;
};


struct DxuiDriveSyncBrokerEvent
{
    int       driveIndex = 0;
    int       tag        = 0;     // caller-defined (e.g. door-open / door-close)
    int64_t   frameTimeMs = 0;
};


class DxuiAnimation
{
public:
    DxuiAnimation  () = default;
    ~DxuiAnimation() = default;

    DxuiTweenHandle  StartTween     (float startValue, float endValue, float durationSec, DxuiTweenEase ease);
    bool         SampleTween    (DxuiTweenHandle handle, float currentTimeSec, float & outValue) const;
    void         AdvanceTime    (float currentTimeSec);
    void         ClearTweens    ();

    void         PublishSyncEvent (const DxuiDriveSyncBrokerEvent & ev);
    std::vector<DxuiDriveSyncBrokerEvent>  ConsumePendingEvents();

    static float ApplyEase      (DxuiTweenEase ease, float t);

private:
    struct DxuiTweenState
    {
        uint32_t   id           = 0;
        float      startValue   = 0.0f;
        float      endValue     = 0.0f;
        float      startTime    = 0.0f;
        float      duration     = 0.0f;
        DxuiTweenEase  ease         = DxuiTweenEase::Linear;
        bool       started      = false;
    };


    std::vector<DxuiTweenState>            m_tweens;
    std::vector<DxuiDriveSyncBrokerEvent>  m_pendingSync;
    float                              m_currentTimeSec = 0.0f;
    uint32_t                           m_nextId         = 1;
};
