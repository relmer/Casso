#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiSlider.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local helpers
//
////////////////////////////////////////////////////////////////////////////////

static constexpr float     s_kEpsilon           = 1e-6f;


float  DxuiSlider::Clamp (float v, float lo, float hi)
{
    float  clamped = v;



    if      (v < lo) { clamped = lo; }
    else if (v > hi) { clamped = hi; }

    return clamped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuantizeToStep
//
////////////////////////////////////////////////////////////////////////////////

float  DxuiSlider::QuantizeToStep (float value, float minValue, float step)
{
    float  raw = 0.0f;
    float  q   = value;



    if (step > s_kEpsilon)
    {
        raw = (value - minValue) / step;
        q   = std::round (raw) * step + minValue;
    }

    return q;
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
    // Quantize to the drag granularity (== the keyboard step unless a finer
    // drag step is set) so a value dialed in by dragging survives a reseed
    // instead of snapping back to the coarse keyboard step.
    float  v = QuantizeToStep (value, m_min, DragStep());



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



    int   puckExtPx = 0;
    bool  isHit     = false;


    if (m_enabled)
    {
        puckExtPx = m_scaler.Px (s_kPuckRadiusMaxDip);

        // The puck bulges past the bounds along the TRACK axis; the cross
        // axis stays exact so neighbors are not shadowed.
        if (m_vertical)
        {
            isHit = x >= m_boundsDip.left &&
                    x <  m_boundsDip.right &&
                    y >= (m_boundsDip.top    - puckExtPx) &&
                    y <  (m_boundsDip.bottom + puckExtPx);
        }
        else
        {
            isHit = x >= (m_boundsDip.left  - puckExtPx) &&
                    x <  (m_boundsDip.right + puckExtPx) &&
                    y >= m_boundsDip.top &&
                    y <  m_boundsDip.bottom;
        }
    }

    return isHit;
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
//  ValueFromY
//
//  The vertical counterpart: max at the TOP, so t runs from the bottom of
//  the track upward. The value readout sits under the track in this
//  orientation, so its reserved area comes off the bottom -- and as with
//  ValueFromX, this must match the paint geometry exactly or a click on the
//  puck snaps the value.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiSlider::ValueFromY (int y) const
{
    constexpr int  s_kValueGapDip    = 6;
    constexpr int  s_kValueHeightDip = 18;



    bool   showValue    = m_explicitShowValue ? m_showValue : !m_suffix.empty();
    int    valueAreaPx  = showValue ? (m_scaler.Px (s_kValueHeightDip) + m_scaler.Px (s_kValueGapDip)) : 0;
    int    trackAvailPx = std::max ((LONG) 1, (LONG) ((m_boundsDip.bottom - m_boundsDip.top) - valueAreaPx));
    float  t            = 0.0f;



    t = (float) ((m_boundsDip.top + trackAvailPx) - y) / (float) trackAvailPx;
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
    ApplyValueWithStep (v, m_step);
}


void DxuiSlider::ApplyValueWithStep (float v, float step)
{
    float  q       = QuantizeToStep (v, m_min, step);
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
    bool  isHit       = HitTest (x, y);



    if (isHit)
    {
        startedDrag = !m_dragging;
        m_dragging  = true;
        ApplyValueWithStep (ValueFromPoint (x, y), DragStep());

        if (startedDrag && m_onDragStart)
        {
            m_onDragStart();
        }
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnLButtonUp (int x, int y)
{
    bool  consumed  = m_dragging;
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
    bool  isDragging = m_dragging;



    if (isDragging)
    {
        ApplyValueWithStep (ValueFromPoint (x, y), DragStep());
    }

    return isDragging;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Keyboard adjustment: the standard slider bindings.
//
//  Both axes are bound -- Left/Down decrease, Right/Up increase -- because the
//  widget does not know whether its host laid it out horizontally or
//  vertically, and binding one axis would leave half the layouts unusable.
//
//  Page Up and Page Down move ten steps, so a slider with a fine step is still
//  crossable from the keyboard without holding an arrow down. Home and End go
//  straight to the rails.
//
//  Everything routes through ApplyValue, so clamping, snapping, and the change
//  notification are identical to what a drag produces -- the keyboard cannot
//  reach a value the mouse could not.
//
//  The keyboard callback fires only on a CONSUMED key, and it is separate from
//  the value change: hosts use it to distinguish deliberate keyboard
//  adjustment from a drag, typically to keep a focus indicator alive.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnKey (WPARAM vk)
{
    constexpr int  s_kPageSteps = 10;
    bool           consumed     = false;
    bool           isActive     = m_enabled && m_focused;



    if (isActive)
    {
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
//  Shared body for both Paint overloads. accentArgb colors the filled
//  track and the puck core -- the theme accent, or the default blue for
//  the non-themed overload.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  s_kInactiveTrackContrast = 1.6f;   // inactive track must stand off the panel bg
    float            tickStep                 = 0.0f;



    if (m_vertical)
    {
        PaintVerticalInternal (painter, text, theme);
        return;
    }

    uint32_t  accentArgb     = theme.Accent();
    uint32_t  s_kTrack       = DxuiColor::TintForContrast (theme.Background(), s_kInactiveTrackContrast);
    uint32_t  s_kTick        = theme.ForegroundMuted();
    uint32_t  s_kPuckBody    = 0xFFFFFFFF;
    uint32_t  s_kPuckRing    = theme.Border();
    uint32_t  s_kPuckCoreDis = theme.ForegroundDisabled();
    uint32_t  s_kValueText   = theme.Foreground();

    // All dimensions stored in dp; scaled to physical pixels via the
    // per-widget DxuiDpiScaler (set by SetDpi). DxuiSlider was previously
    // ignoring DPI which made the value readout illegible at >96dpi.
    constexpr int              s_kTrackHeightDip   = 4;
    constexpr int              s_kPuckRadiusDip    = 8;
    constexpr int              s_kPuckRadiusHovDip = 10;
    constexpr int              s_kPuckRadiusFocDip = 11;
    constexpr float            s_kPuckCoreShare    = 0.45f;   // inner-dot diameter as fraction of outer
    constexpr float            s_kPuckCoreRatio    = 3.0f;   // WCAG 1.4.11 min contrast of core vs white body
    constexpr int              s_kTickHeightDip    = 4;
    constexpr int              s_kTickGapDip       = 4;
    constexpr int              s_kValueGapDip      = 8;
    constexpr int              s_kValueFontDip     = 13;
    constexpr int              s_kValueWidthDip    = 56;
    constexpr const wchar_t  * s_kFont             = DxuiTheme::kBodyFace;

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
    uint32_t coreColor     = m_enabled ? DxuiColor::AccentForWhiteContrast (accentArgb, s_kPuckCoreRatio) : s_kPuckCoreDis;



    if (m_max - m_min > s_kEpsilon)
    {
        t = (m_value - m_min) / (m_max - m_min);
    }

    fillWidth = trackAvailW * t;
    puckCx    = trackLeft + trackAvailW * t;
    fillLeft  = trackLeft;
    trackMid  = trackLeft + trackAvailW * 0.5f;

    // Bipolar sliders (e.g. pan) grow the accent fill from the track
    // center toward the puck rather than from the left edge.
    if (m_centerOriginFill)
    {
        fillLeft  = std::min  (trackMid, puckCx);
        fillWidth = std::fabs (puckCx - trackMid);
    }

    if (m_focused)       { puckR = m_scaler.Pxf (s_kPuckRadiusFocDip); }
    else if (m_hover ||
             m_dragging) { puckR = m_scaler.Pxf (s_kPuckRadiusHovDip); }

    // Track (background + filled portion).
    painter.FillRect (trackLeft, centerY - trackHeight * 0.5f,
                      trackAvailW, trackHeight, s_kTrack);
    painter.FillRect (fillLeft, centerY - trackHeight * 0.5f,
                      fillWidth, trackHeight, accentArgb);

    // Tick marks below the track.
    tickStep = (m_tickInterval > s_kEpsilon) ? m_tickInterval : m_step;

    if (m_showTicks && tickStep > s_kEpsilon && trackAvailW > 0.0f)
    {
        int    tickCount = (int) std::round ((m_max - m_min) / tickStep) + 1;
        int    i         = 0;
        float  tickTop   = centerY + trackHeight * 0.5f + tickGap;

        for (i = 0; i < tickCount; i++)
        {
            float  tickT  = (float) i / (float) (tickCount - 1);
            float  tickCx = trackLeft + trackAvailW * tickT;

            painter.FillRect (tickCx - 0.5f, tickTop, 1.0f, tickHeight, s_kTick);
        }
    }

    // Fluent 2 puck: white outer circle with thin gray ring,
    // accent-colored inner dot. Outer diameter grows on hover/focus.
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckBody);
    painter.FillCircleApprox (puckCx, centerY, puckR,           s_kPuckRing); // ring underlay
    painter.FillCircleApprox (puckCx, centerY, puckR - 1.0f,    s_kPuckBody); // white fill, leaving 1px ring
    painter.FillCircleApprox (puckCx, centerY, puckR * s_kPuckCoreShare, coreColor);

    // Value readout to the right of the track.
    if (showValue)
    {
        wchar_t  buf[32] = {};

        FormatValue (buf);

        hr = text.DrawString (buf,
                              trackLeft + trackAvailW + valueGap,
                              (float) m_boundsDip.top,
                              valueWidth,
                              rectH,
                              s_kValueText,
                              valueFontDip,
                              s_kFont,
                              DxuiTextHAlign::Right,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatValue
//
//  One formatter for both orientations: custom formatter, fixed decimals, or
//  integer + suffix -- exactly what the horizontal readout always printed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::FormatValue (wchar_t (&buf)[32]) const
{
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintVerticalInternal
//
//  The vertical body: track centered on the bounds' X, filled from the
//  BOTTOM up to the puck (max at the top, the fader convention), ticks to
//  the track's right, and the value readout centered UNDER the track --
//  which is where a narrow flyout has room for it. Geometry mirrors
//  ValueFromY exactly; the two disagreeing is the classic snapping bug.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::PaintVerticalInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    constexpr float  s_kInactiveTrackContrast = 1.6f;



    uint32_t  accentArgb     = theme.Accent();
    uint32_t  s_kTrack       = DxuiColor::TintForContrast (theme.Background(), s_kInactiveTrackContrast);
    uint32_t  s_kTick        = theme.ForegroundMuted();
    uint32_t  s_kPuckBody    = 0xFFFFFFFF;
    uint32_t  s_kPuckRing    = theme.Border();
    uint32_t  s_kPuckCoreDis = theme.ForegroundDisabled();
    uint32_t  s_kValueText   = theme.Foreground();

    constexpr int              s_kTrackWidthDip    = 4;
    constexpr int              s_kPuckRadiusDip    = 8;
    constexpr int              s_kPuckRadiusHovDip = 10;
    constexpr int              s_kPuckRadiusFocDip = 11;
    constexpr float            s_kPuckCoreShare    = 0.45f;
    constexpr float            s_kPuckCoreRatio    = 3.0f;
    constexpr int              s_kTickWidthDip     = 4;
    constexpr int              s_kTickGapDip       = 4;
    constexpr int              s_kValueGapDip      = 6;
    constexpr int              s_kValueHeightDip   = 18;
    constexpr int              s_kValueFontDip     = 13;
    constexpr const wchar_t  * s_kFont             = DxuiTheme::kBodyFace;

    HRESULT   hr          = S_OK;
    bool      showValue   = m_explicitShowValue ? m_showValue : !m_suffix.empty();
    float     trackWidth  = m_scaler.Pxf (s_kTrackWidthDip);
    float     tickWidth   = m_scaler.Pxf (s_kTickWidthDip);
    float     tickGap     = m_scaler.Pxf (s_kTickGapDip);
    float     valueGap    = m_scaler.Pxf (s_kValueGapDip);
    float     valueFont   = m_scaler.Pxf (s_kValueFontDip);
    float     valueAreaH  = showValue ? (m_scaler.Pxf (s_kValueHeightDip) + valueGap) : 0.0f;
    float     rectH       = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float     trackTop    = (float) m_boundsDip.top;
    float     trackAvailH = std::max (0.0f, rectH - valueAreaH);
    float     centerX     = (float) m_boundsDip.left +
                            (float) (m_boundsDip.right - m_boundsDip.left) * 0.5f;
    float     t           = 0.0f;
    float     puckCy      = 0.0f;
    float     puckR       = m_scaler.Pxf (s_kPuckRadiusDip);
    float     tickStep    = 0.0f;
    uint32_t  coreColor   = m_enabled ? DxuiColor::AccentForWhiteContrast (accentArgb, s_kPuckCoreRatio)
                                      : s_kPuckCoreDis;



    if (m_max - m_min > s_kEpsilon)
    {
        t = (m_value - m_min) / (m_max - m_min);
    }

    // Max at the top: the puck rides down as the value falls, and the fill
    // spans puck to bottom.
    puckCy = trackTop + trackAvailH * (1.0f - t);

    if (m_focused)
    {
        puckR = m_scaler.Pxf (s_kPuckRadiusFocDip);
    }
    else if (m_hover || m_dragging)
    {
        puckR = m_scaler.Pxf (s_kPuckRadiusHovDip);
    }

    painter.FillRect (centerX - trackWidth * 0.5f, trackTop,
                      trackWidth, trackAvailH, s_kTrack);
    painter.FillRect (centerX - trackWidth * 0.5f, puckCy,
                      trackWidth, trackTop + trackAvailH - puckCy, accentArgb);

    tickStep = (m_tickInterval > s_kEpsilon) ? m_tickInterval : m_step;

    if (m_showTicks && tickStep > s_kEpsilon && trackAvailH > 0.0f)
    {
        int    tickCount = (int) std::round ((m_max - m_min) / tickStep) + 1;
        float  tickLeft  = centerX + trackWidth * 0.5f + tickGap;

        for (int i = 0; i < tickCount; i++)
        {
            float  tickT  = (float) i / (float) (tickCount - 1);
            float  tickCy = trackTop + trackAvailH * tickT;

            painter.FillRect (tickLeft, tickCy - 0.5f, tickWidth, 1.0f, s_kTick);
        }
    }

    painter.FillCircleApprox (centerX, puckCy, puckR,                    s_kPuckBody);
    painter.FillCircleApprox (centerX, puckCy, puckR,                    s_kPuckRing);
    painter.FillCircleApprox (centerX, puckCy, puckR - 1.0f,             s_kPuckBody);
    painter.FillCircleApprox (centerX, puckCy, puckR * s_kPuckCoreShare, coreColor);

    if (showValue)
    {
        wchar_t  buf[32] = {};

        FormatValue (buf);

        hr = text.DrawString (buf,
                              (float) m_boundsDip.left,
                              trackTop + trackAvailH + valueGap,
                              (float) (m_boundsDip.right - m_boundsDip.left),
                              m_scaler.Pxf (s_kValueHeightDip),
                              s_kValueText,
                              valueFont,
                              s_kFont,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
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
//  Tints the track fill and puck core from theme.Accent(); the inactive
//  track, ticks, puck ring, and value text derive from theme tokens.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSlider::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PaintInternal (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move is routed by DRAG STATE, not by position. Mid-drag it adjusts the
//  value and is claimed -- the pointer has usually left the track by then, and
//  hit-testing would hand the drag to whatever is under the cursor. Otherwise
//  it is only a hover update and is reported unhandled.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            handled = OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        }
        else
        {
            SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlider::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSlider::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}

