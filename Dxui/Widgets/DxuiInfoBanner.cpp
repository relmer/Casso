#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiWarningBadge.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"







////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBanner::EstimateLines
//
//  Renderer-free wrapped-line estimate: how many lines the text takes at
//  `textWidthPx`, from an average glyph width. Deliberately generous (a wide
//  average) so the count rounds up and the caller never sizes the banner too
//  short to show every line.
//
//  MEASURED PER PARAGRAPH, BECAUSE A FORCED BREAK ENDS A LINE WHEREVER IT
//  FALLS. Dividing the whole string by the characters that fit assumes every
//  line is full, which undercounts by one for each partly-filled line a break
//  leaves behind -- so a two-paragraph notice was sized as though it were one
//  flowing sentence and lost its last lines off the bottom. GetMeasuredHeightPx
//  does not share the fault, having a real renderer to ask, but the change
//  banner over the machine is laid out without one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiInfoBanner::EstimateLines (float textWidthPx, const DxuiDpiScaler & scaler) const
{
    float   glyphPx = scaler.ToPxf (s_kFontDip) * s_kEstGlyphEm;
    float   perLine = (glyphPx > 0.0f) ? (textWidthPx / glyphPx) : 1.0f;
    size_t  start   = 0;
    size_t  breakAt = 0;
    int     lines   = 0;



    //  NARROWER THAN A SINGLE GLYPH ESTIMATES NOTHING, and must not fall
    //  through to one line per character: the printer page sizes its banner
    //  from a page rect that is briefly tiny, and a 250-character notice
    //  would ask for 250 lines of height. One line, which is what this
    //  returned before it counted paragraphs.
    if (perLine < 1.0f)
    {
        return 1;
    }

    for (start = 0; start <= m_text.size(); start = breakAt + 1)
    {
        breakAt = m_text.find (L'\n', start);

        if (breakAt == std::wstring::npos)
        {
            breakAt = m_text.size();
        }

        //  An empty paragraph is the blank line between two others, and takes
        //  a line of its own.
        lines += (breakAt > start)
                     ? (int) std::ceil ((float) (breakAt - start) / perLine)
                     : 1;

        if (breakAt == m_text.size())
        {
            break;
        }
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
    float   textWidth = widthPx - padX * 2.0f - iconCol - m_trailingReservePx;
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
    float    textWidth = widthPx - padX * 2.0f - iconCol - m_trailingReservePx;
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
//  DxuiInfoBanner::StrokeCircle
//
//  A ring, walked as chords. Twenty segments is past the point where a mark
//  this size shows corners, and the painter has no arc of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInfoBanner::StrokeCircle (IDxuiPainter & painter, float cx, float cy,
                                   float radiusPx, float strokePx, uint32_t argb)
{
    constexpr int  s_kSegments = 20;
    int            i           = 0;



    for (i = 0; i < s_kSegments; i++)
    {
        float  a0 = 6.2831853f * (float) i       / (float) s_kSegments;
        float  a1 = 6.2831853f * (float) (i + 1) / (float) s_kSegments;

        painter.DrawLineApprox (cx + radiusPx * std::cos (a0), cy + radiusPx * std::sin (a0),
                                cx + radiusPx * std::cos (a1), cy + radiusPx * std::sin (a1),
                                strokePx, argb);
    }
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
//  inviting a click. The info badge is monoline for the same reason: a filled
//  disc is the heaviest mark the strip has.
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
    float    textW    = width - padX * 2.0f - iconBox - iconGap - m_trailingReservePx;
    float    iconR    = iconBox * 0.5f;
    float    iconCx   = left + padX + iconR;
    float    iconCy   = top + height * 0.5f;   // vertically centered in the bordered area



    if (!m_visible)
    {
        return;
    }

    //  CENTERED AS A GROUP when the caller asked for it: the badge, the gap
    //  and the text measured together and slid to the middle. Measured, not
    //  estimated -- Paint has the renderer that Layout does not -- and only
    //  when the line actually fits, so a long notice keeps the wrapping
    //  layout it needs rather than being pushed off the leading edge.
    if (m_centered)
    {
        float  measuredW = 0.0f;
        float  measuredH = 0.0f;
        HRESULT  hrMeasure = text.MeasureString (m_text.c_str(), fontPx,
                                                 DxuiTheme::kBodyFace,
                                                 measuredW, measuredH);

        if (SUCCEEDED (hrMeasure) && measuredW > 0.0f && measuredW <= textW)
        {
            float  groupW = iconBox + iconGap + measuredW;
            float  startX = left + (width - m_trailingReservePx - groupW) * 0.5f;

            if (startX < left + padX)
            {
                startX = left + padX;
            }

            iconCx = startX + iconR;
            textX  = startX + iconBox + iconGap;
            textW  = measuredW;
        }
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
        // Info badge, drawn from primitives (no icon-font dependency, exact
        // centering): a RING with an "i" inside it, one pen weight throughout.
        // Monoline, because a filled accent disc is the heaviest mark on a
        // strip whose whole job is to be noticed without shouting -- and it
        // sat next to line-drawn chrome that shares this weight.
        //
        // Inset by half the stroke so the pen's outer edge lands on the icon
        // box rather than straddling it, and the "i" is proportioned off the
        // ring's inner space so it stays centered at any size.
        float  stroke = iconBox * s_kBadgeStrokeEm;
        float  ringR  = iconR - stroke * 0.5f;
        float  dotR   = stroke * 0.55f;
        float  stemH  = iconR * 0.58f;

        if (stroke < 1.0f)
        {
            stroke = 1.0f;
        }

        StrokeCircle     (painter, iconCx, iconCy, ringR, stroke, theme.Accent());
        painter.FillCircleApprox (iconCx, iconCy - iconR * 0.42f, dotR, theme.Accent());
        painter.FillRect         (iconCx - stroke * 0.5f, iconCy - iconR * 0.12f,
                                  stroke, stemH, theme.Accent());
    }

    // Wrapping body text, VERTICALLY CENTERED IN WHATEVER HEIGHT THE BANNER HAS.
    //
    // Top alignment is right only when the banner was sized to exactly the text
    // -- which is true in a dialog and false in a message bar, where the height
    // is the taller of the text and whatever the bar contains. A single line
    // then sat against the top edge with the whole button's worth of space
    // beneath it. Centering is identical in the sized-to-text case, so nothing
    // that used this before moves.
    hr = text.DrawString (m_text.c_str(),
                          textX,
                          top + padY,
                          (textW > 1.0f) ? textW : 1.0f,
                          height - padY * 2.0f,
                          theme.InfoBannerForeground(),
                          fontPx,
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          true);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
