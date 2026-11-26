#include "Pch.h"

#include "DxuiTooltipBody.h"
#include "Core/DxuiDpiScaler.h"
#include "Render/DxuiRoundedOutline.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DxuiTooltipBody
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltipBody::DxuiTooltipBody (
    const DxuiTooltipLook  & look,
    const std::wstring     & text,
    const DxuiDpiScaler    & scaler,
    float                    fontDip,
    const Ink              & ink) :
    m_look    (look),
    m_text    (text),
    m_scaler  (scaler),
    m_fontDip (fontDip),
    m_ink     (ink)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFace
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiTooltipBody::GetFace (const DxuiTooltipLook & look)
{
    return look.isMonospace ? DxuiTheme::kMonoFace : DxuiTheme::GetUiFace();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetMaxTextWidthDip
//
//  How wide a line may run before it wraps, which for a fixed-width tip is
//  more than twice as wide.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetMaxTextWidthDip (const DxuiTooltipLook & look)
{
    if (look.maxTextWidthDip > 0.0f)
    {
        return look.maxTextWidthDip;
    }

    return look.isMonospace ? kMaxMonoWidthDip : kDefaultMaxTextWidthDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::IsTableTip
//
//  A tip with tabs, line colors, swatches or scales is laid out as a table,
//  so it isn't wrapped.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTooltipBody::IsTableTip() const
{
    return IsTable (m_text) || !m_look.lineArgbs.empty() || !m_look.lineSwatches.empty() || !m_look.lineScales.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFontPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetFontPx() const
{
    return m_scaler.ToPxf (m_fontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFrameLabelPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetFrameLabelPx() const
{
    return GetFontPx() * kFrameLabelScale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFrameLabelHPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetFrameLabelHPx() const
{
    return std::ceil (GetFrameLabelPx() * kFrameLineEm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFrameEdgePx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetFrameEdgePx() const
{
    return std::round (m_scaler.ToPxf (kFrameInsetDip) + GetFrameLabelPx() * kFrameCapEm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetContentMargins
//
//  In a frame, the frame is GetFrameEdgePx from the tip's edge on every side,
//  then the same gap inside the frame to the content, where the top's is
//  from the bottom of the label's letters. With no frame, the tip's padding.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltipBody::Margins DxuiTooltipBody::GetContentMargins (
    float  padXPx,
    float  padYPx) const
{
    float  inset = GetFrameEdgePx();
    float  gap   = m_scaler.ToPxf (kFrameGapDip);



    if (!HasFrame())
    {
        return Margins { padXPx, padYPx, padXPx, padYPx };
    }

    return Margins { inset + gap, inset + GetFrameLabelPx() * kFrameCapEm + gap, inset + gap, inset + gap };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetBarsHeightPx
//
//  The height the scales add under the text: a gap, then for each scale its
//  row and its arrow, with a gap between rows.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetBarsHeightPx() const
{
    float  rows = (float) m_look.bars.size();



    return std::ceil (m_scaler.ToPxf (kGradientGapDip) + rows * (GetBarPx() + m_scaler.ToPxf (kGradientArrowDip)) +
                      (rows - 1.0f) * m_scaler.ToPxf (kBarRowGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetBarsWidthPx
//
//  The width of the scales' rows, which the content is at least: the widest
//  label and its gap, then the scale.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetBarsWidthPx (const TextMeasure & measure) const
{
    return std::ceil (GetBarLabelPx (measure) + m_scaler.ToPxf (kGradientWidthDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetBarPx
//
//  A scale is tall enough for the percentage written on it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetBarPx() const
{
    return std::ceil ((std::max) (m_scaler.ToPxf (kGradientHeightDip), GetBarTextPx() * kBarTextLineEm));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetBarTextPx
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetBarTextPx() const
{
    return GetFontPx() * kBarTextScale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetBarLabelPx
//
//  The widest label and the gap after it, so every scale starts at the same
//  place; none when no scale has a label. Estimated where a label cannot be
//  measured.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetBarLabelPx (const TextMeasure & measure) const
{
    float  widest = 0.0f;



    for (const DxuiTipBar & bar : m_look.bars)
    {
        float    width  = (float) bar.label.size() * GetFontPx() * kEstCharWidthEm;
        float    height = 0.0f;
        HRESULT  hr     = S_OK;



        if (bar.label.empty())
        {
            continue;
        }

        hr = measure (bar.label.c_str(), GetFontPx(), kNoWrapPx, width, height);
        IGNORE_RETURN_VALUE (hr, S_OK);

        widest = (std::max) (widest, std::ceil (width));
    }

    return (widest > 0.0f) ? widest + m_scaler.ToPxf (kBarLabelGapDip) : 0.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetFrameWidthPx
//
//  The narrowest tip that fits the frame's label on its top edge: the label,
//  and on each side the frame's inset, its corner and the gap around the
//  label. Estimated where the label cannot be measured.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetFrameWidthPx (const TextMeasure & measure) const
{
    HRESULT  hr     = S_OK;
    float    labelW = (float) m_look.frameLabel.size() * GetFrameLabelPx() * kEstCharWidthEm;
    float    labelH = 0.0f;



    hr = measure (m_look.frameLabel.c_str(), GetFrameLabelPx(), kNoWrapPx, labelW, labelH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return std::ceil (labelW) + (GetFrameEdgePx() + m_scaler.ToPxf (kFrameCornerDip) + m_scaler.ToPxf (kFrameGapDip)) * 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetGradientColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTooltipBody::GetGradientColor (const std::vector<uint32_t> & stops, float at)
{
    float   scaled = 0.0f;
    size_t  low    = 0;



    if (stops.empty())
    {
        return 0;
    }

    if (stops.size() == 1)
    {
        return stops[0];
    }

    scaled = std::clamp (at, 0.0f, 1.0f) * (float) (stops.size() - 1);
    low    = std::min ((size_t) scaled, stops.size() - 2);

    return DxuiColor::Mix (stops[low], stops[low + 1], scaled - (float) low);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawBars
//
//  The scales' rows from the top of their room down to `bottom`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawBars (IDxuiPainter & painter, IDxuiTextRenderer & text, float left, float width, float bottom) const
{
    auto   measure = [this, &text] (const wchar_t * what, float sizePx, float wrapPx, float & w, float & h) -> HRESULT
    {
        return text.MeasureStringWrapped (what, sizePx, GetFace(), wrapPx, w, h);
    };
    float  labelPx = GetBarLabelPx (measure);
    float  top     = bottom - GetBarsHeightPx() + m_scaler.ToPxf (kGradientGapDip);



    for (const DxuiTipBar & bar : m_look.bars)
    {
        DrawBar (painter, text, bar, labelPx, left, width, top);
        top += GetBarPx() + m_scaler.ToPxf (kGradientArrowDip) + m_scaler.ToPxf (kBarRowGapDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawBar
//
//  The label, then the scale in thin slices across the rest of the width,
//  and under it an arrow whose point touches the scale at the mark, in the
//  tip's text color. The mark's percentage is written on the scale over the
//  arrow, moved in from an end it would run past, in an ink that reads on
//  the scale's color there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawBar (IDxuiPainter & painter, IDxuiTextRenderer & text, const DxuiTipBar & bar, float labelPx, float left, float width, float top) const
{
    float         arrowPx  = m_scaler.ToPxf (kGradientArrowDip);
    float         barPx    = GetBarPx();
    float         barLeft  = left + labelPx;
    float         barWidth = width - labelPx;
    float         slice    = barWidth / (float) kGradientSlices;
    float         tipX     = barLeft + barWidth * bar.mark;
    float         padPx    = m_scaler.ToPxf (kBarTextPadDip);
    float         textW    = 0.0f;
    float         textH    = 0.0f;
    float         textX    = 0.0f;
    std::wstring  percent;
    HRESULT       hr       = S_OK;



    if (!bar.label.empty())
    {
        hr = text.DrawString (bar.label.c_str(), left, top, labelPx, barPx, m_ink.textArgb, GetFontPx(), GetFace(),
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (int i = 0; i < kGradientSlices; i++)
    {
        float  at = ((float) i + 0.5f) / (float) kGradientSlices;

        painter.FillRect (barLeft + slice * (float) i, top, slice + 0.5f, barPx, GetGradientColor (bar.stops, at));
    }

    painter.OutlineRect (barLeft, top, barWidth, barPx, 1.0f, m_ink.borderArgb);

    if (bar.mark < 0.0f || bar.mark > 1.0f)
    {
        return;
    }

    painter.FillConvexQuad (tipX,           top + barPx,
                            tipX + arrowPx, top + barPx + arrowPx,
                            tipX - arrowPx, top + barPx + arrowPx,
                            tipX,           top + barPx,
                            m_ink.textArgb);

    percent = std::format (L"{}%", std::lround (bar.mark * 100.0f));
    hr      = text.MeasureStringWrapped (percent.c_str(), GetBarTextPx(), GetFace(), kNoWrapPx, textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    textX = std::clamp (tipX - textW / 2.0f, barLeft + padPx, (std::max) (barLeft + padPx, barLeft + barWidth - padPx - textW));

    hr = text.DrawString (percent.c_str(), textX, top, textW + 1.0f, barPx,
                          DxuiColor::ComputeInkForContrast (m_ink.textArgb, GetGradientColor (bar.stops, (textX + textW / 2.0f - barLeft) / barWidth), kMinTextContrast) | 0xFF000000u,
                          GetBarTextPx(), GetFace(), DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetEdgeArgb
//
//  The balloon's border. Around a frame the tip's edge is drawn close to its
//  fill, so the frame is the border that reads.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTooltipBody::GetEdgeArgb() const
{
    if (!HasFrame())
    {
        return m_ink.borderArgb;
    }

    return DxuiColor::Mix (m_ink.backArgb, m_ink.borderArgb, kFramedEdgeMix) | 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetCornerTagPx
//
//  The room the tag takes on the first line: its width and a gap before it.
//  Estimated where the tag cannot be measured.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetCornerTagPx (const TextMeasure & measure) const
{
    float    fontPx = GetFontPx();
    float    width  = (float) m_look.cornerTag.size() * fontPx * kEstCharWidthEm;
    float    height = 0.0f;
    HRESULT  hr     = S_OK;



    if (m_look.cornerTag.empty())
    {
        return 0.0f;
    }

    hr = measure (m_look.cornerTag.c_str(), fontPx, kNoWrapPx, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return std::ceil (width) + m_scaler.ToPxf (kFrameGapDip) * 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawCornerTag
//
//  Right-aligned against `right` on the first line, over whatever the line
//  has there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawCornerTag (IDxuiTextRenderer & text, float right, float top, float lineHeight) const
{
    float     fontPx  = GetFontPx();
    uint32_t  ink     = (m_look.cornerTagArgb != 0) ? (m_look.cornerTagArgb | 0xFF000000u) : m_ink.textArgb;
    HRESULT   hr      = S_OK;
    float     width   = 0.0f;
    auto      measure = [this, &text] (const wchar_t * what, float sizePx, float wrapPx, float & w, float & h) -> HRESULT
    {
        return text.MeasureStringWrapped (what, sizePx, GetFace(), wrapPx, w, h);
    };



    if (m_look.cornerTag.empty())
    {
        return;
    }

    width = GetCornerTagPx (measure);

    hr =text.DrawString (m_look.cornerTag.c_str(), right - width, top, width, lineHeight,
                          DxuiColor::ComputeInkForContrast (ink, m_ink.backArgb, kMinTextContrast) | 0xFF000000u,
                          fontPx, GetFace(), DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawFrame
//
//  The outline GetFrameEdgePx from the tip's edge on every side, its top
//  edge through the middle of the label's line, and the label on it from
//  the left, past the corner's curve, over a patch of the tip's background
//  that breaks the line there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawFrame (IDxuiPainter & painter, IDxuiTextRenderer & text, float width, float height) const
{
    float                    labelPx = GetFrameLabelPx();
    float                    labelH  = GetFrameLabelHPx();
    float                    labelW  = 0.0f;
    float                    measH   = 0.0f;
    float                    inset   = GetFrameEdgePx();
    float                    left    = inset;
    float                    right   = width  - inset;
    float                    top     = GetFrameEdgePx();
    float                    bottom  = height - inset;
    float                    pad     = m_scaler.ToPxf (kFrameGapDip) / 2.0f;
    float                    labelX  = left + m_scaler.ToPxf (kFrameCornerDip) + pad;
    uint32_t                 ink     = (m_look.frameArgb != 0) ? (m_look.frameArgb | 0xFF000000u) : m_ink.textArgb;
    uint32_t                 stroke  = (m_look.frameArgb != 0) ? (DxuiColor::Mix (m_ink.backArgb, ink, kFrameMix) | 0xFF000000u) : m_ink.borderArgb;
    HRESULT                  hr      = S_OK;



    ink    = DxuiColor::ComputeInkForContrast (ink,    m_ink.backArgb, kMinTextContrast) | 0xFF000000u;
    stroke = DxuiColor::ComputeInkForContrast (stroke, m_ink.backArgb, kMinLineContrast) | 0xFF000000u;



    for (const DxuiRoundedOutline::Segment & segment : DxuiRoundedOutline::GetSegments ({ { left, top }, { right, top }, { right, bottom }, { left, bottom } },
                                                                                      m_scaler.ToPxf (kFrameCornerDip)))
    {
        painter.DrawLine (segment.from.x, segment.from.y, segment.to.x, segment.to.y, 1.0f, stroke);
    }

    hr = text.MeasureStringWrapped (m_look.frameLabel.c_str(), labelPx, GetFace(), kNoWrapPx, labelW, measH);

    if (FAILED (hr) || labelW <= 0.0f)
    {
        labelW = (float) m_look.frameLabel.size() * labelPx * kEstCharWidthEm;
    }

    labelW += pad * 2.0f;

    painter.FillRect (std::round (labelX - pad), top - labelH / 2.0f, std::ceil (labelW), labelH, m_ink.backArgb);

    hr = text.DrawString (m_look.frameLabel.c_str(), std::round (labelX - pad), top - labelH / 2.0f, std::ceil (labelW), labelH,
                          ink, labelPx, GetFace(), DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::LayOutTable
//
//  Each column is as wide as its widest part; a column starts one gap
//  after the one before it ends.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTooltipBody::LayOutTable (const std::wstring & text, float gapPx, const MeasureFn & measure, TableLayout & table)
{
    HRESULT  hr    = S_OK;
    float    spanW = 0.0f;
    float    partW = 0.0f;
    float    partH = 0.0f;
    size_t   start = 0;
    size_t   end   = 0;
    size_t   from  = 0;
    size_t   tab   = 0;
    size_t   c     = 0;



    table = TableLayout();

    while (start <= text.size())
    {
        std::vector<std::wstring>  parts;
        std::wstring               line;



        end  = text.find (L'\n', start);
        line = text.substr (start, (end == std::wstring::npos) ? std::wstring::npos : end - start);
        from = 0;

        while ((tab = line.find (L'\t', from)) != std::wstring::npos)
        {
            parts.push_back (line.substr (from, tab - from));
            from = tab + 1;
        }

        parts.push_back (line.substr (from));
        table.lines.push_back (std::move (parts));

        start = (end == std::wstring::npos) ? text.size() + 1 : end + 1;
    }

    for (const std::vector<std::wstring> & parts : table.lines)
    {
        for (c = 0; c < parts.size(); c++)
        {
            partW = 0.0f;
            partH = 0.0f;

            if (!parts[c].empty())
            {
                hr = measure (parts[c], partW, partH);
                CHR (hr);
            }

            table.lineHeight = (std::max) (table.lineHeight, std::ceil (partH));

            if (parts.size() == 1)
            {
                spanW = (std::max) (spanW, std::ceil (partW));
                continue;
            }

            if (table.columnW.size() <= c)
            {
                table.columnW.resize (c + 1, 0.0f);
            }

            table.columnW[c] = (std::max) (table.columnW[c], std::ceil (partW));
        }
    }

    for (c = 0; c < table.columnW.size(); c++)
    {
        table.columnX.push_back ((c == 0) ? 0.0f : table.columnX[c - 1] + table.columnW[c - 1] + std::ceil (gapPx));
    }

    table.width = table.columnX.empty() ? spanW : (std::max) (spanW, table.columnX.back() + table.columnW.back());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::LayOutTableWith
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTooltipBody::LayOutTableWith (IDxuiTextRenderer & text, TableLayout & table) const
{
    float  fontPx = GetFontPx();



    return LayOutTable (m_text, fontPx, [this, &text, fontPx] (const std::wstring & part, float & w, float & h)
    {
        return text.MeasureStringWrapped (part.c_str(), fontPx, GetFace(), kNoWrapPx, w, h);
    }, table);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawTable
//
//  A line's swatch is a square a little shorter than the line, centered on
//  it, in the indent before the text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawTable (
    IDxuiPainter             & painter,
    IDxuiTextRenderer        & text,
    const TableLayout        & table,
    float                      left,
    float                      top) const
{
    constexpr float  kSwatchShare = 0.6f;
    HRESULT          hr           = S_OK;
    float            fontPx       = GetFontPx();
    float            indent       = GetSwatchIndentPx (table.lineHeight);
    float            side         = std::round (table.lineHeight * kSwatchShare);
    float            y            = top;
    float            x            = 0.0f;
    float            w            = 0.0f;



    for (size_t line = 0; line < table.lines.size(); line++)
    {
        const std::vector<std::wstring>  & parts = table.lines[line];
        uint32_t                           argb  = DxuiColor::ComputeInkForContrast ((line < m_look.lineArgbs.size() && m_look.lineArgbs[line] != 0) ? m_look.lineArgbs[line] : m_ink.textArgb,
                                                                                     m_ink.backArgb, kMinTextContrast);
        uint32_t                           edge  = DxuiColor::ComputeInkForContrast (m_ink.borderArgb, m_ink.backArgb, kMinLineContrast) | 0xFF000000u;
        bool                               scale = line < m_look.lineScales.size() && !m_look.lineScales[line].empty();

        indent = scale ? GetLineIndentPx (table.lineHeight) : GetSwatchIndentPx (table.lineHeight);

        if (line < m_look.lineSwatches.size() && m_look.lineSwatches[line] != 0)
        {
            float  top = y + std::round ((table.lineHeight - side) / 2.0f);

            if (m_look.lineSwatches[line] == DxuiTooltipSwatch::kHatchedArgb)
            {
                PaintHatch (painter, left, top, side);
            }
            else
            {
                painter.FillRect (left, top, side, side, m_look.lineSwatches[line]);
            }

            painter.OutlineRect (left, top, side, side, 1.0f, edge);
        }

        if (scale)
        {
            DrawLineScale (painter, m_look.lineScales[line], left, y + std::round ((table.lineHeight - side) / 2.0f), side);
        }

        for (size_t c = 0; c < parts.size(); c++)
        {
            bool  isSpan = parts.size() == 1;

            x  = isSpan ? 0.0f        : table.columnX[c];
            w  = isSpan ? table.width : table.width - table.columnX[c];
            hr = text.DrawString (parts[c].c_str(), left + indent + x, y, w, table.lineHeight, argb, fontPx, GetFace());
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        y += table.lineHeight;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::DrawLineScale
//
//  A line's scale in thin slices from its first color on the left to its
//  last on the right, heightPx high, outlined in the tip's border color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::DrawLineScale (IDxuiPainter & painter, const std::vector<uint32_t> & stops, float left, float top, float heightPx) const
{
    float  width = std::round (m_scaler.ToPxf (kLineScaleWidthDip));
    float  slice = width / (float) kGradientSlices;



    for (int i = 0; i < kGradientSlices; i++)
    {
        painter.FillRect (left + slice * (float) i, top, slice + 0.5f, heightPx, GetGradientColor (stops, ((float) i + 0.5f) / (float) kGradientSlices));
    }

    painter.OutlineRect (left, top, width, heightPx, 1.0f, m_ink.borderArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetLineIndentPx
//
//  The room before a line with a scale: the scale's width and a gap of half
//  a line. With no scales, a swatch's indent.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetLineIndentPx (float lineHeight) const
{
    bool  hasScales = std::ranges::any_of (m_look.lineScales, [] (const std::vector<uint32_t> & stops) { return !stops.empty(); });



    return hasScales ? std::round (m_scaler.ToPxf (kLineScaleWidthDip) + lineHeight * 0.5f) : GetSwatchIndentPx (lineHeight);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::GetSwatchIndentPx
//
//  The room before a line for its swatch and the gap after it, a line's
//  height; none when the tip has no swatches.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiTooltipBody::GetSwatchIndentPx (float lineHeight) const
{
    constexpr float  kIndentShare = 1.0f;



    return m_look.lineSwatches.empty() ? 0.0f : std::round (lineHeight * kIndentShare);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody::PaintHatch
//
//  Light, with dark diagonal stripes two pixels wide every four, a pixel
//  at a time.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltipBody::PaintHatch (IDxuiPainter & painter, float left, float top, float side)
{
    int  count = (int) side;



    painter.FillRect (left, top, side, side, DxuiTooltipSwatch::kHatchLight);

    for (int y = 0; y < count; y++)
    {
        for (int x = 0; x < count; x++)
        {
            if ((x + y) % 4 < 2)
            {
                painter.FillRect (left + (float) x, top + (float) y, 1.0f, 1.0f, DxuiTooltipSwatch::kHatchDark);
            }
        }
    }
}
