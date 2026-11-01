#include "Pch.h"

#include "Ui/DiskInspector/InspectorTableView.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"





static constexpr float  s_kEmptyPadDip = 8.0f;
static constexpr std::wstring_view  s_kHeadingIndent = L"    ";
static constexpr std::wstring_view  s_kRowIndent     = L"      ";





////////////////////////////////////////////////////////////////////////////////
//
//  IsGroupRow
//
//  A heading in view: its value is empty and its title has the heading's indent.
//
////////////////////////////////////////////////////////////////////////////////

static bool IsGroupRow (const TableRow & row)
{
    return row.cells.size() > 1 && row.cells[1].empty() && row.cells[0].starts_with (s_kHeadingIndent) && !row.cells[0].starts_with (s_kRowIndent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::InspectorTableView
//
//  A selection made by code (SelectRowWhere) does not call the select
//  function, so following the window's selection never loops back to it.
//
////////////////////////////////////////////////////////////////////////////////

InspectorTableView::InspectorTableView (InspectorViewContext & context) :
    InspectorView (context)
{
    m_list.SetShowHeader (true);
    m_list.SetAlwaysShowSelection (true);
    m_list.SetHorizontalScrollEnabled (true);
    m_list.SetOnSelectionChanged ([this] (int row)
    {
        if (m_isGrouped && !m_isSelecting && row >= 0 && row < static_cast<int> (m_rows.size()) && IsGroupRow (m_rows[row]))
        {
            m_toggle = m_rows[row].cells[0].substr (s_kHeadingIndent.size());
        }
        else if (!m_isSelecting && m_select && row >= 0 && row < static_cast<int> (m_rows.size()))
        {
            m_select (m_rows[row]);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::SetColumns (const vector<std::wstring> & titles)
{
    vector<DxuiListView::Column>  columns;



    m_titles = titles;

    for (const std::wstring & title : titles)
    {
        DxuiListView::Column  column;

        column.title = title;
        column.elide = DxuiElide::Tail;
        columns.push_back (column);
    }

    columns.back().stretch = true;
    m_list.SetColumns (std::move (columns));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::SetRows (vector<TableRow> rows)
{
    m_allRows = std::move (rows);
    ShowRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::ShowRows
//
//  The rows in view: all of them, or with groups, each group's heading with
//  a chevron and, unless it is collapsed, its rows indented.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::ShowRows()
{
    vector<vector<DxuiListView::Cell>>  cells;
    bool                                isOpen = true;



    m_rows.clear();

    for (const TableRow & row : m_allRows)
    {
        if (!m_isGrouped)
        {
            m_rows.push_back (row);
        }
        else if (row.cells.size() > 1 && !row.cells[0].empty() && row.cells[1].empty())
        {
            isOpen = !m_collapsed.contains (row.cells[0]);
            m_rows.push_back (row);
            m_rows.back().cells[0] = std::wstring (s_kHeadingIndent) + row.cells[0];
        }
        else if (isOpen)
        {
            m_rows.push_back (row);
            m_rows.back().cells[0] = std::wstring (s_kRowIndent) + row.cells[0];
        }
    }

    for (const TableRow & row : m_rows)
    {
        vector<DxuiListView::Cell>  line;

        for (const std::wstring & text : row.cells)
        {
            DxuiListView::Cell  cell;

            cell.text     = text;
            cell.isStrong  = m_isGrouped && IsGroupRow (row);
            cell.isHeading = cell.isStrong && &text == &row.cells[0];
            cell.isOpen    = cell.isHeading && !m_collapsed.contains (row.cells[0].substr (s_kHeadingIndent.size()));

            //  A path is cut from its start, so the file's name at its end
            //  is visible.
            cell.elide = (text.size() > 2 && (text[1] == L':' || text.starts_with (L"\\\\"))) ? DxuiElide::PathHead : DxuiElide::None;
            line.push_back (cell);
        }

        cells.push_back (std::move (line));
    }

    m_list.SetRows (std::move (cells));
    m_list.UpdateAutoFitFromRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::ApplyToggle
//
//  Collapses or expands the group whose heading was clicked, once the list is
//  done with the click.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::ApplyToggle()
{
    if (!m_toggle.empty())
    {
        if (!m_collapsed.erase (m_toggle))
        {
            m_collapsed.insert (m_toggle);
        }

        m_toggle.clear();

        m_isSelecting = true;
        m_list.SetSelectedRow (-1);
        m_isSelecting = false;

        ShowRows();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::SetOnSort
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::SetOnSort (SortFn sort)
{
    m_list.SetOnSortColumn (std::move (sort));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::SelectRowWhere
//
//  Selects and scrolls to the first row that matches, or clears the
//  selection when none does.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::SelectRowWhere (const std::function<bool (const TableRow & row)> & isMatch)
{
    int  found = -1;
    int  i     = 0;



    for (i = 0; i < static_cast<int> (m_rows.size()) && found < 0; i++)
    {
        found = isMatch (m_rows[i]) ? i : -1;
    }

    if (found != m_list.GetSelectedRow())
    {
        m_isSelecting = true;
        m_list.SetSelectedRow (found);
        m_isSelecting = false;

        if (found >= 0)
        {
            m_list.EnsureVisible (found);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  list = boundsDip;



    InspectorView::Layout (boundsDip, scaler);

    list.top = std::min (list.bottom, list.top + GetTopHeightPx());
    m_list.Layout (list, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  top = m_boundsDip;



    top.bottom = top.top + GetTopHeightPx();

    if (top.bottom > top.top)
    {
        PaintTop (painter, text, theme, top);
    }

    m_list.SetTheme (&theme);
    m_list.Paint (painter, text, theme);

    //  An empty list has its line where its first row would be.
    if (m_rows.empty() && !m_emptyText.empty())
    {
        RECT   list = m_list.GetBounds();
        float  pad  = m_scaler.ToPxf (s_kEmptyPadDip);
        float  top  = static_cast<float> (list.top + m_list.GetHeaderHeightPx()) + pad;

        text.DrawString (m_emptyText.c_str(), static_cast<float> (list.left) + pad, top, static_cast<float> (list.right - list.left) - 2 * pad,
                         static_cast<float> (list.bottom) - top, theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTableView::OnMouse (const DxuiMouseEvent & ev)
{
    RECT            list   = m_list.GetBounds();
    DxuiMouseEvent  listEv = ev;



    //  The list hit-tests in its own coordinates, from its top-left corner.
    listEv.positionDip = { ev.positionDip.x - list.left, ev.positionDip.y - list.top };

    bool  isHandled = m_list.OnMouse (listEv);



    if (ev.kind == DxuiMouseEventKind::Up)
    {
        ApplyToggle();
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::GetTooltip
//
//  A cell its column cut short, in full.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTableView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    RECT  list  = m_list.GetBounds();
    bool  isCut = m_list.GetClippedCellText (pointPx.x - list.left, pointPx.y - list.top, outText);



    if (isCut)
    {
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return isCut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTableView::OnKey (const DxuiKeyEvent & ev)
{
    bool  isHandled = m_list.OnKey (ev);



    ApplyToggle();

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::GetTopHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int InspectorTableView::GetTopHeightPx() const
{
    return m_caption.empty() ? 0 : m_scaler.ToPx (kCaptionDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::PaintTop
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTableView::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    (void) painter;

    text.DrawString (m_caption.c_str(), static_cast<float> (top.left), static_cast<float> (top.top), static_cast<float> (top.right - top.left),
                     static_cast<float> (top.bottom - top.top), theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::GetSelectedText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTableView::GetSelectedText() const
{
    int                            row  = m_list.GetSelectedRow();
    vector<vector<std::wstring>>   rows;



    if (row >= 0 && row < static_cast<int> (m_rows.size()))
    {
        rows.push_back (m_rows[row].cells);
    }

    return rows.empty() ? std::wstring() : InspectorClipboard::FormatTable (m_titles, rows);
}
