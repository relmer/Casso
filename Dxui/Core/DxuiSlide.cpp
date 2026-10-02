#include "Pch.h"

#include "Core/DxuiSlide.h"
#include "Core/DxuiAnimation.h"
#include "Widgets/DxuiPopupMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlide::Start
//
//  Starts moving distancePx toward zero at startMs. The time and the curve
//  are the ones a menu opens with, so everything that moves in the chrome
//  moves the same way. With animations off the slide is done at once and the
//  thing it moves jumps to its place.
//
//  THE FLAG IS PASSED IN. The caller reads the system's animation setting;
//  this never does, so a test sees the same slide on every machine.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSlide DxuiSlide::Start (float distancePx, int64_t startMs, bool isAnimated)
{
    DxuiSlide  slide;



    slide.m_distancePx = distancePx;
    slide.m_startMs    = startMs;
    slide.m_isAnimated = isAnimated;

    return slide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlide::GetOffset
//
//  The distance still to travel at nowMs: all of it at the start, none of it
//  once the slide is done, and the ease-out curve in between.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiSlide::GetOffset (int64_t nowMs) const
{
    float  offset = 0.0f;
    float  t      = 0.0f;
    float  eased  = 0.0f;



    if (!IsDone (nowMs))
    {
        t      = std::clamp ((float) (nowMs - m_startMs) / (float) DxuiPopupMenu::kRevealMs, 0.0f, 1.0f);
        eased  = DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, t);
        offset = m_distancePx * (1.0f - eased);
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlide::IsDone
//
//  Done with nothing to travel, with animations off, or once the duration
//  has run out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlide::IsDone (int64_t nowMs) const
{
    return !m_isAnimated || m_distancePx == 0.0f || nowMs >= GetEndMs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlide::GetEndMs
//
//  When the slide reaches zero: its start when it does not animate.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiSlide::GetEndMs() const
{
    return m_isAnimated ? m_startMs + DxuiPopupMenu::kRevealMs : m_startMs;
}
