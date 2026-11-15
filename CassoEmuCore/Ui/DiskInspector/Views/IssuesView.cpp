#include "Pch.h"

#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"





static constexpr int      s_kToggleDip    = 104;
static constexpr int      s_kToggleGapDip = 4;
static constexpr float    s_kToggleHDip   = 22.0f;
static constexpr int      s_kButtonPadDip = 3;
static constexpr int      s_kHotAll       = -2;
static constexpr LPCWSTR  s_kpszRepair    = L"Repair";
static constexpr LPCWSTR  s_kpszRepairAll = L"Repair disk image...";





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::IssuesView
//
//  The finding columns, the severity as an icon and the finding wrapped to
//  its column's width, and an untitled one at the end for the Repair
//  buttons.
//
////////////////////////////////////////////////////////////////////////////////

IssuesView::IssuesView (InspectorViewContext & context) :
    InspectorTableView (context)
{
    vector<std::wstring>  columns = InspectorTables::GetFindingColumns();
    vector<int>           widths  (columns.size() + 1, 0);



    columns.push_back (L"");
    widths.front() = kSeverityColumnDip;
    widths.back()  = kRepairColumnDip;

    SetColumns    (columns, widths, kFindingColumn);
    SetIconColumn (0);
    SetWrapsText  (true);
    SetOnSort     ([this] (int column) { OnSort (column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Refresh()
{
    vector<TableRow>  rows;



    if (m_context.analysis != nullptr && m_context.hasDisk)
    {
        m_counts = InspectorTables::CountCategories (*m_context.analysis);
        rows     = InspectorTables::BuildFindings (*m_context.analysis, m_mask, m_sortColumn, m_isDescending);
    }
    else
    {
        m_counts = {};
    }

    for (TableRow & row : rows)
    {
        row.cells.push_back (L"");
    }

    SetRows (std::move (rows));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::ShowFinding
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::ShowFinding (int finding)
{
    int  category = 0;



    if (m_context.analysis == nullptr || finding < 0 || finding >= static_cast<int> (m_context.analysis->findings.size()))
    {
        return;
    }

    category = static_cast<int> (m_context.analysis->findings[finding].category);

    if ((m_mask & (1u << category)) == 0)
    {
        m_mask |= (1u << category);
        Refresh();
    }

    SelectRowWhere ([finding] (const TableRow & row) { return row.finding == finding; });

    if (m_select)
    {
        for (const TableRow & row : m_rows)
        {
            if (row.finding == finding)
            {
                m_select (row);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetFixingKeys
//
////////////////////////////////////////////////////////////////////////////////

std::set<std::wstring> IssuesView::GetFixingKeys (int finding) const
{
    std::set<std::wstring>  keys;



    if (m_plan != nullptr && m_plan->canRepair && m_context.analysis != nullptr && finding >= 0 && finding < static_cast<int> (m_context.analysis->findings.size()))
    {
        keys = DiskRepair::GetFixingKeys (*m_plan, m_context.analysis->findings[finding]);
    }

    return keys;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetRepairRect
//
//  The Repair column's cell of a row in view, inset, for a finding a change
//  sets right.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetRepairRect (int row) const
{
    RECT    cell   = {};
    RECT    list   = m_list.GetBounds();
    int     pad    = m_scaler.ToPx (s_kButtonPadDip);
    size_t  column = InspectorTables::GetFindingColumns().size();
    bool    isIn   = row >= 0 && row < static_cast<int> (m_rows.size()) && m_list.GetCellTextRectPx (row, column, cell) && !GetFixingKeys (m_rows[row].finding).empty();



    if (!isIn)
    {
        return RECT {};
    }

    cell = RECT { list.left + cell.left, list.top + cell.top + pad, list.left + cell.right - pad, list.top + (std::min) (cell.bottom, cell.top + m_scaler.ToPx (kLineDip)) - pad };

    if (cell.top < list.top + m_list.GetHeaderHeightPx() || cell.bottom > list.bottom || cell.right <= cell.left)
    {
        return RECT {};
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetSeverityRect
//
//  The severity icon of a row in view, on its first line, centered in its
//  column; empty out of view.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetSeverityRect (int row) const
{
    RECT  cell = {};
    RECT  list = m_list.GetBounds();
    int   size = m_scaler.ToPx (kSeverityIconDip);
    int   line = m_scaler.ToPx (kLineDip);
    int   left = 0;
    int   top  = 0;



    if (row < 0 || row >= static_cast<int> (m_rows.size()) || !m_list.GetCellTextRectPx (row, 0, cell))
    {
        return RECT {};
    }

    left = list.left + (cell.left + cell.right - size) / 2;
    top  = list.top + cell.top + (line - size) / 2;

    if (top < list.top + m_list.GetHeaderHeightPx() || top + size > list.bottom)
    {
        return RECT {};
    }

    return RECT { left, top, left + size, top + size };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetRepairAllRect
//
//  At the right of the toggles while the volume can be repaired; empty
//  otherwise.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetRepairAllRect() const
{
    int   h    = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int   top  = m_boundsDip.top + (GetTopHeightPx() - h) / 2;
    RECT  rect = {};



    if (m_plan != nullptr && m_plan->canRepair)
    {
        rect = RECT { m_boundsDip.right - m_scaler.ToPx (kRepairAllDip), top, m_boundsDip.right, top + h };
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Paint
//
//  The table, then a Repair button over each row whose finding a change
//  sets right.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  rect = {};



    InspectorTableView::Paint (painter, text, theme);

    for (int row = m_list.GetTopRow(); row < static_cast<int> (m_rows.size()); row++)
    {
        rect = GetSeverityRect (row);

        if (rect.right > rect.left && m_rows[row].severity >= 0)
        {
            DxuiSeverityBadge::Draw (painter, static_cast<DxuiSeverity> (m_rows[row].severity), static_cast<float> (rect.left), static_cast<float> (rect.top),
                                     static_cast<float> (rect.right - rect.left), theme);
        }

        rect = GetRepairRect (row);

        if (rect.right > rect.left)
        {
            PaintButton (painter, text, theme, rect, s_kpszRepair, row == m_hotRow);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::PaintButton
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::PaintButton (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & rect, LPCWSTR label, bool isHot) const
{
    float  left   = static_cast<float> (rect.left);
    float  top    = static_cast<float> (rect.top);
    float  width  = static_cast<float> (rect.right - rect.left);
    float  height = static_cast<float> (rect.bottom - rect.top);



    painter.FillRoundedRect    (left, top, width, height, m_scaler.ToPxf (4.0f), isHot ? theme.ButtonHover() : theme.ButtonIdle());
    painter.OutlineRoundedRect (left, top, width, height, m_scaler.ToPxf (4.0f), m_scaler.ToPxf (1.0f), theme.ButtonBorder());
    text.DrawString (label, left, top, width, height, theme.Foreground(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::OnMouse
//
//  A click on a category toggles it in the filter. A Repair button, or
//  "Repair disk image...", opens the confirmation on release over the
//  button it was pressed on.
//
////////////////////////////////////////////////////////////////////////////////

bool IssuesView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = false;
    int   c         = 0;
    int   hot       = -1;
    RECT  r         = {};



    r = GetRepairAllRect();

    if (PtInRect (&r, ev.positionDip))
    {
        hot = s_kHotAll;
    }

    for (int row = m_list.GetTopRow(); row < static_cast<int> (m_rows.size()) && hot == -1; row++)
    {
        r   = GetRepairRect (row);
        hot = PtInRect (&r, ev.positionDip) ? row : -1;
    }

    if (ev.kind == DxuiMouseEventKind::Move || ev.kind == DxuiMouseEventKind::Leave)
    {
        m_hotRow = (ev.kind == DxuiMouseEventKind::Move) ? hot : -1;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && hot != -1)
    {
        m_pressedRow = hot;
        isHandled    = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_pressedRow != -1)
    {
        isHandled = true;

        if (hot == m_pressedRow && m_repair)
        {
            m_repair ((hot == s_kHotAll) ? std::set<std::wstring>() : GetFixingKeys (m_rows[hot].finding));
        }

        m_pressedRow = -1;
    }

    if (!isHandled && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
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
//  out is drawn without its fill. "Repair disk image..." is at the right
//  while the volume can be repaired. With no findings, the list has a line
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

    r = GetRepairAllRect();

    if (r.right > r.left)
    {
        PaintButton (painter, text, theme, r, s_kpszRepairAll, m_hotRow == s_kHotAll);
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
