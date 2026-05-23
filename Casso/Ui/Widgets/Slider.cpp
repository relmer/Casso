#include "Pch.h"

#include "Slider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr float  s_kEpsilon = 1e-6f;


    float  Clamp (float v, float lo, float hi)
    {
        if (v < lo) { return lo; }
        if (v > hi) { return hi; }
        return v;
    }


    float  QuantizeToStep (float value, float minValue, float step)
    {
        float  raw = 0.0f;
        float  q   = 0.0f;



        if (step <= s_kEpsilon)
        {
            return value;
        }

        raw = (value - minValue) / step;
        q   = std::round (raw) * step + minValue;
        return q;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRange
//
////////////////////////////////////////////////////////////////////////////////

void Slider::SetRange (float minValue, float maxValue)
{
    if (maxValue < minValue)
    {
        std::swap (minValue, maxValue);
    }

    m_min = minValue;
    m_max = maxValue;

    if (m_value < m_min) { m_value = m_min; }
    if (m_value > m_max) { m_value = m_max; }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetValue
//
////////////////////////////////////////////////////////////////////////////////

void Slider::SetValue (float value)
{
    float  v = QuantizeToStep (value, m_min, m_step);



    v = Clamp (v, m_min, m_max);
    m_value = v;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool Slider::HitTest (int x, int y) const
{
    if (!m_enabled)
    {
        return false;
    }
    return x >= m_rect.left && x < m_rect.right && y >= m_rect.top && y < m_rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void Slider::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueFromX
//
////////////////////////////////////////////////////////////////////////////////

float Slider::ValueFromX (int x) const
{
    int    width = m_rect.right - m_rect.left;
    float  t     = 0.0f;



    if (width <= 0)
    {
        return m_min;
    }

    t = (float) (x - m_rect.left) / (float) width;
    t = Clamp (t, 0.0f, 1.0f);

    return m_min + t * (m_max - m_min);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyValue
//
////////////////////////////////////////////////////////////////////////////////

void Slider::ApplyValue (float v)
{
    float  q       = QuantizeToStep (v, m_min, m_step);
    float  clamped = Clamp (q, m_min, m_max);
    bool   changed = std::fabs (clamped - m_value) > s_kEpsilon;



    m_value = clamped;

    if (changed && m_change)
    {
        m_change (m_value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool Slider::OnLButtonDown (int x, int y)
{
    if (!HitTest (x, y))
    {
        return false;
    }

    m_dragging = true;
    ApplyValue (ValueFromX (x));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool Slider::OnLButtonUp (int x, int y)
{
    bool  consumed = m_dragging;



    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    m_dragging = false;
    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool Slider::OnMouseMove (int x, int y)
{
    if (!m_dragging)
    {
        return false;
    }

    UNREFERENCED_PARAMETER (y);
    ApplyValue (ValueFromX (x));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool Slider::OnKey (WPARAM vk)
{
    constexpr int  s_kPageSteps = 10;



    if (!m_enabled || !m_focused)
    {
        return false;
    }

    switch (vk)
    {
        case VK_LEFT:
        case VK_DOWN:
            ApplyValue (m_value - m_step);
            return true;

        case VK_RIGHT:
        case VK_UP:
            ApplyValue (m_value + m_step);
            return true;

        case VK_PRIOR:
            ApplyValue (m_value + m_step * (float) s_kPageSteps);
            return true;

        case VK_NEXT:
            ApplyValue (m_value - m_step * (float) s_kPageSteps);
            return true;

        case VK_HOME:
            ApplyValue (m_min);
            return true;

        case VK_END:
            ApplyValue (m_max);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void Slider::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr uint32_t  s_kTrack         = 0xFF404040;
    constexpr uint32_t  s_kTrackFill     = 0xFF6E9BFF;
    constexpr uint32_t  s_kThumb         = 0xFFE8EEF4;
    constexpr uint32_t  s_kThumbDisabled = 0xFF707070;
    constexpr uint32_t  s_kFocusRing     = 0xFFAACCFF;
    constexpr float     s_kTrackHeight   = 4.0f;
    constexpr float     s_kThumbWidth    = 10.0f;
    constexpr float     s_kThumbHeight   = 16.0f;
    constexpr float     s_kFocusThick    = 1.0f;

    float    centerY    = (float) m_rect.top + ((float) (m_rect.bottom - m_rect.top)) * 0.5f;
    float    trackLeft  = (float) m_rect.left;
    float    trackWidth = (float) (m_rect.right - m_rect.left);
    float    t          = 0.0f;
    float    fillWidth  = 0.0f;
    float    thumbX     = 0.0f;
    uint32_t thumbColor = m_enabled ? s_kThumb : s_kThumbDisabled;



    UNREFERENCED_PARAMETER (text);

    if (m_max - m_min > s_kEpsilon)
    {
        t = (m_value - m_min) / (m_max - m_min);
    }

    fillWidth = trackWidth * t;
    thumbX    = trackLeft + trackWidth * t - s_kThumbWidth * 0.5f;

    painter.FillRect (trackLeft, centerY - s_kTrackHeight * 0.5f,
                      trackWidth, s_kTrackHeight, s_kTrack);
    painter.FillRect (trackLeft, centerY - s_kTrackHeight * 0.5f,
                      fillWidth, s_kTrackHeight, s_kTrackFill);

    painter.FillRect (thumbX, centerY - s_kThumbHeight * 0.5f,
                      s_kThumbWidth, s_kThumbHeight, thumbColor);

    if (m_focused)
    {
        painter.OutlineRect (thumbX - 2.0f, centerY - s_kThumbHeight * 0.5f - 2.0f,
                             s_kThumbWidth + 4.0f, s_kThumbHeight + 4.0f,
                             s_kFocusThick, s_kFocusRing);
    }
}
