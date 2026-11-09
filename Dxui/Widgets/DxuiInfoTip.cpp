#include "Pch.h"

#include "Widgets/DxuiInfoTip.h"
#include "Core/DxuiFocusRing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::~DxuiInfoTip
//
////////////////////////////////////////////////////////////////////////////////

DxuiInfoTip::~DxuiInfoTip()
{
    if (s_open == this)
    {
        s_open = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::CloseOpen
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::CloseOpen()
{
    bool  open = s_open != nullptr;



    if (open)
    {
        s_open->Close();
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::CloseOpenOutside
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::CloseOpenOutside (POINT pointDip)
{
    RECT  bounds = (s_open != nullptr) ? s_open->GetBounds() : RECT {};



    if (s_open != nullptr && !PtInRect (&bounds, pointDip))
    {
        s_open->Close();
    }
}





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
//  Display the tip while the pointer is over the glyph, and hide it once the
//  pointer moves off the glyph or out of the window, unless it is pinned. A
//  press pins it, or closes it when it is pinned. Consume only the press,
//  never a move.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::OnMouse (const DxuiMouseEvent & ev)
{
    RECT     bounds  = GetBounds();
    POINT    pt      = { ev.positionDip.x, ev.positionDip.y };
    bool     inside  = ev.kind != DxuiMouseEventKind::Leave && PtInRect (&bounds, pt);
    bool     pinned  = s_open == this;
    int64_t  nowMs   = GetNowMs();
    bool     handled = false;



    if (inside && ev.kind == DxuiMouseEventKind::Down)
    {
        if (pinned)
        {
            Close();
        }
        else
        {
            Open();
        }

        handled = true;
    }
    else if (inside && !pinned && !m_text.empty())
    {
        m_tooltip.RequestShow (bounds, m_text, nowMs);
    }
    else if (m_isHovered && !pinned)
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
//  Enter or Space pins the tip, or closes it if it is already open. Escape
//  closes it, and returns true only when it was open.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::OnKey (const DxuiKeyEvent & ev)
{
    bool  isDown  = ev.kind == DxuiKeyEventKind::Down;
    bool  isPress = isDown && (ev.vk == VK_RETURN || ev.vk == VK_SPACE);
    bool  isOpen  = m_tooltip.IsVisible() || s_open == this;
    bool  handled = isPress || (isDown && ev.vk == VK_ESCAPE && isOpen);



    if ((isPress || ev.vk == VK_ESCAPE) && isDown && isOpen)
    {
        Close();
    }
    else if (isPress)
    {
        Open();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::OnFocusChanged
//
//  Losing focus closes the tip.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::OnFocusChanged (bool focused)
{
    m_isFocused = focused;

    if (!focused)
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::Open
//
//  Display the tip with no dwell and pin it. Any other pinned tip is closed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Open()
{
    if (s_open != nullptr && s_open != this)
    {
        s_open->Close();
    }

    if (!m_text.empty())
    {
        m_tooltip.RequestShowNow (GetBounds(), m_text, GetNowMs());
        s_open = this;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Close()
{
    m_tooltip.HideImmediate();

    if (s_open == this)
    {
        s_open = nullptr;
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
