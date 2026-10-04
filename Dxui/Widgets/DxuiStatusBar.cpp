#include "Pch.h"

#include "DxuiStatusBar.h"

#include "Core/DxuiTextElide.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiShadowedText.h"





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
//  DxuiStatusBar::SetMeter
//
//  The fill is clamped to the meter; a negative fraction hides it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetMeter (size_t index, float fraction, uint32_t argb)
{
    if (index < m_fields.size())
    {
        m_fields[index].meter     = (fraction < 0.0f) ? -1.0f : (std::min) (fraction, 1.0f);
        m_fields[index].meterArgb = argb;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetFill
//
//  The fill is clamped to the field; a negative fraction removes it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetFill (size_t index, float fraction, uint32_t fromArgb, uint32_t toArgb)
{
    if (index < m_fields.size())
    {
        m_fields[index].fill         = (fraction < 0.0f) ? -1.0f : (std::min) (fraction, 1.0f);
        m_fields[index].fillFromArgb = fromArgb;
        m_fields[index].fillToArgb   = toArgb;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetBar
//
//  The bar is clamped to the field; a negative fraction removes it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetBar (size_t index, float fraction, uint32_t fromArgb, uint32_t toArgb)
{
    if (index < m_fields.size())
    {
        m_fields[index].bar         = (fraction < 0.0f) ? -1.0f : (std::min) (fraction, 1.0f);
        m_fields[index].barFromArgb = fromArgb;
        m_fields[index].barToArgb   = toArgb;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::SetWidth
//
//  Takes effect at the next layout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::SetWidth (size_t index, int widthDip)
{
    if (index < m_fields.size())
    {
        m_fields[index].widthDip = widthDip;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::MeasureFieldWidthDip
//
//  Measured at the font's DIP size, so the width is in DIPs; rounded up so
//  the widest text is never elided, with the padding on both sides.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiStatusBar::MeasureFieldWidthDip (IDxuiTextRenderer & text, const std::vector<std::wstring> & texts)
{
    HRESULT  hr      = S_OK;
    float    widest  = 0.0f;
    float    width   = 0.0f;
    float    height  = 0.0f;



    for (const std::wstring & candidate : texts)
    {
        hr = text.MeasureString (candidate.c_str(), kFontDip, DxuiTheme::kBodyFace, width, height);
        CHR (hr);

        widest = (std::max) (widest, width);
    }

    CBR (widest > 0.0f);

Error:
    return SUCCEEDED (hr) ? (int) std::ceil (widest) + 2 * kFieldPadDip : 0;
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
//  DxuiStatusBar::FindFieldAt
//
////////////////////////////////////////////////////////////////////////////////

int DxuiStatusBar::FindFieldAt (POINT point) const
{
    for (size_t i = 0; i < m_fieldRects.size(); i++)
    {
        const RECT &  r = m_fieldRects[i];

        if (point.x >= r.left && point.x < r.right && point.y >= r.top && point.y < r.bottom)
        {
            return (int) i;
        }
    }

    return -1;
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
//  field's text elided to fit. A field that is a meter has its fill drawn
//  first and its text over it with a shadow, so the text reads on the fill's
//  colors and on the band alike.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float    padPx   = m_scaler.ToPxf ((float) kFieldPadDip);
    float    fontPx  = m_scaler.ToPxf (kFontDip);
    float    lineW   = (float) (std::max) (1L, std::lround (m_scaler.ToPxf (1.0f)));
    size_t   i       = 0;
    HRESULT  hr      = S_OK;



    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left),
                      (float) (m_boundsDip.bottom - m_boundsDip.top), theme.StatusBackground());

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), lineW, theme.Divider());

    for (i = 0; i < m_fields.size() && i < m_fieldRects.size(); i++)
    {
        const RECT &  r      = m_fieldRects[i];
        float         boxW   = (float) (r.right - r.left) - 2.0f * padPx;
        std::wstring  shown;

        if (i > 0)
        {
            painter.FillRect ((float) r.left, (float) r.top + lineW * 4.0f, lineW,
                              (float) (r.bottom - r.top) - lineW * 8.0f, theme.Divider());
        }

        if (m_fields[i].meter >= 0.0f)
        {
            PaintMeter (painter, m_fields[i], r, theme);
            boxW -= m_scaler.ToPxf ((float) (m_fields[i].meterWidthDip + kFieldPadDip));
        }

        if (m_fields[i].fill >= 0.0f)
        {
            painter.FillHorizontalGradientRect ((float) r.left, (float) r.top + lineW,
                                                (float) (r.right - r.left) * m_fields[i].fill,
                                                (float) (r.bottom - r.top) - lineW,
                                                m_fields[i].fillFromArgb, m_fields[i].fillToArgb);
        }

        if (m_fields[i].bar >= 0.0f)
        {
            PaintBar (painter, m_fields[i], r);
        }

        if (boxW <= 0.0f || m_fields[i].text.empty())
        {
            continue;
        }

        shown = DxuiTextElide::ToWidth (text, m_fields[i].text, fontPx, DxuiTheme::kBodyFace, boxW, DxuiElide::Tail);

        if (m_fields[i].fill >= 0.0f)
        {
            DxuiShadowedText::PaintShadowed (text, shown.c_str(), (float) r.left + padPx, (float) r.top, boxW,
                                             (float) (r.bottom - r.top), kFillTextArgb, fontPx,
                                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                                             (std::max) (1, m_scaler.ToPx (kShadowReachDip)));
            continue;
        }

        hr = text.DrawString (shown.c_str(), (float) r.left + padPx, (float) r.top, boxW,
                              (float) (r.bottom - r.top), theme.ForegroundMuted(), fontPx,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::PaintMeter
//
//  A track at the field's right end, inset by the field padding and centered
//  on the band, with the filled part from its left.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::PaintMeter (IDxuiPainter & painter, const Field & field, const RECT & fieldRect, const IDxuiTheme & theme) const
{
    float  padPx   = m_scaler.ToPxf ((float) kFieldPadDip);
    float  widthPx = m_scaler.ToPxf ((float) field.meterWidthDip);
    float  highPx  = m_scaler.ToPxf ((float) kMeterHeightDip);
    float  lineW   = (float) (std::max) (1L, std::lround (m_scaler.ToPxf (1.0f)));
    float  left    = (float) fieldRect.right - padPx - widthPx;
    float  top     = (float) fieldRect.top + ((float) (fieldRect.bottom - fieldRect.top) - highPx) / 2.0f;



    if (widthPx <= 0.0f || left < (float) fieldRect.left)
    {
        return;
    }

    painter.FillRect    (left, top, widthPx, highPx, theme.ControlBackground());
    painter.FillRect    (left, top, widthPx * field.meter, highPx, field.meterArgb);
    painter.OutlineRect (left, top, widthPx, highPx, lineW, theme.Border());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::PaintBar
//
//  A thin strip near the field's bottom, inset by the field padding at each
//  side, filled from its left; at least a pixel tall at any DPI.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStatusBar::PaintBar (IDxuiPainter & painter, const Field & field, const RECT & fieldRect) const
{
    float  padPx   = m_scaler.ToPxf ((float) kFieldPadDip);
    float  highPx  = (float) (std::max) (1L, std::lround (m_scaler.ToPxf (kBarHeightDip)));
    float  insetPx = (float) std::lround (m_scaler.ToPxf (kBarInsetDip));
    float  trackPx = (float) (fieldRect.right - fieldRect.left) - 2.0f * padPx;
    float  top     = (float) fieldRect.bottom - insetPx - highPx;



    if (trackPx <= 0.0f || field.bar <= 0.0f)
    {
        return;
    }

    painter.FillHorizontalGradientRect ((float) fieldRect.left + padPx, top, trackPx * field.bar, highPx,
                                        field.barFromArgb, field.barToArgb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::OnMouse
//
//  A left press on a field with a click action runs it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiStatusBar::OnMouse (const DxuiMouseEvent & ev)
{
    int  index = FindFieldAt (ev.positionDip);



    if (index < 0 || !m_fields[(size_t) index].onClick)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        m_fields[(size_t) index].onClick (m_fieldRects[(size_t) index]);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBar::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiStatusBar::GetCursorForPoint (POINT clientPx) const
{
    int  index = FindFieldAt (clientPx);



    return (index >= 0 && m_fields[(size_t) index].onClick) ? IDC_HAND : nullptr;
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
