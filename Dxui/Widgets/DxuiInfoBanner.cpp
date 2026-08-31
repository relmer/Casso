#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiWarningBadge.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"




static constexpr uint32_t   s_kBadgeInkArgb = 0xFFF7F9FCu;   // near-white "i" on the accent disc





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::EstimateLines
//
//  Renderer-free wrapped-line estimate: how many lines the text takes at
//  `textWidthPx`, from an average glyph width. Deliberately generous (a wide
//  average) so the count rounds up and the caller never sizes the banner too
//  short to show every line.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiInfoBanner::EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const
{
    float   glyphPx = scaler.ToPxf (s_kFontDip) * s_kEstGlyphEm;
    float   perLine = (glyphPx > 0.0f) ? (textWidthPx / glyphPx) : 1.0f;
    int     chars   = (int) m_text.size();
    int     lines   = 1;



    if (perLine >= 1.0f)
    {
        lines = (int) std::ceil ((float) chars / perLine);
    }

    if (lines < 1)
    {
        lines = 1;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetPreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const
{
    float   padX      = scaler.ToPxf (s_kPadXDip);
    float   padY      = scaler.ToPxf (s_kPadYDip);
    float   iconCol   = scaler.ToPxf (s_kIconBoxDip) + scaler.ToPxf (s_kIconGapDip);
    float   textWidth = widthPx - padX * 2.0f - iconCol;
    float   lineH     = scaler.ToPxf (s_kFontDip) * s_kLineHeightEm;
    int     lines     = EstimateLines ((textWidth > 1.0f) ? textWidth : 1.0f, scaler);
    float   textH     = lineH * (float) lines;
    float   iconH     = scaler.ToPxf (s_kIconBoxDip);



    // The content is the taller of the wrapped text and the icon, plus padding.
    return ((textH > iconH) ? textH : iconH) + padY * 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::GetMeasuredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiInfoBanner::GetMeasuredHeightPx (IDxuiTextRenderer   &  text,
                                           float                  widthPx,
                                           const DxuiDpiScaler &  scaler) const
{
    HRESULT  hr        = S_OK;
    float    padX      = scaler.ToPxf (s_kPadXDip);
    float    padY      = scaler.ToPxf (s_kPadYDip);
    float    iconCol   = scaler.ToPxf (s_kIconBoxDip) + scaler.ToPxf (s_kIconGapDip);
    float    textWidth = widthPx - padX * 2.0f - iconCol;
    float    iconH     = scaler.ToPxf (s_kIconBoxDip);
    float    outW      = 0.0f;
    float    outH      = 0.0f;



    if (textWidth < 1.0f)
    {
        textWidth = 1.0f;
    }

    hr = text.MeasureStringWrapped (m_text.c_str(), scaler.ToPxf (s_kFontDip),
                                    DxuiTheme::kBodyFace, textWidth, outW, outH);

    if (FAILED (hr) || outH <= 0.0f)
    {
        return GetPreferredHeightPx (widthPx, scaler);
    }

    return ((outH > iconH) ? outH : iconH) + padY * 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::Paint
//
//  Draws an informational notice: an accent-tinted surface, an info badge, and
//  wrapping body text.
//
//  A tinted fill inside a MUTED accent border, rather than a solid accent
//  panel, so the banner reads as a notice and not as a button. That
//  distinction is the widget's entire job -- it must be noticed without
//  inviting a click.
//
//  The badge is drawn from PRIMITIVES rather than an icon-font glyph. That
//  avoids a font dependency for one symbol, and more importantly gives exact
//  centering: a glyph's optical center rarely coincides with its line-box
//  center, so a font-drawn "i" sits visibly off inside a disc.
//
//  The badge geometry is expressed as fractions of the disc radius, so it
//  scales with DPI and with any icon size without a second set of constants.
//
//  Text is inset past the badge and wraps, since a notice is prose of
//  unpredictable length -- unlike a label, it must not be clipped.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr       = S_OK;
    float    left     = (float) m_boundsDip.left;
    float    top      = (float) m_boundsDip.top;
    float    width    = (float) (m_boundsDip.right  - m_boundsDip.left);
    float    height   = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float    padX     = m_scaler.ToPxf (s_kPadXDip);
    float    padY     = m_scaler.ToPxf (s_kPadYDip);
    float    borderPx = m_scaler.ToPxf (s_kBorderDip);
    float    iconBox  = m_scaler.ToPxf (s_kIconBoxDip);
    float    iconGap  = m_scaler.ToPxf (s_kIconGapDip);
    float    fontPx   = m_scaler.ToPxf (s_kFontDip);
    float    textX    = left + padX + iconBox + iconGap;
    float    textW    = width - padX * 2.0f - iconBox - iconGap;
    float    iconR    = iconBox * 0.5f;
    float    iconCx   = left + padX + iconR;
    float    iconCy   = top + height * 0.5f;   // vertically centered in the bordered area



    if (!m_visible)
    {
        return;
    }

    // Themed surface: a subtle tinted fill inside a muted border, so the banner
    // reads as a notice, not a button. A warning carries its own hue rather
    // than the theme accent -- caution should not depend on what the accent
    // happens to be.
    if (m_severity == Severity::Warning)
    {
        painter.FillRect    (left, top, width, height, theme.InfoBannerWarningBackground());
        painter.OutlineRect (left, top, width, height, borderPx, theme.InfoBannerWarningBorder());
    }
    else
    {
        painter.FillRect    (left, top, width, height, theme.InfoBannerBackground());
        painter.OutlineRect (left, top, width, height, borderPx, theme.InfoBannerBorder());
    }

    if (m_severity == Severity::Warning)
    {
        // The same triangle the drive widget shows on a damaged disk, so the
        // two read as one idea rather than two similar-looking marks.
        DxuiWarningBadge::Draw (painter,
                                iconCx - iconR, iconCy - iconR,
                                iconBox, iconBox,
                                theme.WarningAccent(), theme.WarningEdge(), theme.WarningMark());
    }
    else
    {
        // Info badge, drawn from primitives (no icon-font dependency, exact centering):
        // a full-accent disc with a light "i" -- a dot over a stem, symmetric about the
        // disc center both ways.
        float  dotR  = iconR * 0.17f;
        float  stemW = iconR * 0.24f;
        float  stemH = iconR * 0.62f;

        painter.FillCircleApprox (iconCx, iconCy, iconR, theme.Accent());
        painter.FillCircleApprox (iconCx, iconCy - iconR * 0.38f, dotR, s_kBadgeInkArgb);
        painter.FillRect         (iconCx - stemW * 0.5f, iconCy - iconR * 0.07f, stemW, stemH, s_kBadgeInkArgb);
    }

    // Wrapping body text.
    hr = text.DrawString (m_text.c_str(),
                          textX,
                          top + padY,
                          (textW > 1.0f) ? textW : 1.0f,
                          height - padY * 2.0f,
                          theme.InfoBannerForeground(),
                          fontPx,
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Top,
                          DxuiFontWeight::Normal,
                          true);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
