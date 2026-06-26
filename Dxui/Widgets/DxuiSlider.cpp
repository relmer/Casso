#include "Pch.h"

#include "Widgets/DxuiSlider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr float     s_kEpsilon           = 1e-6f;
    constexpr uint32_t  s_kDefaultAccentArgb = 0xFF6E9BFF;   // slider accent: track fill + puck core


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

void DxuiSlider::SetRange (float minValue, float maxValue)
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

void DxuiSlider::SetValue (float value)
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

bool DxuiSlider::HitTest (int x, int y) const
{
    // Puck radius (max focused size) extends BEYOND m_boundsDip on the
    // left when value is at min, and on the right when value is at
    // max AND there's no value-readout area to absorb it. Without
    // this extension, the puck visibly reacts to hover/click in its
    // outer half but the outer half is outside m_boundsDip, so the input
    // gets silently dropped.
    constexpr int  s_kPuckRadiusMaxDip = 11;

    int  puckExtPx = 0;


    if (!m_enabled)
    {
        return false;
    }
    puckExtPx = m_scaler.Px (s_kPuckRadiusMaxDip);
    return x >= (m_boundsDip.left  - puckExtPx) &&
           x <  (m_boundsDip.right + puckExtPx) &&
           y >= m_boundsDip.top &&
           y <  m_boundsDip.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ValueFromX
//
////////////////////////////////////////////////////////////////////////////////

float DxuiSlider::ValueFromX (int x) const
{
    constexpr int  s_kValueGapDip   = 8;
    constexpr int  s_kValueWidthDip = 56;

    // Must match the showValue logic in Paint() exactly, otherwise the
    // puck draw position and the click-to-value mapping disagree and
    // a click on the puck snaps to a different value.
    bool   showValue    = m_explicitShowValue ? m_showValue : !m_suffix.empty();
    int    valueAreaPx  = showValue ? (m_scaler.Px (s_kValueWidthDip) + m_scaler.Px (s_kValueGapDip)) : 0;
    int    trackAvailPx = std::max ((LONG) 1, (LONG) ((m_boundsDip.right - m_boundsDip.left) - valueAreaPx));
    float  t            = 0.0f;



    t = (float) (x - m_boundsDip.left) / (float) trackAvailPx;
    t = Clamp (t, 0.0f, 1.0f);

    return m_min + t * (m_max - m_min);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyValue
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::ApplyValue (float v)
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

bool DxuiSlider::OnLButtonDown (int x, int y)
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

bool DxuiSlider::OnLButtonUp (int x, int y)
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

bool DxuiSlider::OnMouseMove (int x, int y)
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

bool DxuiSlider::OnKey (WPARAM vk)
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
//  PaintInternal
//
//  Shared body for both Paint overloads. accentArgb colours the filled
//  track and the puck core -- the theme accent, or the default blue for
//  the non-themed overload.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb) const
{
    constexpr uint32_t  s_kTrack         = 0xFF404040;
    constexpr uint32_t  s_kTick          = 0xFF6A7585;
    constexpr uint32_t  s_kPuckBody      = 0xFFFFFFFF;
    constexpr uint32_t  s_kPuckRing      = 0xFF606060;
    constexpr uint32_t  s_kPuckCoreDis   = 0xFF888888;
    constexpr uint32_t  s_kValueText     = 0xFFE8EEF4;

    // All dimensions stored in dp; scaled to physical pixels via the
    // per-widget DxuiDpiScaler (set by SetDpi). DxuiSlider was previously
    // ignoring DPI which made the value readout illegible at >96dpi.
    constexpr int       s_kTrackHeightDip    = 4;
    constexpr int       s_kPuckRadiusDip     = 8;
    constexpr int       s_kPuckRadiusHovDip  = 10;
    constexpr int       s_kPuckRadiusFocDip  = 11;
    constexpr float     s_kPuckCoreShare    = 0.45f;   // inner-dot diameter as fraction of outer
    constexpr int       s_kTickHeightDip     = 4;
    constexpr int       s_kTickGapDip        = 4;
    constexpr int       s_kValueGapDip       = 8;
    constexpr int       s_kValueFontDip      = 13;
    constexpr int       s_kValueWidthDip     = 56;
    constexpr wchar_t   s_kFont[]           = L"Segoe UI";

    HRESULT  hr            = S_OK;
    bool     showValue     = m_explicitShowValue ? m_showValue : !m_suffix.empty();
    float    trackHeight   = m_scaler.Pxf (s_kTrackHeightDip);
    float    tickHeight    = m_scaler.Pxf (s_kTickHeightDip);
    float    tickGap       = m_scaler.Pxf (s_kTickGapDip);
    float    valueGap      = m_scaler.Pxf (s_kValueGapDip);
    float    valueFontDip  = m_scaler.Pxf (s_kValueFontDip);
    float    valueWidth    = m_scaler.Pxf (s_kValueWidthDip);
    float    valueAreaW    = showValue ? (valueWidth + valueGap) : 0.0f;
    float    rectW         = (float) (m_boundsDip.right  - m_boundsDip.left);
    float    rectH         = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float    trackLeft     = (float) m_boundsDip.left;
    float    trackAvailW   = std::max (0.0f, rectW - valueAreaW);
    float    centerY       = (float) m_boundsDip.top + rectH * 0.5f;
    float    t             = 0.0f;
    float    fillWidth     = 0.0f;
    float    fillLeft      = 0.0f;
    float    trackMid      = 0.0f;
    float    puckCx        = 0.0f;
    float    puckR         = m_scaler.Pxf (s_kPuckRadiusDip);
    uint32_t coreColor     = m_enabled ? accentArgb : s_kPuckCoreDis;



    if (m_max - m_min > s_kEpsilon)
    {
        t = (m_value - m_min) / (m_max - m_min);
    }

    fillWidth = trackAvailW * t;
    puckCx    = trackLeft + trackAvailW * t;
    fillLeft  = trackLeft;
    trackMid  = trackLeft + trackAvailW * 0.5f;

    // Bipolar sliders (e.g. pan) grow the accent fill from the track
    // centre toward the puck rather than from the left edge.
    if (m_centerOriginFill)
    {
        fillLeft  = std::min  (trackMid, puckCx);
        fillWidth = std::fabs (puckCx - trackMid);
    }

    if (m_focused)       { puckR = m_scaler.Pxf (s_kPuckRadiusFocDip); }
    else if (m_hover ||
             m_dragging) { puckR = m_scaler.Pxf (s_kPuckRadiusHovDip); }

    // ----- Track (background + filled portion). -----
    painter.FillRect (trackLeft, centerY - trackHeight * 0.5f,
                      trackAvailW, trackHeight, s_kTrack);
    painter.FillRect (fillLeft, centerY - trackHeight * 0.5f,
                      fillWidth, trackHeight, accentArgb);

    // ----- Tick marks below the track. -----
    if (m_showTicks && m_step > s_kEpsilon && trackAvailW > 0.0f)
    {
        int    tickCount = (int) std::round ((m_max - m_min) / m_step) + 1;
        int    i         = 0;
        float  tickTop   = centerY + trackHeight * 0.5f + tickGap;

        for (i = 0; i < tickCount; i++)
        {
            float  tickT  = (float) i / (float) (tickCount - 1);
            float  tickCx = trackLeft + trackAvailW * tickT;

            painter.FillRect (tickCx - 0.5f, tickTop, 1.0f, tickHeight, s_kTick);
        }
    }

    // ----- Fluent 2 puck: white outer circle with thin grey ring,
    // accent-coloured inner dot. Outer diameter grows on hover/focus.
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckBody);
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckRing); // ring underlay
    painter.FillCircleApprox (puckCx, centerY, puckR - 1.0f,    s_kPuckBody); // white fill, leaving 1px ring
    painter.FillCircleApprox (puckCx, centerY, puckR * s_kPuckCoreShare, coreColor);

    // ----- Value readout to the right of the track. -----
    if (showValue)
    {
        wchar_t  buf[32] = {};

        if (m_formatter)
        {
            std::wstring  formatted = m_formatter (m_value);

            wcsncpy_s (buf, formatted.c_str(), _TRUNCATE);
        }
        else if (m_decimalPlaces > 0)
        {
            wchar_t  fmt[16] = {};
            swprintf_s (fmt, L"%%.%dlf%%ls", m_decimalPlaces);
            swprintf_s (buf, fmt, (double) m_value, m_suffix.c_str());
        }
        else
        {
            int  pct = (int) std::round (m_value);
            swprintf_s (buf, L"%d%ls", pct, m_suffix.c_str());
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (buf,
                                                  trackLeft + trackAvailW + valueGap,
                                                  (float) m_boundsDip.top,
                                                  valueWidth,
                                                  rectH,
                                                  s_kValueText,
                                                  valueFontDip,
                                                  s_kFont,
                                                  DxuiTextHAlign::Right,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::Paint
//
//  Non-themed overload keeps the default blue accent; the IDxuiControl
//  themed override tints the track / puck core from theme.Accent().
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintInternal (painter, text, s_kDefaultAccentArgb);
}




void DxuiSlider::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PaintInternal (painter, text, theme.Accent());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            return OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        }
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        return false;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}
