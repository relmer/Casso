#include "Pch.h"

#include "DxuiStatusBar.h"

#include "Core/DxuiTextElide.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiMenuBar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::GetBandDp
//
////////////////////////////////////////////////////////////////////////////////

int DxuiStatusBar::GetBandDp()
{
    return DxuiMenuBar::GetStripHeightPx (DxuiDpiScaler::kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetFields
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetFields (std::vector<Field> fields)
{
    m_fields = std::move (fields);
    m_fieldRects.assign (m_fields.size(), RECT {});

    Layout (m_boundsDip, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetText
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetText (size_t index, std::wstring text)
{
    if (index < m_fields.size())
    {
        m_fields[index].text = std::move (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetShimmer
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetShimmer (size_t index, float phase)
{
    if (index < m_fields.size())
    {
        m_fields[index].shimmer = phase;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::GetFieldRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiStatusBar::GetFieldRect (size_t index) const
{
    return (index < m_fieldRects.size()) ? m_fieldRects[index] : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::Layout
//
//  Fixed fields first, then the stretch field gets the remainder. With no
//  stretch field the fixed ones sit from the left and the rest is empty; with
//  more than one, only the first stretches and the others are treated as
//  fixed, so the widths always add up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int     totalPx   = boundsDip.right - boundsDip.left;
    int     fixedPx   = 0;
    int     stretchAt = -1;
    int     x         = boundsDip.left;
    size_t  i         = 0;



    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
    m_fieldRects.assign (m_fields.size(), RECT {});

    for (i = 0; i < m_fields.size(); i++)
    {
        if (m_fields[i].stretch && stretchAt < 0)
        {
            stretchAt = (int) i;
        }
        else
        {
            fixedPx += (m_fields[i].widthPx >= 0) ? m_fields[i].widthPx : m_scaler.ToPx (m_fields[i].widthDip);
        }
    }

    for (i = 0; i < m_fields.size(); i++)
    {
        int  widthPx = ((int) i == stretchAt) ? (std::max) (totalPx - fixedPx, 0)
                                              : (m_fields[i].widthPx >= 0) ? m_fields[i].widthPx : m_scaler.ToPx (m_fields[i].widthDip);

        m_fieldRects[i] = RECT { x, boundsDip.top, x + widthPx, boundsDip.bottom };
        x += widthPx;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::Paint
//
//  The band, a divider along its top, a divider between fields, and each
//  field's text elided to fit.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float    padPx   = m_scaler.ToPxf ((float) kFieldPadDip);
    float    fontPx  = m_scaler.ToPxf (kFontDip);
    float    lineW   = (float) (std::max) (1L, std::lround (m_scaler.ToPxf (1.0f)));
    float    flowX   = (float) m_boundsDip.left + m_scaler.ToPxf (m_leadDip);
    float    gapPx   = m_scaler.ToPxf (kFlowGapDip);
    float    ruleH   = m_scaler.ToPxf (kFlowRuleDip);
    float    textW   = 0.0f;
    float    textH   = 0.0f;
    size_t   i       = 0;
    HRESULT  hr      = S_OK;



    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left),
                      (float) (m_boundsDip.bottom - m_boundsDip.top), theme.StatusBackground());

    if (m_dividers)
    {
        painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                          (float) (m_boundsDip.right - m_boundsDip.left), lineW, theme.Divider());
    }

    for (i = 0; i < m_fields.size() && i < m_fieldRects.size(); i++)
    {
        const RECT &  r      = m_fieldRects[i];
        float         left   = (float) r.left + padPx;
        float         boxW   = 0.0f;
        std::wstring  shown;

        //  A field after flowing ones starts past their text, which is as
        //  wide as it is rather than as wide as their rectangles. A field
        //  further right starts there too when their text runs past its left
        //  edge, and the field's text is cut short to the room left.
        left = (std::max) (left, flowX);
        boxW = (float) r.right - padPx - left;

        if (i > 0 && m_dividers)
        {
            painter.FillRect ((float) r.left, (float) r.top + lineW * 4.0f, lineW,
                              (float) (r.bottom - r.top) - lineW * 8.0f, theme.Divider());
        }

        if (m_fields[i].text.empty())
        {
            continue;
        }

        //  A flowing field follows the one before it, as wide as its text.
        if (m_fields[i].flow)
        {
            hr = text.MeasureString (m_fields[i].text.c_str(), fontPx, DxuiTheme::GetUiFace(), textW, textH);

            if (FAILED (hr))
            {
                continue;
            }

            hr = text.DrawString (m_fields[i].text.c_str(), flowX, (float) r.top, textW + 1.0f,
                                  (float) (r.bottom - r.top), theme.ForegroundMuted(), fontPx,
                                  DxuiTheme::GetUiFace(), DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                                  DxuiFontWeight::Normal, false);
            IGNORE_RETURN_VALUE (hr, S_OK);

            flowX += std::ceil (textW) + gapPx;
            painter.FillRect (std::floor (flowX), (float) r.top + ((float) (r.bottom - r.top) - ruleH) * 0.5f,
                              lineW, ruleH, theme.ForegroundMuted());
            flowX += lineW + gapPx;
            continue;
        }

        if (boxW <= 0.0f)
        {
            continue;
        }

        shown = DxuiTextElide::ToWidth (text, m_fields[i].text, fontPx, DxuiTheme::GetUiFace(), boxW, DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(), left, (float) r.top, boxW,
                              (float) (r.bottom - r.top), theme.ForegroundMuted(), fontPx,
                              DxuiTheme::GetUiFace(), DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (m_fields[i].shimmer >= 0.0f)
        {
            PaintShimmer (painter, text, theme, shown, m_fields[i].shimmer, left, r, boxW, fontPx);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::PaintShimmer
//
//  The text again in the accent color, through a band that crosses it. The
//  band's edges are dimmer than its middle, so the sweep has no hard line.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::PaintShimmer (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & shown, float phase, float left, const RECT & r, float boxW, float fontPx)
{
    static constexpr float  kAlphas[] = { 0.3f, 0.7f, 1.0f, 0.7f, 0.3f };
    HRESULT                 hr        = S_OK;
    float                   textW     = 0.0f;
    float                   textH     = 0.0f;
    float                   bandW     = m_scaler.ToPxf (kShimmerDip);
    float                   sliceW    = bandW / (float) std::size (kAlphas);
    float                   bandX     = 0.0f;



    hr = text.MeasureString (shown.c_str(), fontPx, DxuiTheme::GetUiFace(), textW, textH);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    textW = (std::min) (textW, boxW);
    bandX = left - bandW + phase * (textW + bandW);

    for (size_t step = 0; step < std::size (kAlphas); step++)
    {
        float     sliceX = bandX + sliceW * (float) step;
        uint32_t  alpha  = (uint32_t) std::lround (255.0f * kAlphas[step]);
        uint32_t  color  = (theme.Accent() & 0x00FFFFFFu) | (alpha << 24);

        painter.PushClipRect (sliceX, (float) r.top, sliceW, (float) (r.bottom - r.top));

        hr = text.DrawString (shown.c_str(), left, (float) r.top, boxW,
                              (float) (r.bottom - r.top), color, fontPx,
                              DxuiTheme::GetUiFace(), DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.PopClipRect();
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::GetAccessibleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiStatusBar::GetAccessibleName() const
{
    std::wstring  name;



    for (const Field & field : m_fields)
    {
        if (field.text.empty())
        {
            continue;
        }

        if (!name.empty())
        {
            name += L", ";
        }

        name += field.text;
    }

    return name;
}
