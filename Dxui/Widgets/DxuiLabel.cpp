#include "Pch.h"

#include "Widgets/DxuiLabel.h"

#include "Render/DxuiTextRenderer.h"
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
//  A disabled label, such as the one beside a field out of use, takes the
//  disabled color whatever its role.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::Paint (IDxuiPainter & /*painter*/, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t  argb = !IsEnabled()     ? theme.ForegroundDisabled()
                   : m_useThemeRole   ? (uint32_t) theme.TextColor (m_role)
                                      : m_argb;
    float     dip  = (m_fontDip > 0.0f) ? m_fontDip : theme.BodyFont().sizeDip;



    DrawResolved (text, argb, dip);

    if (m_isRequired && IsEnabled())
    {
        DrawRequiredMark (text, theme.ErrorForeground(), dip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::DrawRequiredMark
//
//  The mark is drawn just past the text, where the text ends on its line,
//  for a label aligned left; the text is one line in every dialog that
//  marks its fields.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::DrawRequiredMark (IDxuiTextRenderer & text, uint32_t argb, float fontDip) const
{
    HRESULT  hr     = S_OK;
    float    fontPx = m_scaler.ToPxf (fontDip);
    float    width  = 0.0f;
    float    height = 0.0f;
    float    left   = 0.0f;



    if (m_hAlign != DxuiTextHAlign::Left || m_text.empty())
    {
        return;
    }

    hr = text.MeasureString (m_text.c_str(), fontPx, m_fontFace.c_str(), width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    left = (float) m_boundsDip.left + width;

    hr = text.DrawString (kRequiredMark,
                          left,
                          (float) m_boundsDip.top,
                          (std::max) (0.0f, (float) m_boundsDip.right - left),
                          (float) (m_boundsDip.bottom - m_boundsDip.top),
                          argb,
                          fontPx,
                          m_fontFace.c_str(),
                          DxuiTextHAlign::Left,
                          m_vAlign,
                          m_weight,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::DrawResolved
//
//  With an elision set, the text is one line, cut to the label's width,
//  rather than wrapped onto lines below.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLabel::DrawResolved (IDxuiTextRenderer & text, uint32_t argb, float fontDip) const
{
    HRESULT       hr = S_OK;
    std::wstring  shown;



    if (m_elide != DxuiElide::None)
    {
        shown = DxuiTextElide::ToWidth (text, m_text, m_scaler.ToPxf (fontDip), m_fontFace.c_str(),
                                        (float) (m_boundsDip.right - m_boundsDip.left), m_elide);

        hr = text.DrawString (shown.c_str(),
                              (float) m_boundsDip.left,
                              (float) m_boundsDip.top,
                              (float) (m_boundsDip.right  - m_boundsDip.left),
                              (float) (m_boundsDip.bottom - m_boundsDip.top),
                              argb,
                              m_scaler.ToPxf (fontDip),
                              m_fontFace.c_str(),
                              m_hAlign,
                              m_vAlign,
                              m_weight,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLabel::MeasureWrappedDip
//
//  The font resolves as in the theme-less Paint: the label's size, or the
//  fallback size at the sentinel.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiLabel::MeasureWrappedDip (float widthDip, float & outHeightDip) const
{
    float  fontDip = (m_fontDip > 0.0f) ? m_fontDip : s_kFallbackFontDip;



    return DxuiTextRenderer::MeasureWrappedDip (m_text.c_str(), fontDip, m_fontFace.c_str(), m_weight, widthDip, outHeightDip);
}
