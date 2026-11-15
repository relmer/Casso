#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockFilesView.h"
#include "Ui/DiskInspector/ZoomStops.h"





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
////////////////////////////////////////////////////////////////////////////////

int BlockFilesView::GetVisibleRows() const
{
    RECT  bounds = GetBounds();
    int   row    = std::max (1, m_scaler.ToPx ((int) kRowDip));



    return std::max (1, (int) (bounds.bottom - bounds.top) / row);
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
////////////////////////////////////////////////////////////////////////////////

void BlockFilesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     bounds = GetBounds();
    float    row    = m_scaler.ToPxf (kRowDip);
    float    pad    = m_scaler.ToPxf (kPadDip);
    float    size   = m_scaler.ToPxf (kTextDip);
    float    width  = (float) (bounds.right - bounds.left);
    float    count  = m_scaler.ToPxf (kCountDip);
    float    y      = (float) bounds.top;
    int      i      = 0;
    HRESULT  hr     = S_OK;



    if (!IsVisible())
    {
        return;
    }

    if (m_rows.empty())
    {
        hr = text.DrawString (L"The volume has no files.", (float) bounds.left + pad, y, width - 2 * pad, row, theme.ForegroundMuted(), size,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    for (i = m_scroll; i < (int) m_rows.size() && y + row <= bounds.bottom; i++)
    {
        if (i == m_hover)
        {
            painter.FillRect ((float) bounds.left, y, width, row, theme.SelectionBackground());
        }

        hr = text.DrawString (m_rows[i].path.c_str(), (float) bounds.left + pad, y, width - count - 3 * pad, row, theme.Foreground(), size,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (std::format (L"{} {}", m_rows[i].blocks, m_rows[i].blocks == 1 ? L"block" : L"blocks").c_str(),
                              (float) bounds.right - count - pad, y, count, row, theme.ForegroundMuted(), size,
                              DxuiTheme::kBodyFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

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
    int   row    = std::max (1, m_scaler.ToPx ((int) kRowDip));
    bool  inside = PtInRect (&bounds, ev.positionDip) != FALSE;
    int   index  = inside ? m_scroll + (int) (ev.positionDip.y - bounds.top) / row : -1;



    index = (index >= 0 && index < (int) m_rows.size()) ? index : -1;

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
                m_onSelect (m_rows[index]);
                return true;
            }

            return inside;

        default:
            return false;
    }
}
