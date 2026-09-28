#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiToggle.h"

#include "Core/DxuiFocusRing.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

// The palette constants are private members of DxuiToggle.





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::HitTest (int x, int y) const
{
    // A disabled toggle is transparent to the mouse, so whatever is behind
    // it gets the click.
    return m_enabled
           && x >= m_boundsDip.left && x < m_boundsDip.right
           && y >= m_boundsDip.top  && y < m_boundsDip.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
    if (!m_hover)
    {
        m_pressed = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnLButtonDown (int x, int y)
{
    bool  hit = HitTest (x, y);



    if (hit)
    {
        m_pressed = true;
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnLButtonUp (int x, int y)
{
    bool  consumed = m_pressed && HitTest (x, y);



    m_pressed = false;

    if (consumed)
    {
        Flip();
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnKey (WPARAM vk)
{
    bool  flips = m_enabled
                  && m_focused
                  && (vk == VK_SPACE || vk == VK_RETURN);



    if (flips)
    {
        Flip();
    }

    return flips;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Flip()
{
    m_checked = !m_checked;

    if (m_change)
    {
        m_change (m_checked);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeTrackAndThumb
//
//  The pill laid out in pill, whichever way that box is drawn: the track
//  runs its long side along the direction of travel from the box's top-left
//  corner, horizontal for Right and upright for Up and Down. The end caps
//  are half the short side across, and the thumb sits inset a sixth of the
//  short side from the edge, at the end the direction gives while checked
//  and at the other end while not. At the toggle's 18-pixel height that is
//  the 3-pixel inset the pill has always had.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToggle::TrackAndThumb DxuiToggle::ComputeTrackAndThumb (
    const D2D1_RECT_F  & pill,
    OnDirection          direction,
    bool                 checked)
{
    constexpr float  kThumbInsetRatio = 1.0f / 6.0f;
    constexpr float  kHalf            = 0.5f;



    TrackAndThumb  geometry;
    float          width      = pill.right  - pill.left;
    float          height     = pill.bottom - pill.top;
    float          longSide   = std::max (width, height);
    float          shortSide  = std::min (width, height);
    bool           isUpright  = direction != OnDirection::Right;
    bool           isAtFar    = false;



    geometry.capRadius   = shortSide * kHalf;
    geometry.thumbRadius = geometry.capRadius - shortSide * kThumbInsetRatio;
    geometry.track       = D2D1::RectF (pill.left,
                                        pill.top,
                                        pill.left + (isUpright ? shortSide : longSide),
                                        pill.top  + (isUpright ? longSide  : shortSide));

    // The far end is the right for a horizontal pill and the bottom for an
    // upright one; Up is the only direction whose on end is the near one.
    isAtFar = (direction == OnDirection::Up) ? !checked : checked;

    if (isUpright)
    {
        geometry.thumbCenter.x = geometry.track.left + geometry.capRadius;
        geometry.thumbCenter.y = isAtFar ? geometry.track.bottom - geometry.capRadius
                                         : geometry.track.top    + geometry.capRadius;
    }
    else
    {
        geometry.thumbCenter.x = isAtFar ? geometry.track.right - geometry.capRadius
                                         : geometry.track.left  + geometry.capRadius;
        geometry.thumbCenter.y = geometry.track.top + geometry.capRadius;
    }

    return geometry;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Pill body painted as one rect plus two end-cap circles, the thumb a
//  circle at one end, all from ComputeTrackAndThumb. The pill sits at the
//  left of the bounds, centered top to bottom, with the label beside it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::PaintInternal (IDxuiPainter & painter, IDxuiTextRenderer & text, uint32_t accentArgb, uint32_t focusArgb) const
{
    constexpr uint32_t  s_kPillOff       = 0xFF4A5260;
    constexpr uint32_t  s_kPillOffHover  = 0xFF5A6271;
    constexpr uint32_t  s_kPillDisabled  = 0xFF2A2F38;
    constexpr uint32_t  s_kThumb         = 0xFFFFFFFF;
    constexpr uint32_t  s_kThumbDisabled = 0xFF707070;
    constexpr uint32_t  s_kTextIdle      = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled  = 0xFF707070;
    constexpr float     s_kPillWidthDip  = 36.0f;
    constexpr float     s_kPillHeightDip = 18.0f;
    constexpr float     s_kLabelGapDip   = 8.0f;
    constexpr float     s_kFontDip       = 13.0f;
    constexpr float     s_kPillRatio     = 3.0f;    // WCAG 1.4.11 min contrast of pill vs white thumb



    HRESULT        hr         = S_OK;
    float          pillW      = m_scaler.ToPxf (s_kPillWidthDip);
    float          pillH      = m_scaler.ToPxf (s_kPillHeightDip);
    float          labelGap   = m_scaler.ToPxf (s_kLabelGapDip);
    float          fontDip    = m_scaler.ToPxf (s_kFontDip);
    bool           isUpright  = m_onDirection != OnDirection::Right;
    float          extentH    = isUpright ? pillW : pillH;
    float          pillLeft   = (float) m_boundsDip.left;
    float          pillTop    = (float) m_boundsDip.top + ((float) (m_boundsDip.bottom - m_boundsDip.top) - extentH) * 0.5f;
    TrackAndThumb  geometry   = ComputeTrackAndThumb (D2D1::RectF (pillLeft, pillTop, pillLeft + pillW, pillTop + pillH), m_onDirection, m_checked);
    float          capR       = geometry.capRadius;
    float          trackW     = geometry.track.right  - geometry.track.left;
    float          trackH     = geometry.track.bottom - geometry.track.top;
    uint32_t       pillColor;
    uint32_t       accentBase = DxuiColor::ComputeAccentForWhiteContrast (accentArgb, s_kPillRatio);
    uint32_t       thumbColor = m_enabled ? s_kThumb : s_kThumbDisabled;
    uint32_t       textColor  = m_enabled ? s_kTextIdle : s_kTextDisabled;

    // Same rule as DxuiCheckbox: no area means the control has not been laid
    // out yet (a WM_PAINT can land between OnCreate and the first Layout), and
    // the label box below is the width MINUS the pill and gap -- on a
    // {0,0,0,0} rect that is negative, which DWrite rejects outright.
    bool           hasArea    = (m_boundsDip.right > m_boundsDip.left)
                                && (m_boundsDip.bottom > m_boundsDip.top);



    if (hasArea)
    {
        if (!m_enabled)
        {
            pillColor = s_kPillDisabled;
        }
        else if (m_checked)
        {
            pillColor = m_hover ? DxuiColor::Scale (accentBase, kHoverLighten) : accentBase;
        }
        else
        {
            pillColor = m_hover ? s_kPillOffHover : s_kPillOff;
        }

        if (isUpright)
        {
            painter.FillRect   (geometry.track.left,        geometry.track.top + capR,    trackW, trackH - trackW, pillColor);
            painter.FillCircle (geometry.track.left + capR, geometry.track.top + capR,    capR,   pillColor);
            painter.FillCircle (geometry.track.left + capR, geometry.track.bottom - capR, capR,   pillColor);
        }
        else
        {
            painter.FillRect   (geometry.track.left + capR,  geometry.track.top,        trackW - trackH, trackH, pillColor);
            painter.FillCircle (geometry.track.left + capR,  geometry.track.top + capR, capR,            pillColor);
            painter.FillCircle (geometry.track.right - capR, geometry.track.top + capR, capR,            pillColor);
        }

        painter.FillCircle (geometry.thumbCenter.x, geometry.thumbCenter.y, geometry.thumbRadius, thumbColor);

        // An unlabeled toggle narrates its own state instead, so the pill is
        // never left with nothing beside it.
        std::wstring   shown = m_label.empty() ? (m_checked ? L"On" : L"Off")
                                               : m_label;

        hr = text.DrawString (shown.c_str(),
                              pillLeft + trackW + labelGap,
                              (float) m_boundsDip.top,
                              (float) (m_boundsDip.right - m_boundsDip.left) - trackW - labelGap,
                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                              textColor,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        //  The ring encloses the PILL AND ITS LABEL. Same rule as the
        //  checkbox and the radio: the control is the pair, and an unlabeled
        //  toggle narrates its own state, so there is always a run to ring.
        if (m_focused && m_focusCueVisible)
        {
            DxuiFocusRing::AroundRun (painter, text, shown, fontDip, DxuiTheme::kBodyFace,
                                      pillLeft,
                                      pillLeft + trackW + labelGap,
                                      (float) m_boundsDip.top,
                                      (float) (m_boundsDip.bottom - m_boundsDip.top),
                                      trackH,
                                      m_scaler,
                                      focusArgb);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::Paint
//
//  Non-themed overload keeps the default blue accent; the IDxuiControl
//  themed override tints the "on" pill from theme.Accent() and the focus
//  ring from theme.FocusRing().
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToggle::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    PaintInternal (painter, text, kDefaultAccentArgb, kDefaultFocusArgb);
}




void DxuiToggle::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    PaintInternal (painter, text, theme.Accent(), theme.FocusRing());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the toggle does not consume moves other widgets want.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isLeft   = (ev.button == DxuiMouseButton::Left);
    bool  consumed = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        // Hover tracking never claims the event -- a toggle does not stop a
        // move from reaching whatever else wants to see it.
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Down:
        consumed = isLeft && OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        break;

    case DxuiMouseEventKind::Up:
        consumed = isLeft && OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        break;

    default:
        break;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggle::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToggle::OnKey (const DxuiKeyEvent & ev)
{
    // Key-up would flip a second time for the same press.
    return (ev.kind == DxuiKeyEventKind::Down) && OnKey (ev.vk);
}

