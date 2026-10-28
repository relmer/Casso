#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiSegmentedControl.h"

#include "Core/DxuiAnimation.h"
#include "Core/DxuiSystemSettings.h"
#include "Theme/DxuiColor.h"





static constexpr float  s_kPillInsetDip = 2.0f;
static constexpr float  s_kRingGapDip   = 2.0f;
static constexpr float  s_kRingDip      = 1.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::DxuiSegmentedControl
//
////////////////////////////////////////////////////////////////////////////////

DxuiSegmentedControl::DxuiSegmentedControl()
{
    m_focusable         = true;
    m_animationsEnabled = DxuiSystemSettings::Instance().AreAnimationsEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::SetSegments
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::SetSegments (std::vector<std::wstring> labels)
{
    m_labels    = std::move (labels);
    m_selected  = m_labels.empty() ? -1 : 0;
    m_hover     = -1;
    m_pressed   = -1;
    m_pillPos   = 0.0f;
    m_isSliding = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::SetSelected
//
//  Clamped to the segments. The change callback is not raised: the host
//  made the change.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::SetSelected (int index, bool isAnimated)
{
    int  count = static_cast<int> (m_labels.size());



    if (count > 0)
    {
        index = std::clamp (index, 0, count - 1);

        if (index != m_selected)
        {
            m_selected = index;

            if (isAnimated)
            {
                StartSlide();
            }
            else
            {
                m_pillPos   = static_cast<float> (index);
                m_isSliding = false;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::StartSlide
//
//  The slide starts from wherever the pill is, so a second click during a
//  slide turns the pill around rather than jumping it. Its clock starts at
//  the next Tick, since input handlers have no frame time.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::StartSlide()
{
    if (m_animationsEnabled)
    {
        m_slideFrom    = m_pillPos;
        m_slideStartMs = -1;
        m_isSliding    = m_pillPos != static_cast<float> (m_selected);
    }
    else
    {
        m_pillPos   = static_cast<float> (m_selected);
        m_isSliding = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::Tick (int64_t nowMs)
{
    float  t = 0.0f;



    if (m_isSliding)
    {
        if (m_slideStartMs < 0)
        {
            m_slideStartMs = nowMs;
        }

        t = static_cast<float> (nowMs - m_slideStartMs) / static_cast<float> (kSlideMs);

        if (t >= 1.0f)
        {
            m_pillPos   = static_cast<float> (m_selected);
            m_isSliding = false;
        }
        else
        {
            m_pillPos = m_slideFrom + (static_cast<float> (m_selected) - m_slideFrom) * DxuiAnimation::ApplyEase (DxuiTweenEase::EaseInOut, std::max (t, 0.0f));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::GetSegmentWidth
//
//  The segments split the capsule inside the pill's inset evenly.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiSegmentedControl::GetSegmentWidth() const
{
    float  inner = static_cast<float> (m_boundsDip.right - m_boundsDip.left) - 2.0f * m_scaler.ToPxf (s_kPillInsetDip);



    return m_labels.empty() ? 0.0f : std::max (inner, 0.0f) / static_cast<float> (m_labels.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::HitTest
//
//  The segment under a point, or -1. A disabled control is transparent to
//  the mouse.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiSegmentedControl::HitTest (int x, int y) const
{
    float  segment = GetSegmentWidth();
    float  left    = static_cast<float> (m_boundsDip.left) + m_scaler.ToPxf (s_kPillInsetDip);
    int    hit     = -1;



    if (m_enabled && segment > 0.0f &&
        x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom)
    {
        hit = std::clamp (static_cast<int> ((static_cast<float> (x) - left) / segment), 0, static_cast<int> (m_labels.size()) - 1);
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);

    if (m_pressed >= 0 && m_pressed != m_hover)
    {
        m_pressed = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSegmentedControl::OnLButtonDown (int x, int y)
{
    int  hit = HitTest (x, y);



    if (hit >= 0)
    {
        m_pressed = hit;
    }

    return hit >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSegmentedControl::OnLButtonUp (int x, int y)
{
    int   hit      = HitTest (x, y);
    bool  consumed = m_pressed >= 0 && hit == m_pressed;



    m_pressed = -1;

    if (consumed)
    {
        Commit (hit);
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::OnKey
//
//  Left and Right move one segment and stop at the ends, as a radio group's
//  arrows do across two choices; Home and End go to the ends.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSegmentedControl::OnKey (WPARAM vk)
{
    int   count   = static_cast<int> (m_labels.size());
    int   next    = m_selected;
    bool  handled = m_enabled && m_focused && count > 0;



    if (handled)
    {
        switch (vk)
        {
            case VK_LEFT:  next = std::max (m_selected - 1, 0);         break;
            case VK_RIGHT: next = std::min (m_selected + 1, count - 1); break;
            case VK_HOME:  next = 0;                                    break;
            case VK_END:   next = count - 1;                            break;
            default:       handled = false;                             break;
        }
    }

    if (handled)
    {
        Commit (next);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::Commit (int newIndex)
{
    if (newIndex != m_selected)
    {
        m_selected = newIndex;
        StartSlide();

        if (m_change)
        {
            m_change (newIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::Paint
//
//  The capsule, a hover fill in an unselected segment, the pill at its
//  position, then the labels over both: the selected one in the body color
//  and the rest muted.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSegmentedControl::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  s_kPressedScale = 0.85f;



    HRESULT  hr      = S_OK;
    float    left    = static_cast<float> (m_boundsDip.left);
    float    top     = static_cast<float> (m_boundsDip.top);
    float    width   = static_cast<float> (m_boundsDip.right  - m_boundsDip.left);
    float    height  = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float    inset   = m_scaler.ToPxf (s_kPillInsetDip);
    float    segment = GetSegmentWidth();
    float    inner   = height - 2.0f * inset;
    float    ringGap = m_scaler.ToPxf (s_kRingGapDip);
    float    fontPx  = m_scaler.ToPxf (kLabelFontDip);
    int      i       = 0;



    if (width > 0.0f && height > 0.0f && segment > 0.0f)
    {
        painter.FillRoundedRect    (left, top, width, height, height / 2.0f, theme.BackgroundElevated());
        painter.OutlineRoundedRect (left, top, width, height, height / 2.0f, m_scaler.ToPxf (s_kRingDip), theme.ButtonBorder());

        if (m_hover >= 0 && m_hover != m_selected && m_enabled)
        {
            painter.FillRoundedRect (left + inset + segment * static_cast<float> (m_hover), top + inset, segment, inner, inner / 2.0f,
                                     m_pressed == m_hover ? DxuiColor::Darken (theme.HoverBackground(), s_kPressedScale) : theme.HoverBackground());
        }

        if (m_selected >= 0)
        {
            painter.FillRoundedRect (left + inset + segment * m_pillPos, top + inset, segment, inner, inner / 2.0f,
                                     m_enabled ? theme.ButtonHover() : theme.ButtonIdle());
        }

        for (i = 0; i < static_cast<int> (m_labels.size()); i++)
        {
            uint32_t  color = !m_enabled ? theme.ForegroundDisabled()
                            : (i == m_selected || i == m_hover) ? theme.Foreground()
                            : theme.ForegroundMuted();

            hr = text.DrawString (m_labels[static_cast<size_t> (i)].c_str(),
                                  left + inset + segment * static_cast<float> (i), top,
                                  segment, height,
                                  color, fontPx, DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Center, DxuiTextVAlign::Center);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (m_focused && m_focusCueVisible)
        {
            painter.OutlineRoundedRect (left - ringGap, top - ringGap, width + 2.0f * ringGap, height + 2.0f * ringGap,
                                        height / 2.0f + ringGap, m_scaler.ToPxf (s_kRingDip), theme.FocusRing());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::OnMouse
//
//  A move is handled only when it changes the hovered or armed segment, so
//  the window repaints at once without swallowing moves other controls use.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSegmentedControl::OnMouse (const DxuiMouseEvent & ev)
{
    int   prevHover   = m_hover;
    int   prevPressed = m_pressed;
    bool  isLeft      = ev.button == DxuiMouseButton::Left;
    bool  handled     = false;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            SetMouseHover (ev.positionDip.x, ev.positionDip.y);
            handled = m_hover != prevHover || m_pressed != prevPressed;
            break;

        case DxuiMouseEventKind::Down:
            handled = isLeft && OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
            break;

        case DxuiMouseEventKind::Up:
            handled = isLeft && OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
            break;

        default:
            break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSegmentedControl::OnKey (const DxuiKeyEvent & ev)
{
    return ev.kind == DxuiKeyEventKind::Down && OnKey (ev.vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSegmentedControl::GetAccessibleName
//
//  The selected segment's label.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiSegmentedControl::GetAccessibleName() const
{
    return (m_selected >= 0) ? m_labels[static_cast<size_t> (m_selected)] : std::wstring();
}
