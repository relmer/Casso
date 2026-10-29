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
//  The label's measured width, its padding, and room for the check mark,
//  so a chip does not change width as it turns on and off.
//
////////////////////////////////////////////////////////////////////////////////

int OverlayChip::GetWidthPx (IDxuiTextRenderer & text, const DxuiDpiScaler & scaler) const
{
    float  w = 0;
    float  h = 0;



    text.MeasureString (m_label.c_str(), scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, w, h);

    return static_cast<int> (std::ceil (w)) + 2 * scaler.ToPx (static_cast<int> (kPadDip)) + scaler.ToPx (static_cast<int> (kCheckDip + kCheckGapDip));
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
//  A separate outlined pill. On, an accent tint and border and a check mark
//  before the label in full ink; off, the label in muted ink, with a faint
//  fill under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void OverlayChip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  x      = static_cast<float> (m_boundsDip.left);
    float  y      = static_cast<float> (m_boundsDip.top);
    float  w      = static_cast<float> (m_boundsDip.right - m_boundsDip.left);
    float  h      = static_cast<float> (m_boundsDip.bottom - m_boundsDip.top);
    float  ring   = m_scaler.ToPxf (2.0f);
    float  check  = m_scaler.ToPxf (kCheckDip);
    float  pad    = m_scaler.ToPxf (kPadDip);
    float  edge   = m_scaler.ToPxf (1.0f);



    if (w > 0 && h > 0)
    {
        if (m_isOn)
        {
            painter.FillRoundedRect    (x, y, w, h, h / 2.0f, (theme.Accent() & 0x00FFFFFFu) | 0x40000000u);
            painter.OutlineRoundedRect (x, y, w, h, h / 2.0f, edge, theme.Accent());
            text.DrawString (kpszCheck, x + pad, y, check, h, theme.Foreground(), check, kpszCheckFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                             DxuiFontWeight::Normal, false);
        }
        else
        {
            if (m_isHover && IsEnabled())
            {
                painter.FillRoundedRect (x, y, w, h, h / 2.0f, theme.HoverBackground());
            }

            painter.OutlineRoundedRect (x, y, w, h, h / 2.0f, edge, theme.Border());
        }

        text.DrawString (m_label.c_str(), x + check + m_scaler.ToPxf (kCheckGapDip), y, w - check - m_scaler.ToPxf (kCheckGapDip), h, !IsEnabled() ? theme.ForegroundDisabled() : m_isOn ? theme.Foreground() : theme.ForegroundMuted(),
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
