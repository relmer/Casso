#include "Pch.h"

#include "Ui/Debugger/Panes/ProfileProgress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileProgress::GetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProfileProgress::GetText (double share)
{
    return std::format (L"Building the profile from history: {}%", (int) (std::clamp (share, 0.0, 1.0) * 100.0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileProgress::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ProfileProgress::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_scaler    = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileProgress::Paint
//
//  The text is in the body color over the track and over the fill alike, so
//  the fill is a lighter tint of the accent.
//
////////////////////////////////////////////////////////////////////////////////

void ProfileProgress::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  kTint  = 0.45f;
    float            left   = (float) m_boundsDip.left;
    float            top    = (float) m_boundsDip.top;
    float            width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float            height = (float) (m_boundsDip.bottom - m_boundsDip.top);
    DxuiFontHandle   font   = theme.BodyFont();
    HRESULT          hr     = S_OK;



    if (!m_visible || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    painter.FillRect    (left, top, width, height, theme.Divider());
    painter.FillRect    (left, top, width * (float) m_share, height, DxuiColor::Mix (theme.ContentBackground(), theme.Accent(), kTint));
    painter.OutlineRect (left, top, width, height, 1.0f, theme.Divider());

    hr = text.DrawString (GetText (m_share).c_str(), left, top, width, height, theme.Foreground(), m_scaler.ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
