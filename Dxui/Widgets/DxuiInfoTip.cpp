#include "Pch.h"

#include "Widgets/DxuiInfoTip.h"





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
//  explains.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoTip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds = GetBounds();
    float    fontPx = kGlyphDip * (float) m_scaler.GetDpi() / 96.0f;
    HRESULT  hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);

    m_tooltip.SetTheme (theme);

    if (IsVisible())
    {
        hr = text.DrawString (kGlyph, (float) bounds.left, (float) bounds.top,
                              (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top),
                              m_isHovered ? theme.Foreground() : theme.ForegroundMuted(), fontPx, kGlyphFace,
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTip::OnMouse
//
//  Shows the tip while the pointer is over the glyph and hides it once the
//  pointer moves off or leaves the window. Never consumes the event.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiInfoTip::OnMouse (const DxuiMouseEvent & ev)
{
    RECT     bounds = GetBounds();
    POINT    pt     = { ev.positionDip.x, ev.positionDip.y };
    bool     inside = ev.kind != DxuiMouseEventKind::Leave && PtInRect (&bounds, pt);
    int64_t  nowMs  = GetNowMs();



    if (inside && !m_text.empty())
    {
        m_tooltip.RequestShow (bounds, m_text, nowMs);
    }
    else if (m_isHovered)
    {
        m_tooltip.RequestHide (nowMs);
    }

    m_isHovered = inside;

    return false;
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
