#include "Pch.h"

#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"





static constexpr int    s_kToggleDip    = 104;
static constexpr int    s_kToggleGapDip = 4;
static constexpr float  s_kToggleHDip   = 22.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::IssuesView
//
////////////////////////////////////////////////////////////////////////////////

IssuesView::IssuesView (InspectorViewContext & context) :
    InspectorTableView (context)
{
    SetColumns (InspectorTables::GetFindingColumns());
    SetOnSort  ([this] (int column) { OnSort (column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Refresh()
{
    if (m_context.analysis != nullptr && m_context.hasDisk)
    {
        m_counts = InspectorTables::CountCategories (*m_context.analysis);
        SetRows (InspectorTables::BuildFindings (*m_context.analysis, m_mask, m_sortColumn, m_isDescending));
    }
    else
    {
        m_counts = {};
        SetRows ({});
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::OnMouse
//
//  A click on a category toggles it in the filter.
//
////////////////////////////////////////////////////////////////////////////////

bool IssuesView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = false;
    int   c         = 0;
    RECT  r         = {};



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        for (c = 0; c < InspectorTables::kCategoryCount && !isHandled; c++)
        {
            r = GetToggleRect (c);

            if (m_counts[c] > 0 && PtInRect (&r, ev.positionDip))
            {
                m_mask   ^= (1u << c);
                isHandled = true;
                Refresh();
            }
        }
    }

    if (!isHandled)
    {
        isHandled = InspectorTableView::OnMouse (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetTopHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int IssuesView::GetTopHeightPx() const
{
    return m_scaler.ToPx (kFilterDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::PaintTop
//
//  A toggle per category that has findings, with its count; one filtered
//  out is drawn without its fill. With no findings, the list has a line
//  to that effect.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    int           c       = 0;
    int           total   = 0;
    RECT          r       = {};
    bool          isOn    = false;
    std::wstring  label;



    for (c = 0; c < InspectorTables::kCategoryCount; c++)
    {
        total += m_counts[c];

        if (m_counts[c] == 0)
        {
            continue;
        }

        r     = GetToggleRect (c);
        isOn  = (m_mask & (1u << c)) != 0;
        label = std::format (L"{} {}", FindingFormatter::FormatCategory (static_cast<FindingCategory> (c)), m_counts[c]);

        painter.FillRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                 static_cast<float> (r.bottom - r.top) / 2, isOn ? theme.SelectionBackground() : theme.Background());
        painter.OutlineRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                    static_cast<float> (r.bottom - r.top) / 2, m_scaler.ToPxf (1.0f), theme.ButtonBorder());
        text.DrawString (label.c_str(), static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                         isOn ? theme.Foreground() : theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    SetEmptyText ((total == 0 && m_context.hasDisk) ? L"No findings" : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::OnSort
//
//  A second click on the same column reverses it.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::OnSort (int column)
{
    m_isDescending = (column == m_sortColumn) ? !m_isDescending : false;
    m_sortColumn   = column;

    SetSortIndicator (m_sortColumn, m_isDescending);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetToggleRect
//
//  Toggles sit left to right in category order, skipping empty ones.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetToggleRect (int category) const
{
    int  w     = m_scaler.ToPx (s_kToggleDip);
    int  gap   = m_scaler.ToPx (s_kToggleGapDip);
    int  h     = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int  shown = 0;
    int  c     = 0;
    int  left  = 0;
    int  top   = m_boundsDip.top + (GetTopHeightPx() - h) / 2;



    for (c = 0; c < category; c++)
    {
        shown += (m_counts[c] > 0) ? 1 : 0;
    }

    left = m_boundsDip.left + shown * (w + gap);

    return { left, top, left + w, top + h };
}
