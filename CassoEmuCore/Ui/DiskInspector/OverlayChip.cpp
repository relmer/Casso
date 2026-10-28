#include "Pch.h"

#include "Ui/DiskInspector/OverlayChip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::OverlayChip
//
////////////////////////////////////////////////////////////////////////////////

OverlayChip::OverlayChip (const std::wstring & label) :
    m_label (label)
{
    m_focusable = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::GetWidthPx
//
//  The label's measured width and its padding.
//
////////////////////////////////////////////////////////////////////////////////

int OverlayChip::GetWidthPx (IDxuiTextRenderer & text, const DxuiDpiScaler & scaler) const
{
    float  w = 0;
    float  h = 0;



    text.MeasureString (m_label.c_str(), scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, w, h);

    return static_cast<int> (std::ceil (w)) + 2 * scaler.ToPx (static_cast<int> (kPadDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::Layout
//
////////////////////////////////////////////////////////////////////////////////

void OverlayChip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::Paint
//
//  On, a pill in the selection color under the label in full ink; off, the
//  label in muted ink, with a faint pill under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void OverlayChip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  x      = static_cast<float> (m_boundsDip.left);
    float  y      = static_cast<float> (m_boundsDip.top);
    float  w      = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float  h      = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float  ring   = m_scaler.ToPxf (2.0f);



    if (w > 0 && h > 0)
    {
        if (m_isOn)
        {
            painter.FillRoundedRect (x, y, w, h, h / 2.0f, theme.SelectionBackground());
        }
        else if (m_isHover && IsEnabled())
        {
            painter.FillRoundedRect (x, y, w, h, h / 2.0f, theme.HoverBackground());
        }

        text.DrawString (m_label.c_str(), x, y, w, h, !IsEnabled() ? theme.ForegroundDisabled() : m_isOn ? theme.Foreground() : theme.ForegroundMuted(),
                         m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        if (m_isFocused && m_focusCueVisible)
        {
            painter.OutlineRoundedRect (x - ring, y - ring, w + 2 * ring, h + 2 * ring, h / 2.0f + ring, m_scaler.ToPxf (1.0f), theme.FocusRing());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::OnMouse
//
//  A move is handled only when it changes the hover, so the window repaints
//  without the chip swallowing moves.
//
////////////////////////////////////////////////////////////////////////////////

bool OverlayChip::OnMouse (const DxuiMouseEvent & ev)
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

            if (isHandled)
            {
                Flip();
            }

            break;

        default:
            break;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool OverlayChip::OnKey (const DxuiKeyEvent & ev)
{
    bool  isFlip = ev.kind == DxuiKeyEventKind::Down && m_isFocused && IsEnabled() && (ev.vk == VK_SPACE || ev.vk == VK_RETURN);



    if (isFlip)
    {
        Flip();
    }

    return isFlip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OverlayChip::Flip
//
////////////////////////////////////////////////////////////////////////////////

void OverlayChip::Flip()
{
    m_isOn = !m_isOn;

    if (m_change)
    {
        m_change (m_isOn);
    }
}
