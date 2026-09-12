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
            fixedPx += m_scaler.ToPx (m_fields[i].widthDip);
        }
    }

    for (i = 0; i < m_fields.size(); i++)
    {
        int  widthPx = ((int) i == stretchAt) ? (std::max) (totalPx - fixedPx, 0)
                                              : m_scaler.ToPx (m_fields[i].widthDip);

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
    float    lineW   = m_scaler.ToPxf (1.0f);
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

        if (boxW <= 0.0f || m_fields[i].text.empty())
        {
            continue;
        }

        shown = DxuiTextElide::ToWidth (text, m_fields[i].text, fontPx, DxuiTheme::kBodyFace, boxW, DxuiElide::Tail);

        hr = text.DrawString (shown.c_str(), (float) r.left + padPx, (float) r.top, boxW,
                              (float) (r.bottom - r.top), theme.ForegroundMuted(), fontPx,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::CenterOnCapHeight,
                              DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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
