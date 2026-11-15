#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/Blocks/BlockFilesView.h"
#include "Ui/DiskInspector/ZoomStops.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesView::SetRows (vector<Row> rows)
{
    m_rows  = std::move (rows);
    m_hover = -1;

    ClampScroll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::GetVisibleRows
//
//  The rows under the headings.
//
////////////////////////////////////////////////////////////////////////////////

int BlockFilesView::GetVisibleRows() const
{
    RECT  bounds = GetBounds();
    int   row    = std::max (1, m_scaler.ToPx ((int) kRowDip));



    return std::max (1, (int) (bounds.bottom - bounds.top) / row - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::FindRowAt
//
////////////////////////////////////////////////////////////////////////////////

int BlockFilesView::FindRowAt (POINT pointPx) const
{
    RECT  bounds = GetBounds();
    int   row    = std::max (1, m_scaler.ToPx ((int) kRowDip));
    int   index  = 0;



    if (PtInRect (&bounds, pointPx) == FALSE || pointPx.y < bounds.top + row)
    {
        return -1;
    }

    index = m_scroll + (int) (pointPx.y - bounds.top - row) / row;

    return index < (int) m_rows.size() ? index : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::ClampScroll
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesView::ClampScroll()
{
    m_scroll = std::clamp (m_scroll, 0, std::max (0, (int) m_rows.size() - GetVisibleRows()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::Paint
//
//  The headings, then a row for each file: its path, its kind, its count of
//  blocks and its first block. The selected file's row is marked.
//
////////////////////////////////////////////////////////////////////////////////

void BlockFilesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds = GetBounds();
    float    row    = m_scaler.ToPxf (kRowDip);
    float    pad    = m_scaler.ToPxf (kPadDip);
    float    size   = m_scaler.ToPxf (kTextDip);
    float    width  = (float) (bounds.right - bounds.left);
    float    first  = m_scaler.ToPxf (kFirstDip);
    float    count  = m_scaler.ToPxf (kCountDip);
    float    kind   = m_scaler.ToPxf (kKindDip);
    float    xFirst = (float) bounds.right - pad - first;
    float    xCount = xFirst - count;
    float    xKind  = xCount - kind;
    float    y      = (float) bounds.top;
    int      i      = 0;
    HRESULT  hr     = S_OK;



    if (!IsVisible())
    {
        return;
    }

    auto  cell = [&] (const std::wstring & value, float x, float w, uint32_t color, DxuiTextHAlign align, DxuiFontWeight weight)
    {
        hr = text.DrawString (value.c_str(), x, y, std::max (0.0f, w), row, color, size, DxuiTheme::kBodyFace, align, DxuiTextVAlign::Center, weight, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    };

    cell (L"Name",        (float) bounds.left + pad, xKind - bounds.left - 2 * pad, theme.HeadingForeground(), DxuiTextHAlign::Left,  DxuiFontWeight::SemiBold);
    cell (L"Kind",        xKind,                     kind,                          theme.HeadingForeground(), DxuiTextHAlign::Left,  DxuiFontWeight::SemiBold);
    cell (L"Blocks",      xCount,                    count - pad,                   theme.HeadingForeground(), DxuiTextHAlign::Right, DxuiFontWeight::SemiBold);
    cell (L"First block", xFirst,                    first,                         theme.HeadingForeground(), DxuiTextHAlign::Right, DxuiFontWeight::SemiBold);
    y += row;

    painter.FillRect ((float) bounds.left, y - 1.0f, width, 1.0f, theme.Divider());

    if (m_rows.empty())
    {
        cell (L"There are no files on this volume.", (float) bounds.left + pad, width - 2 * pad, theme.ForegroundMuted(), DxuiTextHAlign::Left, DxuiFontWeight::Normal);
        return;
    }

    for (i = m_scroll; i < (int) m_rows.size() && y + row <= bounds.bottom; i++)
    {
        if (m_rows[i].path == m_selected)
        {
            painter.FillRect ((float) bounds.left, y, width, row, theme.SelectionBackground());
        }
        else if (i == m_hover)
        {
            painter.FillRect ((float) bounds.left, y, width, row, theme.HoverBackground());
        }

        cell (m_rows[i].path,                                           (float) bounds.left + pad, xKind - bounds.left - 2 * pad, theme.Foreground(),      DxuiTextHAlign::Left,  DxuiFontWeight::Normal);
        cell (m_rows[i].isDirectory ? L"Directory" : L"File",           xKind,                     kind,                          theme.ForegroundMuted(), DxuiTextHAlign::Left,  DxuiFontWeight::Normal);
        cell (NumberFormat::FormatInteger (m_rows[i].blocks),          xCount,                    count - pad,                   theme.Foreground(),      DxuiTextHAlign::Right, DxuiFontWeight::Normal);
        cell (NumberFormat::FormatInteger (m_rows[i].first),           xFirst,                    first,                         theme.Foreground(),      DxuiTextHAlign::Right, DxuiFontWeight::Normal);

        y += row;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockFilesView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool BlockFilesView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  bounds = GetBounds();
    bool  inside = PtInRect (&bounds, ev.positionDip) != FALSE;
    int   index  = FindRowAt (ev.positionDip);



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Leave:
            m_hover = -1;
            return false;

        case DxuiMouseEventKind::Move:
        case DxuiMouseEventKind::Enter:
            m_hover = index;
            return inside;

        case DxuiMouseEventKind::Wheel:
            if (inside && !ev.wheelHorizontal)
            {
                m_scroll -= ZoomStops::GetWheelSteps (ev.wheelDelta) * 3;
                ClampScroll();
                return true;
            }

            return false;

        case DxuiMouseEventKind::Down:
            if (index >= 0 && ev.button == DxuiMouseButton::Left && m_onSelect)
            {
                m_selected = m_rows[index].path;
                m_onSelect (m_rows[index]);
                return true;
            }

            return inside;

        default:
            return false;
    }
}
