#include "Pch.h"

#include "Widgets/DxuiLabel.h"

#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::Paint  (theme-less)
//
//  Legacy theme-less paint. Draws with the pinned explicit color (SetColor);
//  role resolution needs a theme, so consumers that use this overload must
//  set an explicit color. A label that never pinned a color has nothing but
//  the fallback white here, which is why this overload is the wrong one to
//  reach for: pass the theme and the label follows it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::Paint (IDxuiTextRenderer & text) const
{
    DrawResolved (text, m_argb, (m_fontDip > 0.0f) ? m_fontDip : s_kFallbackFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::Paint  (themed)
//
//  Resolves the color from the semantic role (unless an explicit color was
//  pinned) and the font size from the theme's body font when left at the
//  sentinel, so a plain label carries no color or size of its own and tracks
//  the active theme automatically.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::Paint (IDxuiPainter & /*painter*/, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    //  A disabled label, such as the one beside a field out of use, takes
    //  the disabled color whatever its role.
    uint32_t  argb = !IsEnabled()     ? theme.ForegroundDisabled()
                   : m_useThemeRole   ? (uint32_t) theme.TextColor (m_role)
                                      : m_argb;
    float     dip  = (m_fontDip > 0.0f) ? m_fontDip : theme.BodyFont().sizeDip;



    DrawResolved (text, argb, dip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::DrawResolved
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::DrawResolved (IDxuiTextRenderer & text, uint32_t argb, float fontDip) const
{
    HRESULT  hr = S_OK;



    hr = text.DrawString (m_text.c_str(),
                          (float) m_boundsDip.left,
                          (float) m_boundsDip.top,
                          (float) (m_boundsDip.right  - m_boundsDip.left),
                          (float) (m_boundsDip.bottom - m_boundsDip.top),
                          argb,
                          m_scaler.ToPxf (fontDip),
                          m_fontFace.c_str(),
                          m_hAlign,
                          m_vAlign,
                          m_weight);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




