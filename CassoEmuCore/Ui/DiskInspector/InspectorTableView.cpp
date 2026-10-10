#include "Pch.h"

#include "Ui/DiskInspector/InspectorTableView.h"
#include "Devices/Disk/Inspector/InspectorClipboard.h"





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
        if (!m_isSelecting && m_select && row >= 0 && row < static_cast<int> (m_rows.size()))
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
    vector<vector<DxuiListView::Cell>>  cells;



    m_rows = std::move (rows);

    for (const TableRow & row : m_rows)
    {
        vector<DxuiListView::Cell>  line;

        for (const std::wstring & text : row.cells)
        {
            DxuiListView::Cell  cell;

            cell.text = text;
            line.push_back (cell);
        }

        cells.push_back (std::move (line));
    }

    m_list.SetRows (std::move (cells));
    m_list.UpdateAutoFitFromRows();
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTableView::OnMouse (const DxuiMouseEvent & ev)
{
    return m_list.OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableView::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorTableView::OnKey (const DxuiKeyEvent & ev)
{
    return m_list.OnKey (ev);
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
