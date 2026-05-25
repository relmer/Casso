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
    bool  startedDrag = false;


    if (!HitTest (x, y))
    {
        return false;
    }

    startedDrag = !m_dragging;
    m_dragging  = true;
    ApplyValue (ValueFromX (x));

    if (startedDrag && m_onDragStart)
    {
        m_onDragStart();
    }
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
    bool  endedDrag = m_dragging;


    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);

    m_dragging = false;

    if (endedDrag && m_onDragEnd)
    {
        m_onDragEnd();
    }
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
    bool           consumed = false;



    if (!m_enabled || !m_focused)
    {
        return false;
    }

    switch (vk)
    {
        case VK_LEFT:
        case VK_DOWN:
            ApplyValue (m_value - m_step);
            consumed = true;
            break;

        case VK_RIGHT:
        case VK_UP:
            ApplyValue (m_value + m_step);
            consumed = true;
            break;

        case VK_PRIOR:
            ApplyValue (m_value + m_step * (float) s_kPageSteps);
            consumed = true;
            break;

        case VK_NEXT:
            ApplyValue (m_value - m_step * (float) s_kPageSteps);
            consumed = true;
            break;

        case VK_HOME:
            ApplyValue (m_min);
            consumed = true;
            break;

        case VK_END:
            ApplyValue (m_max);
            consumed = true;
            break;

        default:
            break;
    }

    if (consumed && m_onKeyboard)
    {
        m_onKeyboard();
    }
    return consumed;
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
    constexpr uint32_t  s_kTick          = 0xFF6A7585;
    constexpr uint32_t  s_kPuckBody      = 0xFFFFFFFF;
    constexpr uint32_t  s_kPuckCore      = 0xFF6E9BFF;
    constexpr uint32_t  s_kPuckRing      = 0xFF606060;
    constexpr uint32_t  s_kPuckCoreDis   = 0xFF888888;
    constexpr uint32_t  s_kValueText     = 0xFFE8EEF4;
    constexpr float     s_kTrackHeight   = 4.0f;
    constexpr float     s_kPuckRadius    = 8.0f;
    constexpr float     s_kPuckRadiusHov = 10.0f;
    constexpr float     s_kPuckRadiusFoc = 11.0f;
    constexpr float     s_kPuckCoreShare = 0.45f;   // inner-dot diameter as fraction of outer
    constexpr float     s_kTickHeight    = 4.0f;
    constexpr float     s_kTickGap       = 4.0f;
    constexpr float     s_kValueGapPx    = 8.0f;
    constexpr float     s_kValueFontDip  = 12.0f;
    constexpr float     s_kValueWidthPx  = 48.0f;
    constexpr wchar_t   s_kFont[]        = L"Segoe UI";

    HRESULT  hr            = S_OK;
    float    rectW         = (float) (m_rect.right  - m_rect.left);
    float    rectH         = (float) (m_rect.bottom - m_rect.top);
    float    valueW        = s_kValueWidthPx;
    float    trackLeft     = (float) m_rect.left;
    float    trackAvailW   = std::max (0.0f, rectW - (valueW + s_kValueGapPx));
    float    centerY       = (float) m_rect.top + rectH * 0.5f;
    float    t             = 0.0f;
    float    fillWidth     = 0.0f;
    float    puckCx        = 0.0f;
    float    puckR         = m_enabled ? s_kPuckRadius : s_kPuckRadius;
    uint32_t coreColor     = m_enabled ? s_kPuckCore : s_kPuckCoreDis;



    if (m_max - m_min > s_kEpsilon)
    {
        t = (m_value - m_min) / (m_max - m_min);
    }

    fillWidth = trackAvailW * t;
    puckCx    = trackLeft + trackAvailW * t;

    if (m_focused)       { puckR = s_kPuckRadiusFoc; }
    else if (m_hover ||
             m_dragging) { puckR = s_kPuckRadiusHov; }

    // ----- Track (background + filled portion). -----
    painter.FillRect (trackLeft, centerY - s_kTrackHeight * 0.5f,
                      trackAvailW, s_kTrackHeight, s_kTrack);
    painter.FillRect (trackLeft, centerY - s_kTrackHeight * 0.5f,
                      fillWidth, s_kTrackHeight, s_kTrackFill);

    // ----- Tick marks below the track. -----
    if (m_showTicks && m_step > s_kEpsilon && trackAvailW > 0.0f)
    {
        int    tickCount = (int) std::round ((m_max - m_min) / m_step) + 1;
        int    i         = 0;
        float  tickTop   = centerY + s_kTrackHeight * 0.5f + s_kTickGap;

        for (i = 0; i < tickCount; i++)
        {
            float  tickT  = (float) i / (float) (tickCount - 1);
            float  tickCx = trackLeft + trackAvailW * tickT;

            painter.FillRect (tickCx - 0.5f, tickTop, 1.0f, s_kTickHeight, s_kTick);
        }
    }

    // ----- Fluent 2 puck: white outer circle with thin grey ring,
    // accent-coloured inner dot. Outer diameter grows on hover/focus.
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckBody);
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckRing); // ring underlay
    painter.FillCircleApprox (puckCx, centerY, puckR - 1.0f,    s_kPuckBody); // white fill, leaving 1px ring
    painter.FillCircleApprox (puckCx, centerY, puckR * s_kPuckCoreShare, coreColor);

    // ----- Value readout to the right of the track. -----
    {
        wchar_t  buf[32] = {};
        int      pct     = (int) std::round (m_value);

        if (!m_suffix.empty())
        {
            swprintf_s (buf, L"%d%ls", pct, m_suffix.c_str());
        }
        else
        {
            swprintf_s (buf, L"%d", pct);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (buf,
                                                  trackLeft + trackAvailW + s_kValueGapPx,
                                                  (float) m_rect.top,
                                                  valueW,
                                                  rectH,
                                                  s_kValueText,
                                                  s_kValueFontDip,
                                                  s_kFont,
                                                  DwriteTextRenderer::HAlign::Right,
                                                  DwriteTextRenderer::VAlign::Center));
    }
}
