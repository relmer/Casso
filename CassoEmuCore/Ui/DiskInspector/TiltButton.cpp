#include "Pch.h"

#include "Ui/DiskInspector/TiltButton.h"





static constexpr float  s_kDiskShare    = 0.32f;
static constexpr float  s_kHubShare     = 0.3f;
static constexpr float  s_kTiltedSquash = 0.42f;
static constexpr float  s_kStrokeDip    = 1.5f;





////////////////////////////////////////////////////////////////////////////////
//
//  TiltButton::Layout
//
////////////////////////////////////////////////////////////////////////////////

void TiltButton::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  TiltButton::Paint
//
//  The disk as a ring with its hub: round from above, squashed when tilted,
//  with a faint circle behind it under the pointer or while pressed.
//
////////////////////////////////////////////////////////////////////////////////

void TiltButton::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float     w        = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float     h        = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float     cx       = m_boundsDip.left + w / 2.0f;
    float     cy       = m_boundsDip.top + h / 2.0f;
    float     radius   = std::min (w, h) * s_kDiskShare;
    bool      isTilted = m_isTilted && m_isTilted();
    float     squash   = isTilted ? s_kTiltedSquash : 1.0f;
    float     stroke   = m_scaler.ToPxf (s_kStrokeDip);
    uint32_t  ink      = IsEnabled() ? theme.Foreground() : theme.ForegroundDisabled();



    if (w > 0 && h > 0)
    {
        if (m_isHover || m_isPressed)
        {
            painter.FillCircle (cx, cy, std::min (w, h) / 2.0f, m_isPressed ? theme.PressedBackground() : theme.HoverBackground());
        }

        text.DrawEllipse (cx, cy, radius, radius * squash, stroke, ink);
        text.DrawEllipse (cx, cy, radius * s_kHubShare, radius * s_kHubShare * squash, stroke, ink);

        if (m_isFocused && m_focusCueVisible)
        {
            text.DrawEllipse (cx, cy, std::min (w, h) / 2.0f - 1.0f, std::min (w, h) / 2.0f - 1.0f, m_scaler.ToPxf (1.0f), theme.FocusRing());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TiltButton::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool TiltButton::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isOver    = IsEnabled() && PtInRect (&m_boundsDip, ev.positionDip) != FALSE;
    bool  wasHover  = m_isHover;
    bool  isHandled = false;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            m_isHover = isOver;
            isHandled = m_isHover != wasHover;
            break;

        case DxuiMouseEventKind::Leave:
            m_isHover   = false;
            m_isPressed = false;
            break;

        case DxuiMouseEventKind::Down:
            m_isPressed = isOver && ev.button == DxuiMouseButton::Left;
            isHandled   = m_isPressed;
            break;

        case DxuiMouseEventKind::Up:
            isHandled   = m_isPressed && isOver && ev.button == DxuiMouseButton::Left;
            m_isPressed = false;

            if (isHandled && m_click)
            {
                m_click();
            }

            break;

        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TiltButton::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool TiltButton::OnKey (const DxuiKeyEvent & ev)
{
    bool  isClick = ev.kind == DxuiKeyEventKind::Down && m_isFocused && IsEnabled() && (ev.vk == VK_SPACE || ev.vk == VK_RETURN);



    if (isClick && m_click)
    {
        m_click();
    }

    return isClick;
}
