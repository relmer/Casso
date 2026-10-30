#include "Pch.h"
#include "Core/DxuiClipboard.h"

#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetRect
//
//  SetRows may have run before the host knew the paint rect; it would
//  have clamped m_topRow against a zero-capacity rect and, with
//  sticky-tail on, pinned m_topRow past the end. Re-clamp now that the
//  real capacity is known, preserving sticky-tail intent. A view clamped
//  to the end rests at the very end, its last row whole.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRect (const RECT & rect)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;
    int   maxTop    = 0;



    SetBounds (rect);
    maxTop = m_scrollbars.GetMaxTopRow();

    if (wasSticky || m_topRow > maxTop)
    {
        m_topRow      = maxTop;
        m_flushBottom = m_scrollbars.CanLiftRows();
    }

    if (m_topRow < 0)
    {
        m_topRow = 0;
    }

    m_scrollbars.m_stickyTail = m_scrollbars.m_stickyTailEnabled && (m_topRow >= maxTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetColumns (std::vector<Column> cols)
{
    m_columns = std::move (cols);
    m_columnSizing.m_measuredWPx.clear();
    m_columnSizing.m_overrideWPx.assign (m_columns.size(), -1);

    m_header.MatchColumnCount (m_columns.size());

    // NOTE: deliberately does NOT ResetAutoFit. Consumers re-issue
    // SetColumns on width / visibility changes (and per mouse-move
    // during a column-resize drag); resetting here would collapse the
    // auto columns mid-drag. The column COUNT is stable for these
    // callers, and UpdateAutoFitFromRows re-sizes m_autoMaxChars if it
    // ever changes. Call ResetAutoFit explicitly on a data clear.
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRows (std::vector<std::vector<Cell>> rows)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;



    m_rowsVersion++;

    // A pushed row set leaves provider (virtual) mode.
    m_virtual     = false;
    m_rowProvider = nullptr;
    m_rowSource   = nullptr;

    m_rowData = std::move (rows);

    //  NOT RE-MEASURED HERE. Measuring walks every cell of every row through
    //  the text renderer, and a folder of a few thousand files paid that on
    //  every refill. Columns are sized once for the view and then left alone,
    //  as Explorer's are; a divider double-click re-fits one on demand.

    ClampTopAfterCountChange (wasSticky);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRowIcon
//
//  One row's icon, for an icon that arrives after its row.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRowIcon (int row, std::shared_ptr<const DxuiIconImage> icon)
{
    if (row >= 0 && row < (int) m_rowData.size() && !m_rowData[(size_t) row].empty())
    {
        m_rowData[(size_t) row][0].icon = std::move (icon);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampTopAfterCountChange
//
//  Re-clamp the top row and re-evaluate sticky-tail after the row count
//  changes. Shared by SetRows / AppendRows / SetVirtualRowCount /
//  SetRowProvider so all four keep identical scroll behavior. A view clamped
//  to the end rests at the very end, its last row whole, and a page still
//  sliding stops there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ClampTopAfterCountChange (bool wasSticky)
{
    int  maxTop = m_scrollbars.GetMaxTopRow();



    if (wasSticky || m_topRow > maxTop)
    {
        m_topRow      = maxTop;
        m_flushBottom = m_scrollbars.CanLiftRows();
        m_items.m_pageSlide   = {};
    }

    if (m_topRow < 0)
    {
        m_topRow = 0;
    }

    //  A list that now fits has no end to hold, should it grow again.
    m_flushBottom             = m_flushBottom && m_grouping.GetLineCount() > m_scrollbars.GetVisibleRowCapacity();
    m_scrollbars.m_stickyTail = m_scrollbars.m_stickyTailEnabled && (m_topRow >= maxTop);

    PruneSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRowProvider / SetVirtualRowCount
//
//  Enter (or refresh) the virtual row model: the list holds no materialized
//  rows, tracks only a total count, and pulls cells for the visible window
//  from `provider` during Paint. SetVirtualRowCount updates just the count
//  (e.g. a streaming log growing under a stable provider) and keeps the same
//  sticky-tail semantics as SetRows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRowProvider (int rowCount, RowProvider provider)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;



    m_virtual      = true;
    m_rowProvider  = std::move (provider);
    m_rowSource    = nullptr;
    m_virtualCount = (rowCount > 0) ? rowCount : 0;
    m_rowData.clear();

    ClampTopAfterCountChange (wasSticky);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRowSource
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRowSource (int rowCount, RowSource source)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;



    m_rowsVersion++;

    m_virtual      = true;
    m_rowProvider  = nullptr;
    m_rowSource    = std::move (source);
    m_virtualCount = (rowCount > 0) ? rowCount : 0;
    m_rowData.clear();

    ClampTopAfterCountChange (wasSticky);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetVirtualRowCount
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetVirtualRowCount (int rowCount)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;



    m_rowsVersion++;

    m_virtual      = true;
    m_virtualCount = (rowCount > 0) ? rowCount : 0;

    ClampTopAfterCountChange (wasSticky);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProvideRow / NoteAutoFitRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ProvideRow (int r, std::vector<Cell> & out) const
{
    out.clear();

    if (m_virtual)
    {
        if (m_rowSource && r >= 0 && r < m_virtualCount)
        {
            out = m_rowSource (r);
        }
        else if (m_rowProvider && r >= 0 && r < m_virtualCount)
        {
            m_rowProvider (r, out);
        }

        return;
    }

    if (r >= 0 && (size_t) r < m_rowData.size())
    {
        out = m_rowData[(size_t) r];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendRows
//
//  Streaming-friendly counterpart to SetRows: appends new rows without
//  rebuilding the existing ones, so callers spamming the list pay a cost
//  proportional to the number of new rows rather than the total. The
//  sticky-tail decision is made from the PRE-append state so a viewer
//  parked at the bottom keeps following the tail while one scrolled up
//  stays put.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::AppendRows (std::vector<std::vector<Cell>> rows)
{
    bool  wasSticky = m_scrollbars.m_stickyTail;



    if (rows.empty())
    {
        return;
    }

    // Appending to a materialized set leaves provider (virtual) mode.
    m_virtual     = false;
    m_rowSource   = nullptr;
    m_rowProvider = nullptr;

    m_rowData.insert (m_rowData.end(),
                                  std::make_move_iterator (rows.begin()),
                                  std::make_move_iterator (rows.end()));

    if (m_columnSizing.m_preciseAutoFit)
    {
        m_columnSizing.m_measureDirty = true;   // grow precise widths monotonically (keep prior max)
    }
    else
    {
        m_columnSizing.m_measuredWPx.clear();
    }

    ClampTopAfterCountChange (wasSticky);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelectedRow
//
//  Selects a data row (clamped to range), then scrolls just enough to
//  bring it into view.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetSelectedRow (int r)
{
    int  rows = GetRowCount();



    if (r >= rows)
    {
        r = rows - 1;
    }

    if (r < 0)
    {
        m_selectedRow       = -1;
        m_anchorRow = -1;
        m_selectedRows.clear();
        return;
    }

    EnsureVisible (r);
    m_selectedRow       = r;
    m_anchorRow = r;
    m_selectedRows.assign (1, r);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMultiSelect
//
//  Turning it off collapses the set to the row the keyboard is on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetMultiSelect (bool enabled)
{
    m_multiSelect = enabled;

    if (!enabled)
    {
        SetSelectedRow (m_selectedRow);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelectedRows
//
//  Rows outside the list are dropped and the rest sorted and made unique.
//  The last row in the set becomes the keyboard's row unless the anchor is
//  among them, in which case the anchor is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetSelectedRows (std::vector<int> rows, int anchor)
{
    int  count = GetRowCount();



    std::erase_if (rows, [count] (int r) { return r < 0 || r >= count; });
    std::sort (rows.begin(), rows.end());
    rows.erase (std::unique (rows.begin(), rows.end()), rows.end());

    if (!m_multiSelect && rows.size() > 1)
    {
        rows.assign (1, std::find (rows.begin(), rows.end(), anchor) != rows.end() ? anchor : rows.back());
    }

    m_selectedRows = std::move (rows);

    if (m_selectedRows.empty())
    {
        m_selectedRow       = -1;
        m_anchorRow = -1;
        return;
    }

    m_anchorRow = IsRowSelected (anchor) ? anchor : m_selectedRows.back();
    m_selectedRow       = m_anchorRow;

    EnsureVisible (m_selectedRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRowSelected
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsRowSelected (int row) const
{
    return std::binary_search (m_selectedRows.begin(), m_selectedRows.end(), row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectRangeFromAnchor
//
//  The anchor stays where it was, so a second Shift+click from the same
//  anchor replaces the range rather than adding to it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SelectRangeFromAnchor (int row)
{
    int  anchor = (m_anchorRow >= 0) ? m_anchorRow : row;
    int  first  = (std::min) (anchor, row);
    int  last   = (std::max) (anchor, row);
    int  r      = 0;



    m_selectedRows.clear();

    for (r = first; r <= last; r++)
    {
        m_selectedRows.push_back (r);
    }

    m_anchorRow = anchor;
    m_selectedRow       = row;

    EnsureVisible (row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClickRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ClickRow (int row, bool ctrl, bool shift)
{
    auto  at = std::lower_bound (m_selectedRows.begin(), m_selectedRows.end(), row);



    if (row < 0 || row >= GetRowCount())
    {
        return;
    }

    if (m_multiSelect && shift)
    {
        SelectRangeFromAnchor (row);
    }
    else if (m_multiSelect && ctrl)
    {
        if (at != m_selectedRows.end() && *at == row)
        {
            m_selectedRows.erase (at);
        }
        else
        {
            m_selectedRows.insert (at, row);
        }

        m_anchorRow = row;
        m_selectedRow       = row;
        EnsureVisible (row);
    }
    else
    {
        SetSelectedRow (row);
    }

    if (m_onSelectionChanged)
    {
        m_onSelectionChanged (row);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectAllRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SelectAllRows()
{
    int  count = GetRowCount();
    int  r     = 0;



    if (!m_multiSelect || count <= 0)
    {
        return;
    }

    m_selectedRows.clear();

    for (r = 0; r < count; r++)
    {
        m_selectedRows.push_back (r);
    }

    if (m_selectedRow < 0)
    {
        m_selectedRow = 0;
    }

    if (m_anchorRow < 0)
    {
        m_anchorRow = m_selectedRow;
    }

    if (m_onSelectionChanged)
    {
        m_onSelectionChanged (m_selectedRow);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PruneSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PruneSelection()
{
    int  count = GetRowCount();



    std::erase_if (m_selectedRows, [count] (int r) { return r >= count; });

    //  The keyboard's row is left alone with multiple selection off, which is
    //  how the list has always treated it across a row change.
    if (m_multiSelect && m_selectedRow >= count)
    {
        m_selectedRow = m_selectedRows.empty() ? -1 : m_selectedRows.back();
    }

    if (m_anchorRow >= count)
    {
        m_anchorRow = m_selectedRows.empty() ? -1 : m_selectedRows.back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureVisible
//
//  Scroll the minimum needed to bring `row` into the visible window. Unlike
//  SetSelectedRow it does not touch selection, so a host can keep a focused
//  row on screen (e.g. after a re-sort) independent of what is selected.
//
//  A row already whole in sight leaves the view alone, which keeps a page
//  down's rows flush with the bottom. After one, a row past the bottom comes
//  in flush with it too. A row cut off at the top, while the rows are lifted
//  by a page or at the list's end, comes in whole.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::EnsureVisible (int row)
{
    int  cap   = m_scrollbars.GetVisibleRowCapacity();
    int  line  = 0;
    int  top   = 0;
    int  first = 0;
    int  last  = 0;



    if (IsItemsView())
    {
        m_items.EnsureItemVisible (row);
        return;
    }

    if (row < 0 || row >= GetRowCount() || cap <= 0)
    {
        return;
    }

    //  A group's first row brings its header into view with it; a row under a
    //  collapsed header has no line to scroll to.
    line = m_grouping.GetLineOfRow (row);

    if (line < 0)
    {
        return;
    }

    top  = (HasGroupLines() && line > 0 && m_grouping.GetRowOfLine (line - 1) < 0) ? line - 1 : line;

    if (m_keyboard.TryGetWholeLineSpan (first, last) && top >= first && line <= last)
    {
        return;
    }

    if (top < m_topRow || (top == m_topRow && m_scrollbars.GetRowShiftPx() > 0))
    {
        m_scrollbars.SetTopRow (top);
    }
    else if (m_flushBottom && line > last)
    {
        m_keyboard.ScrollLineToBottom (line);
    }
    else if (line >= m_topRow + cap)
    {
        m_scrollbars.SetTopRow (line - cap + 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CenterOnRow
//
//  Scrolls so `row` lands mid-window (SetTopRow clamps at either end of
//  the list). Selection is untouched.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::CenterOnRow (int row)
{
    int  cap = m_scrollbars.GetVisibleRowCapacity();



    if (row < 0 || row >= GetRowCount() || cap <= 0)
    {
        return;
    }

    m_scrollbars.SetTopRow (m_grouping.GetLineOfRow (row) - cap / 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCursorForPoint  (IDxuiControl override)
//
//  Advertises the horizontal resize cursor when the point (in list-local
//  px, as the hosting panel translates for OnMouse) is over a column
//  divider; otherwise no preference. Uses the same grab tolerance as the
//  resize-drag hit test so the cursor and the drag agree.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiListView::GetCursorForPoint (POINT localPx) const
{
    LPCWSTR  cursor  = nullptr;
    int      grabTol = m_scaler.ToPx (s_kResizeGrabDip);



    if (m_header.HitTestColumnResize (localPx.x, localPx.y, grabTol) >= 0)
    {
        cursor = IDC_SIZEWE;
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestRow
//
//  xPx/yPx are relative to the list's rect.left/top. Returns the
//  data-row index (into m_rowData), accounting for the current scroll
//  offset, or -1 if the point is outside any visible data row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestRow (int xPx, int yPx) const
{
    int  line = 0;



    if (IsItemsView())
    {
        return m_items.HitTestItem (xPx, yPx);
    }

    line = m_grouping.HitTestLine (xPx, yPx);

    //  Past the last column is empty space, as in Explorer's Details view: a
    //  click there selects nothing and gives the folder's own menu.
    if (m_showHeader && !m_columns.empty() && xPx + m_leftPx >= m_columnSizing.GetContentWidthPx())
    {
        return -1;
    }

    return (line < 0) ? -1 : m_grouping.GetRowOfLine (line);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePalette
//
//  Derives the per-element ARGB colors from the active theme. Called
//  once per paint; assumes the theme has already been null-checked.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Palette DxuiListView::MakePalette() const
{
    DxuiListViewTypes::Palette  pal = {};



    pal.fg       = m_theme->Foreground();
    pal.fgDim    = (pal.fg & 0x00FFFFFFu) | 0xA0000000u;
    pal.hdrFg    = m_theme->HeadingForeground();
    pal.bgRow    = m_theme->ContentBackground();
    pal.bgHover  = m_theme->ContentHover();
    pal.bgSel    = m_textSelectionColors ? m_theme->SelectionBackground() : m_theme->ContentSelection();
    pal.edgeSel  = m_textSelectionColors ? 0u : m_theme->ContentSelectionEdge();
    pal.bgHeader = (pal.bgRow & 0x00FFFFFFu) | 0xFF000000u;
    pal.border   = m_theme->ContentHeaderDivider();
    pal.matchBg  = (m_theme->Accent() & 0x00FFFFFFu) | 0x80000000u;

    return pal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Paints the background, optional bold header row (with sort glyph,
//  separators and keyboard-focus markers), the visible data rows, and
//  the scrollbar when the row count exceeds capacity.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT          hr         = S_OK;
    ScrollLayout     layout     = m_scrollbars.ComputeScrollLayout();
    float            x          = (float) m_boundsDip.left;
    float            y          = (float) m_boundsDip.top;
    float            fullW      = (float) (m_boundsDip.right - m_boundsDip.left);
    float            fullH      = (float) (m_boundsDip.bottom - m_boundsDip.top);
    int              visibleCap = layout.rowCap;
    int              totalRows  = m_grouping.GetLineCount();
    int              firstRow   = m_topRow;
    int              lastRow    = std::min (totalRows, m_topRow + (visibleCap > 0 ? m_scrollbars.GetShownRowCount() : totalRows));
    int              rowH       = GetRowHeightPx();
    int              slidePx    = IsItemsView() ? 0 : m_items.GetPageSlidePx();
    float            barW       = layout.vBar ? (float) GetScrollbarWidthPx() : 0.0f;
    float            hBarH      = layout.hBar ? (float) GetScrollbarWidthPx() : 0.0f;
    float            layoutW    = fullW - barW;
    float            contentH   = fullH - hBarH;
    float            bodyTop    = m_showHeader ? (float) (GetHeaderBarPx() + GetHeaderGapPx()) : 0.0f;
    //  Always: a header or a row wider than the list used to paint over
    //  whatever sat beside it, since nothing else clips a Dxui widget.
    bool             clip       = true;
    Palette          pal        = {};
    bool             gdiText    = false;
    std::vector<int> colXPx;
    std::vector<int> colWPx;



    BAIL_OUT_IF (m_theme == nullptr || m_columns.empty(), S_OK);

    //  Names drawn by GDI's advances, as Explorer's list draws them; its
    //  columns are sized and its names cut by the same measure.
    gdiText = text.SetGdiClassicText (true);

    pal = MakePalette();

    if (m_columnSizing.m_measuredWPx.size() != m_columns.size() || (m_columnSizing.m_preciseAutoFit && m_columnSizing.m_measureDirty))
    {
        m_columnSizing.MeasureColumnsPx (text);
        m_columnSizing.m_measureDirty = false;
    }

    m_columnSizing.ComputeColumnLayout (layoutW, colXPx, colWPx);

    //  A sliding page draws the rows it is leaving as well as those it rests
    //  on: above them while it slides down, below them while it slides up.
    if (slidePx > 0 && rowH > 0)
    {
        firstRow = (std::max) (0, m_topRow - (slidePx + rowH - 1) / rowH);
    }
    else if (slidePx < 0 && rowH > 0)
    {
        lastRow = (std::min) (totalRows, lastRow + (rowH - slidePx - 1) / rowH);
    }

    painter.FillRect (x, y, fullW, fullH, pal.bgRow);

    //  The painter's clip as well as the text's: a row cut off at the top,
    //  as a page down leaves one, would otherwise draw its highlight above
    //  the list.
    if (clip)
    {
        hr = text.PushClipRect (x, y, layoutW, contentH);
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PushClipRect (x, y, layoutW, contentH);
    }

    //  The rows first and the header over them, so a row drawn partly above
    //  the body, at the end of the list, goes under the header. While the
    //  content is hidden, no rows are drawn.
    if (!m_contentHidden && IsItemsView())
    {
        m_itemPainter.PaintItems (painter, text, pal, x, y);
    }
    else if (!m_contentHidden)
    {
        //  Below the header the rows are clipped as well: its fill covers
        //  theirs, but text is drawn after every fill, so a row a sliding page
        //  carries up out of the body would write over the column titles.
        if (bodyTop > 0.0f)
        {
            hr = text.PushClipRect (x, y + bodyTop, layoutW, contentH - bodyTop);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClipRect (x, y + bodyTop, layoutW, contentH - bodyTop);
        }

        m_rows.Paint (painter, text, pal, x, y, layoutW, firstRow, lastRow, colXPx, colWPx);
        m_band.Paint (painter, pal, x, y);

        if (bodyTop > 0.0f)
        {
            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PopClipRect();
        }
    }

    if (m_showHeader)
    {
        painter.FillRect (x, y, layoutW, (float) (GetHeaderBarPx() + GetHeaderGapPx()), pal.bgRow);
        m_header.PaintHeader (painter, text, pal, x, y, layoutW, colXPx, colWPx);
        m_header.PaintHeaderFocusMarkers (painter, pal, x, y, colXPx, colWPx);
    }

    if (clip)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }

    if (!m_contentHidden)
    {
        m_scrollbars.PaintScrollbar  (painter, pal, x, y);
        m_scrollbars.PaintHScrollbar (painter, pal, x, y);
    }

    if (layout.vBar && layout.hBar)
    {
        uint32_t  cornerArgb = (pal.fg & 0x00FFFFFFu) | 0x18000000u;

        painter.FillRect (x + layoutW, y + contentH, barW, hBarH, cornerArgb);
    }

    text.SetGdiClassicText (gdiText);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetClockMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t DxuiListView::GetClockMs() const
{
    return m_keyboard.m_clock ? m_keyboard.m_clock() : (int64_t) GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowCells
//
//  The cell vector for row `r`. Virtual mode pulls the row on demand into
//  the reused provider scratch and grows the auto-fit counts from it; push
//  mode references m_rowData directly, with no per-row copy. Factored from
//  PaintDataRows so its loop can bind the row's cells at the top of the
//  body instead of after the mode branch.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DxuiListViewTypes::Cell> & DxuiListView::GetRowCells (int r) const
{
    const std::vector<Cell> *  cells = nullptr;



    if (m_virtual && m_rowSource)
    {
        if (r < 0 || r >= m_virtualCount)
        {
            m_providerScratch.clear();
            return m_providerScratch;
        }

        cells = &m_rowSource (r);
        m_columnSizing.NoteAutoFitRow (*cells);
    }
    else if (m_virtual)
    {
        ProvideRow (r, m_providerScratch);
        m_columnSizing.NoteAutoFitRow (m_providerScratch);
        cells = &m_providerScratch;
    }
    else
    {
        cells = &m_rowData[(size_t) r];
    }

    return *cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::QueryCommand  (IDxuiControl override)
//
//  The list handles Select all when multiple selection is enabled. The meaning
//  of Copy for a set of rows (file names, a path, cell text) depends on the
//  host, so the list does not handle it and the router passes it to the
//  containing control.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    bool  handled = false;



    if (command == DxuiStandardCommand::SelectAll && m_multiSelect)
    {
        outEnabled = GetRowCount() > 0;
        handled    = true;
    }
    else if (command == DxuiStandardCommand::Copy && m_ownerHwnd != nullptr)
    {
        outEnabled = !m_selectedRows.empty();
        handled    = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::InvokeCommand  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        SelectAllRows();
    }
    else if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        DxuiClipboard::SetText (m_ownerHwnd, GetSelectionText());
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::Layout  (IDxuiControl override)
//
//  Delegates to the existing SetRect / SetDpi pair so column widths,
//  scroll positions, and sticky-tail state are preserved.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    //  The DPI first, since the top row is clamped by the row height; and the
    //  horizontal scroll again, since a wider window or narrower columns mean
    //  less to scroll.
    SetBounds (boundsDip);
    SetDpi    (scaler.GetDpi());
    SetRect   (boundsDip);
    m_scrollbars.SetLeftPx (m_leftPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SetDpi
//
//  A width the user gave a column is held in pixels, so a new DPI scales it
//  with everything else: a column set before the window knew its monitor, or
//  one moved to a monitor of another DPI, stays the same size to the eye.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetDpi (UINT dpi)
{
    UINT  old = m_scaler.GetDpi();



    m_scaler.SetDpi (dpi);

    if (old == 0 || old == dpi)
    {
        return;
    }

    for (int & width : m_columnSizing.m_overrideWPx)
    {
        if (width > 0)
        {
            width = MulDiv (width, (int) dpi, (int) old);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    // Adopt the theme handed down by the paint pump every frame so a
    // theme change / preview takes effect on the next repaint without a
    // separate push. m_theme is read only within the synchronous const
    // Paint below (the per-paint palette), so this is safe.
    m_theme = &theme;

    //  Before the pass below, so the frame draws the width a fit asked for.
    //  Here rather than inside that pass because a width is real state, and
    //  measuring needs the renderer only the paint holds.
    m_columnSizing.ApplyPendingFit (text);

    //  Small icons and List size their columns to the names, so the names
    //  are measured before the pass lays anything out.
    if (m_view == View::SmallIcons || m_view == View::List)
    {
        m_items.MeasureItemNames (text);
    }

    static_cast<const DxuiListView *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::OnMouse  (IDxuiControl override)
//
//  Self-contained mouse handling. ev.positionDip is widget-relative (the
//  host subtracts the list's own origin first, matching the 0-based
//  hit-test accessors), so the coordinates pass straight through.
//  Dispatches by event kind and returns true when the list consumed it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled  = false;
    int   lx       = ev.positionDip.x;
    int   ly       = ev.positionDip.y;
    int   widthPx  = m_boundsDip.right  - m_boundsDip.left;
    int   heightPx = m_boundsDip.bottom - m_boundsDip.top;
    bool  inside   = (lx >= 0 && lx < widthPx && ly >= 0 && ly < heightPx);



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Down:  handled = OnPress (ev, lx, ly, inside);  break;
        case DxuiMouseEventKind::Move:  handled = (ev.button != DxuiMouseButton::Left && IsInteracting())
                                                  ? OnRelease (lx, ly, inside)
                                                  : OnDrag    (lx, ly, inside);     break;
        case DxuiMouseEventKind::Up:    handled = OnRelease (lx, ly, inside);      break;
        case DxuiMouseEventKind::Wheel: handled = m_scrollbars.OnWheel (ev, inside);          break;
        case DxuiMouseEventKind::Leave: SetHoveredRow (-1); m_grouping.ClearHover(); m_header.OnHover (-1, -1, false); break;
        default:                                                                            break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPress
//
//  Left-press priority chain: the scrollbars, then the header's dividers and
//  columns, then a group's header, then a data row, then empty space, which
//  starts a rubber band, as in Explorer: between items, below the last row,
//  or past the last column. Without multiple selection, a plain click on the
//  empty space below the rows selects nothing. A left-press anywhere inside
//  the list is consumed so the host can claim focus. (Right-click stays with
//  the host: its menu content is host-specific.)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::OnPress (const DxuiMouseEvent & ev, int lx, int ly, bool inside)
{
    int  row = -1;



    if (ev.button != DxuiMouseButton::Left || !inside)
    {
        return false;
    }

    if (m_scrollbars.OnPress (lx, ly) || m_header.OnPress (lx, ly))
    {
        return true;
    }

    row = HitTestRow (lx, ly);

    if (m_rows.OnCheckPress (lx, row))
    {
        return true;
    }

    if (m_grouping.OnPress (lx, ly) || m_rows.OnPress (ev, row))
    {
        // Taken.
    }
    else if (m_multiSelect && (IsItemsView() || ly >= GetHeaderHeightPx() + (m_showHeader ? GetHeaderGapPx() : 0)))
    {
        m_band.Begin (lx, ly, ev.ctrl);
    }
    else if (!ev.ctrl && !ev.shift && !m_selectedRows.empty())
    {
        ClearSelection();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDrag
//
//  Drives an in-progress resize, header, thumb, band or selection drag;
//  otherwise tracks the hovered row and group header while the pointer is
//  inside. Returns true while dragging or hovering inside the list.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::OnDrag (int lx, int ly, bool inside)
{
    if (m_header.OnDrag (lx) || m_scrollbars.OnDrag (lx, ly) || m_band.OnDrag (lx, ly) || m_rows.OnDrag (ly))
    {
        return true;
    }

    m_header.OnHover (lx, ly, inside);

    if (!inside)
    {
        SetHoveredRow (-1);
        m_grouping.ClearHover();
        return false;
    }

    SetHoveredRow (HitTestRow (lx, ly));
    m_grouping.OnHover (lx, ly);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRelease
//
//  Ends the band, a range a row press left, a held scrollbar, or a header
//  press or resize; otherwise a release over a data row selects it and
//  raises onActivateRow. Returns true when one of those ended or a row was
//  released on.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::OnRelease (int lx, int ly, bool inside)
{
    int   row    = inside ? HitTestRow (lx, ly) : -1;
    bool  ranged = m_rows.EndPress();



    return m_band.OnRelease() || ranged || m_scrollbars.OnRelease() || m_header.OnRelease (lx, ly) || m_rows.OnRelease (row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::OnFocusChanged
//
//  Mirrors the list-focused flag the widget renders with. Losing focus
//  also drops any keyboard column sub-focus so the header / divider
//  markers don't linger after Tab moves on to another control.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::OnFocusChanged (bool focused)
{
    int  row = -1;



    SetListFocused (focused);

    if (!focused)
    {
        m_header.ReleaseKeyboardColumnFocus();
    }
    else if (m_kbColNavEnabled && !m_kbColResize)
    {
        // In the body/header model, gaining focus lands directly on the body
        // sub-stop (selecting a row if none) so a single Tab from the
        // neighboring control shows focus immediately. The resize model keeps
        // its neutral entry (the first Tab / Shift+Tab picks a direction into
        // the sub-stops).
        //
        // The row selected is the top VISIBLE one, not row 0: a click on a
        // scrolled list gives it focus before the press is hit-tested, so
        // scrolling back to row 0 here would land the click on the wrong row.
        m_header.m_kbColFocus = 0;
        m_header.ClearColumnFocusMarkers();

        //  THE ROW IN VIEW, NOT ROW ZERO. Seeding the first row scrolls a list
        //  the user had scrolled elsewhere, and a host that gives the list
        //  focus on a press then hit-tests that press against a list which has
        //  just jumped home: the click lands on whatever fell under the
        //  pointer after the jump.
        //  With groups, the top is a line, and a header line has no row: its
        //  group's first row is the one below it.
        if (m_seedRowOnFocus && GetSelectedRow() < 0 && GetRowCount() > 0)
        {
            row = IsItemsView() ? m_topRow : m_grouping.GetRowOfLine (m_topRow);
            row = (row >= 0) ? row : m_grouping.GetRowOfLine (m_topRow + 1);
            SetSelectedRow ((std::max) (0, (std::min) (row, GetRowCount() - 1)));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::OnKey
//
//  Opt-in keyboard navigation (SetKeyboardColumnNav). Off: the list is inert
//  (existing consumers unaffected). On: dispatches to one of two models --
//  the default body/header model or the column/divider resize model
//  (SetKeyboardColumnResize).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::OnKey (const DxuiKeyEvent & ev)
{
    bool  dispatches = m_kbColNavEnabled && (ev.kind == DxuiKeyEventKind::Down);
    bool  handled    = false;



    //  In a list with check boxes, Space checks or clears the selected row's,
    //  unless it is typed in a type-ahead search.
    if (m_onCheckToggled && ev.vk == VK_SPACE && GetSelectedRow() >= 0 && !m_keyboard.IsTypingAhead())
    {
        if (ev.kind == DxuiKeyEventKind::Down)
        {
            m_onCheckToggled (GetSelectedRow());
        }

        return true;
    }

    //  A printable character jumps to a row, as it does in Explorer. With Ctrl
    //  or Alt down it is a shortcut instead, and is left to the host.
    if (m_kbColNavEnabled && ev.kind == DxuiKeyEventKind::Char && !ev.ctrl && !ev.alt
        && ev.vk >= 0x20 && ev.vk != 0x7F)
    {
        return m_keyboard.HandleTypeAhead ((wchar_t) ev.vk);
    }

    if (dispatches)
    {
        handled = m_kbColResize ? m_header.OnKeyColumnResizeNav (ev) : m_header.OnKeyBodyHeaderNav (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetSelectionText
//
//  The selected rows in list order, whatever order they were selected in.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiListView::GetSelectionText() const
{
    std::vector<int>  rows = m_selectedRows;
    std::wstring      text;



    std::sort (rows.begin(), rows.end());

    for (int row : rows)
    {
        if (row >= 0 && row < GetRowCount())
        {
            const std::vector<Cell> &  cells = GetRowCells (row);

            for (size_t col = 0; col < cells.size(); col++)
            {
                text += (col > 0 ? L"\t" : L"") + cells[col].text;
            }

            text += L"\r\n";
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::GetScrollbarMenu
//
//  The bars' positions are a top line and a left pixel, as the list's are.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiListView::GetScrollbarMenu (POINT pt)
{
    POINT  barPt = {};



    m_scrollbars.SyncVertScroll();
    m_scrollbars.SyncHorzScroll();

    m_scrollbars.m_vertScroll.SetOnScroll ([this] (int, int pos) { m_scrollbars.SetTopRow (pos); });
    m_scrollbars.m_horzScroll.SetOnScroll ([this] (int, int pos) { m_scrollbars.SetLeftPx (pos); });

    if (m_scrollbars.m_vertScroll.GetMetrics().visible && IsOverBar (m_scrollbars.m_vertScroll, pt))
    {
        barPt = GetBarPoint (m_scrollbars.m_vertScroll, pt);
        return m_scrollbars.m_vertScroll.BuildContextMenu (barPt.x, barPt.y);
    }

    if (m_scrollbars.IsHorzScrollbarVisible() && IsOverBar (m_scrollbars.m_horzScroll, pt))
    {
        barPt = GetBarPoint (m_scrollbars.m_horzScroll, pt);
        return m_scrollbars.m_horzScroll.BuildContextMenu (barPt.x, barPt.y);
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::SetView
//
//  The header belongs to Details alone, so it is put away while another view
//  shows and comes back with Details. The view opens at its start, with the
//  selected entry scrolled into sight.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetView (View view)
{
    if (view == m_view)
    {
        return;
    }

    if (m_view == View::Details)
    {
        m_detailsHeader = m_showHeader;
        m_showHeader    = false;
    }
    else if (view == View::Details)
    {
        m_showHeader = m_detailsHeader;
    }

    m_view           = view;
    m_topRow         = 0;
    m_leftPx         = 0;
    m_items.m_pageSlide      = {};
    m_flushBottom    = false;
    m_items.m_itemAnchorLine = -1;

    EnsureVisible (m_selectedRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumnFilter
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetColumnFilter (size_t column, bool filterable, bool filtered)
{
    if (column < m_columns.size())
    {
        m_columns[column].filterable = filterable;
        m_columns[column].filtered   = filtered;
    }
}