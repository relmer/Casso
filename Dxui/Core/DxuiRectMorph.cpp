#include "Pch.h"

#include "Core/DxuiRectMorph.h"
#include "Core/DxuiAnimation.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::Start
//
//  Starts moving from `from` toward `to` at startMs. With animations off, or
//  with either rectangle empty, the morph is done at once and the rectangle
//  is at `to`.
//
//  THE FLAG IS PASSED IN. The caller reads the system's animation setting;
//  this class never reads it, so a morph is the same on every machine.
//
////////////////////////////////////////////////////////////////////////////////

DxuiRectMorph DxuiRectMorph::Start (const RECT & from, const RECT & to, int64_t startMs, bool isAnimated)
{
    DxuiRectMorph  morph;



    morph.m_from       = from;
    morph.m_to         = to;
    morph.m_startMs    = startMs;
    morph.m_isAnimated = isAnimated && !IsEmpty (from) && !IsEmpty (to);

    return morph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::Interpolate
//
//  Each edge `t` of the way from `from` to `to`, rounded to the nearest
//  pixel: `from` at 0 and `to` at 1.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiRectMorph::Interpolate (const RECT & from, const RECT & to, float t)
{
    auto  lerp = [t] (LONG a, LONG b) { return (LONG) std::lround ((float) a + (float) (b - a) * t); };



    return RECT { lerp (from.left,   to.left),
                  lerp (from.top,    to.top),
                  lerp (from.right,  to.right),
                  lerp (from.bottom, to.bottom) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::GetRect
//
//  The rectangle at nowMs: `from` at the start, `to` once the morph is done,
//  and the ease-out curve in between.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiRectMorph::GetRect (int64_t nowMs) const
{
    RECT   rect  = m_to;
    float  t     = 0.0f;
    float  eased = 0.0f;



    if (!IsDone (nowMs))
    {
        t     = std::clamp ((float) (nowMs - m_startMs) / (float) kDurationMs, 0.0f, 1.0f);
        eased = DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, t);
        rect  = Interpolate (m_from, m_to, eased);
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::IsDone
//
//  Done when it does not animate, or once the duration has run out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRectMorph::IsDone (int64_t nowMs) const
{
    return !m_isAnimated || nowMs >= GetEndMs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::GetEndMs
//
//  The time the rectangle is at `to`: the start time when it does not animate.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiRectMorph::GetEndMs() const
{
    return m_isAnimated ? m_startMs + kDurationMs : m_startMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorph::IsEmpty
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRectMorph::IsEmpty (const RECT & rect)
{
    return rect.right <= rect.left || rect.bottom <= rect.top;
}





