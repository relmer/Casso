#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAnimation
//
//  Tween engine. Tweens advance per-frame from a start value to an end
//  value over a fixed duration.
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


class DxuiAnimation
{
public:
    DxuiAnimation  ();
    ~DxuiAnimation() = default;

    DxuiTweenHandle  StartTween     (float startValue, float endValue, float durationSec, DxuiTweenEase ease);
    bool         SampleTween    (DxuiTweenHandle handle, float currentTimeSec, float & outValue) const;
    void         AdvanceTime    (float currentTimeSec);
    void         ClearTweens    ();

    static float ApplyEase      (DxuiTweenEase ease, float t);

    //  Whether tweens actually play. Seeded from the system's animation
    //  setting; a caller that animates regardless, or a test that needs a
    //  tween to run whatever the host is configured for, sets it.
    void  SetAnimationsEnabled (bool on) { m_animationsEnabled = on; }
    bool  AreAnimationsEnabled () const  { return m_animationsEnabled; }

private:
    struct DxuiTweenState
    {
        uint32_t       id         = 0;
        float          startValue = 0.0f;
        float          endValue   = 0.0f;
        float          startTime  = 0.0f;
        float          duration   = 0.0f;
        DxuiTweenEase  ease       = DxuiTweenEase::Linear;
        bool           started    = false;
    };


    std::vector<DxuiTweenState>            m_tweens;
    float                                  m_currentTimeSec    = 0.0f;
    bool                                   m_animationsEnabled = true;
    uint32_t                               m_nextId            = 1;
};
