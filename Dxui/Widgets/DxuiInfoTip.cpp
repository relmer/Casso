#include "Pch.h"

#include "Widgets/DxuiInfoTip.h"
#include "Core/DxuiFocusRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    SetDpi    (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::Paint
//
//  The glyph in the theme's muted text color, full strength under the
//  pointer, centered in the bounds: present, but quieter than the label it
//  sits beside. A ring around it once the keyboard has focused it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds = GetBounds();
    float    fontPx = kGlyphDip * (float) m_scaler.GetDpi() / 96.0f;
    HRESULT  hr     = S_OK;



    m_tooltip.SetTheme (theme);

    if (IsVisible())
    {
        hr = text.DrawString (kGlyph, (float) bounds.left, (float) bounds.top,
                              (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top),
                              m_isHovered ? theme.Foreground() : theme.ForegroundMuted(), fontPx, kGlyphFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (m_isFocused && IsFocusCueVisible())
        {
            DxuiFocusRing::Around (painter, (float) bounds.left, (float) bounds.top,
                                   (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top),
                                   m_scaler, theme.FocusRing());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::OnMouse
//
//  Shows the tip while the pointer is over the glyph and hides it once the
//  pointer moves off or leaves the window, and opens it at once on a press.
//  Consumes only the press, never a move.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::OnMouse (const DxuiMouseEvent & ev)
{
    RECT     bounds  = GetBounds();
    POINT    pt      = { ev.positionDip.x, ev.positionDip.y };
    bool     inside  = ev.kind != DxuiMouseEventKind::Leave && PtInRect (&bounds, pt);
    int64_t  nowMs   = GetNowMs();
    bool     handled = false;



    if (inside && ev.kind == DxuiMouseEventKind::Down)
    {
        Open();
        handled = true;
    }
    else if (inside && !m_text.empty())
    {
        m_tooltip.RequestShow (bounds, m_text, nowMs);
    }
    else if (m_isHovered)
    {
        m_tooltip.RequestHide (nowMs);
    }

    m_isHovered = inside;

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::OnKey
//
//  Enter or Space opens the tip, or closes it if it is already open.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::OnKey (const DxuiKeyEvent & ev)
{
    bool  isPress = ev.kind == DxuiKeyEventKind::Down && (ev.vk == VK_RETURN || ev.vk == VK_SPACE);



    if (isPress && m_tooltip.IsVisible())
    {
        m_tooltip.HideImmediate();
    }
    else if (isPress)
    {
        Open();
    }

    return isPress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::OnFocusChanged
//
//  Losing focus closes a tip the keyboard opened.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::OnFocusChanged (bool focused)
{
    m_isFocused = focused;

    if (!focused)
    {
        m_tooltip.RequestHide (GetNowMs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::Open
//
//  Shows the tip with no dwell, for as long as a hovered tip would stay.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Open()
{
    if (!m_text.empty())
    {
        m_tooltip.ShowTimed (GetBounds(), m_text, GetNowMs(), DxuiTooltip::kMaxVisibleMs);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiInfoTip::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
