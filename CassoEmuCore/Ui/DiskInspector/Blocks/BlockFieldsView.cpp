#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockFieldsView.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::SetFields
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldsView::SetFields (vector<BlockField> fields)
{
    m_fields   = std::move (fields);
    m_top      = 0;
    m_hoverRow = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::UpdateFields
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldsView::UpdateFields (vector<BlockField> fields)
{
    m_fields   = std::move (fields);
    m_top      = std::clamp (m_top, 0, std::max (0, (int) m_fields.size() - GetRowsShown()));
    m_hoverRow = m_hoverRow < (int) m_fields.size() ? m_hoverRow : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::FormatRow
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockFieldsView::FormatRow (const BlockField & field)
{
    std::wstring  text = field.label;



    if (!field.value.empty())
    {
        text += L": " + field.value;
    }

    if (field.link != BlockField::kNoLink)
    {
        text += std::wstring (L"  ") + s_kpszRightArrow + L" block " + NumberFormat::FormatInteger (field.link);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldsView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsPx);

    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::GetRowsShown
//
//  The heading takes the first row.
//
////////////////////////////////////////////////////////////////////////////////

int BlockFieldsView::GetRowsShown() const
{
    RECT   bounds = GetBounds();
    float  row    = m_scaler.ToPxf (kRowDip);



    return row <= 0.0f ? 0 : std::max (0, (int) ((bounds.bottom - bounds.top) / row) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::FindRowAt
//
////////////////////////////////////////////////////////////////////////////////

int BlockFieldsView::FindRowAt (POINT pointPx) const
{
    RECT   bounds = GetBounds();
    float  row    = m_scaler.ToPxf (kRowDip);
    int    index  = 0;



    if (row <= 0.0f || pointPx.x < bounds.left || pointPx.x >= bounds.right || pointPx.y < bounds.top + row || pointPx.y >= bounds.bottom)
    {
        return -1;
    }

    index = m_top + (int) ((pointPx.y - bounds.top - row) / row);

    return index < (int) m_fields.size() ? index : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockFieldsView::GetCursorForPoint (POINT clientPx) const
{
    int  row = FindRowAt (clientPx);



    return row >= 0 && m_fields[row].link != BlockField::kNoLink ? IDC_HAND : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool BlockFieldsView::OnMouse (const DxuiMouseEvent & ev)
{
    int  row     = FindRowAt (ev.positionDip);
    int  maximum = std::max (0, (int) m_fields.size() - GetRowsShown());



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Wheel:
        {
            RECT  bounds = GetBounds();

            if (ev.wheelHorizontal || !PtInRect (&bounds, ev.positionDip))
            {
                return false;
            }

            m_top = std::clamp (m_top - (int) std::lround (ev.wheelDelta * kWheelRows), 0, maximum);
            return true;
        }

        case DxuiMouseEventKind::Leave:
            row = -1;
            [[fallthrough]];

        case DxuiMouseEventKind::Move:
        case DxuiMouseEventKind::Enter:
            if (row != m_hoverRow)
            {
                m_hoverRow = row;

                if (m_onHover)
                {
                    m_onHover (row >= 0 ? &m_fields[row] : nullptr);
                }
            }

            return false;

        case DxuiMouseEventKind::Down:
            if (ev.button == DxuiMouseButton::Left && row >= 0 && m_fields[row].link != BlockField::kNoLink && m_onFollow)
            {
                m_onFollow (&m_fields[row]);
                return true;
            }

            return false;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldsView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT      bounds = GetBounds();
    float     left   = (float) bounds.left;
    float     width  = (float) (bounds.right - bounds.left);
    float     row    = m_scaler.ToPxf (kRowDip);
    float     font   = m_scaler.ToPxf (kFontDip);
    float     offset = m_scaler.ToPxf (kOffsetDip);
    float     y      = (float) bounds.top;
    int       shown  = GetRowsShown();
    int       i      = 0;
    HRESULT   hr     = S_OK;
    wchar_t   at[16] = {};



    if (!IsVisible())
    {
        return;
    }

    hr = text.DrawString (m_heading.c_str(), left, y, width, row, theme.HeadingForeground(), font, s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_fields.empty() && !m_emptyText.empty())
    {
        hr = text.DrawString (m_emptyText.c_str(), left, y + row, width, row, theme.ForegroundMuted(), font, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (i = 0; i < shown && m_top + i < (int) m_fields.size(); i++)
    {
        const BlockField &  field = m_fields[m_top + i];

        y = bounds.top + row * (i + 1);

        if (m_top + i == m_hoverRow)
        {
            painter.FillRect (left, y, width, row, theme.HoverBackground());
        }

        swprintf_s (at, L"+$%03X", field.offset);

        hr = text.DrawString (at, left, y, offset, row, theme.ForegroundMuted(), font, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (FormatRow (field).c_str(), left + offset, y, width - offset, row,
                              field.link != BlockField::kNoLink ? theme.Accent() : theme.Foreground(), font, s_kpszFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::MeasureWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int BlockFieldsView::MeasureWidthPx (IDxuiTextRenderer & text) const
{
    float    font   = m_scaler.ToPxf (kFontDip);
    float    widest = 0.0f;
    float    width  = 0.0f;
    float    height = 0.0f;
    HRESULT  hr     = S_OK;



    hr = text.MeasureStringWeighted (m_heading.c_str(), font, s_kpszFont, DxuiFontWeight::SemiBold, widest, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const BlockField & field : m_fields)
    {
        hr = text.MeasureString (FormatRow (field).c_str(), font, s_kpszFont, width, height);

        if (SUCCEEDED (hr))
        {
            widest = std::max (widest, m_scaler.ToPxf (kOffsetDip) + width);
        }
    }

    return (int) std::ceil (widest) + m_scaler.ToPx ((int) kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::GetClippedRowRect
//
////////////////////////////////////////////////////////////////////////////////

bool BlockFieldsView::GetClippedRowRect (int row, IDxuiTextRenderer & text, RECT & outRect) const
{
    RECT     bounds = GetBounds();
    int      rowPx  = m_scaler.ToPx ((int) kRowDip);
    int      top    = 0;
    float    width  = 0.0f;
    float    height = 0.0f;
    HRESULT  hr     = S_OK;



    if (!IsVisible() || row < m_top || row >= (int) m_fields.size() || row - m_top >= GetRowsShown())
    {
        return false;
    }

    hr = text.MeasureString (FormatRow (m_fields[(size_t) row]).c_str(), m_scaler.ToPxf (kFontDip), s_kpszFont, width, height);

    if (FAILED (hr) || bounds.left + m_scaler.ToPxf (kOffsetDip) + width <= bounds.right)
    {
        return false;
    }

    top     = (int) bounds.top + rowPx * (row - m_top + 1);
    outRect = { bounds.left, top - 1, bounds.left + (LONG) std::ceil (m_scaler.ToPxf (kOffsetDip) + width) + m_scaler.ToPx ((int) kPadDip), top + rowPx + 1 };

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFieldsView::PaintRowTip
//
//  The row's offset and text at the places the row has them, inside the
//  tooltip's border, as the tree's tip draws a name.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFieldsView::PaintRowTip (int row, IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    RECT     box    = {};
    float    rowPx  = m_scaler.ToPxf (kRowDip);
    float    font   = m_scaler.ToPxf (kFontDip);
    float    offset = m_scaler.ToPxf (kOffsetDip);
    HRESULT  hr     = S_OK;
    wchar_t  at[16] = {};



    if (!GetClippedRowRect (row, text, box))
    {
        return;
    }

    const BlockField  & field = m_fields[(size_t) row];

    painter.OutlineRect (0.0f, 0.0f, (float) (box.right - box.left), (float) (box.bottom - box.top), 1.0f, theme.TooltipBorder());

    swprintf_s (at, L"+$%03X", field.offset);

    hr = text.DrawString (at, 0.0f, 1.0f, offset, rowPx, theme.ForegroundMuted(), font, s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (FormatRow (field).c_str(), offset, 1.0f, (float) (box.right - box.left) - offset, rowPx,
                          field.link != BlockField::kNoLink ? theme.Accent() : theme.Foreground(), font, s_kpszFont,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
