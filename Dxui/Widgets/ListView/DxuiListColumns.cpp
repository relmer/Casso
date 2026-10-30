#include "Pch.h"

#include "Widgets/ListView/DxuiListColumns.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListColumns::DxuiListColumns
//
////////////////////////////////////////////////////////////////////////////////

DxuiListColumns::DxuiListColumns (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteAutoFitRow
//
//  Tracks the widest cell seen per auto-sized column, so column widths can be
//  fitted to real content.
//
//  Width is measured in CHARACTERS, not pixels, because this is called from
//  the row-append path where no text renderer is available. It is an
//  approximation, and a proportional font makes it a loose one, but it is
//  computed for free on data the caller already has -- measuring every cell
//  would cost a renderer call per cell per frame in a list that streams.
//
//  Columns with an explicit width or a stretch flag are skipped: their width
//  is already decided, and tracking content for them would waste the work.
//
//  The header title participates in the maximum when the header is shown, so a
//  column whose title is longer than any of its values is not truncated in its
//  own heading.
//
//  Values only ever GROW -- this accumulates a high-water mark across the
//  rows it is shown, and ResetAutoFit is what clears it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::NoteAutoFitRow (const std::vector<DxuiListViewTypes::Cell> & cells) const
{
    if (m_autoMaxChars.size() != m_list.m_columns.size())
    {
        m_autoMaxChars.assign (m_list.m_columns.size(), 0);
    }

    for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); ++c)
    {
        int  chars = 0;

        if (m_list.m_columns[c].widthDip != 0 || m_list.m_columns[c].stretch)
        {
            continue;
        }

        chars = (int) cells[c].text.size();

        if (m_list.m_showHeader)
        {
            chars = std::max (chars, (int) m_list.m_columns[c].title.size());
        }

        if (chars > m_autoMaxChars[c])
        {
            m_autoMaxChars[c] = chars;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumnVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::SetColumnVisible (size_t idx, bool visible)
{
    HRESULT  hr          = S_OK;
    size_t   columnCount = 0;



    columnCount = m_list.m_columns.size();
    CBRAEx (idx < columnCount, E_INVALIDARG);

    m_list.m_columns[idx].visible = visible;
    m_list.m_scrollbars.SetLeftPx (m_list.m_leftPx);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumnOverrideWidthPx
//
//  User-resized column width (pixels). -1 means clear the override and
//  fall back to widthDip / measured / stretch. The widget enforces a
//  minimum of 1px.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::SetColumnOverrideWidthPx (size_t idx, int px)
{
    HRESULT  hr          = S_OK;
    size_t   columnCount = 0;



    columnCount = m_list.m_columns.size();
    CBRAEx (idx < columnCount, E_INVALIDARG);

    if (m_overrideWPx.size() < m_list.m_columns.size())
    {
        m_overrideWPx.assign (m_list.m_columns.size(), -1);
    }

    m_overrideWPx[idx] = (px < 1 && px != -1) ? 1 : px;
    m_list.m_scrollbars.SetLeftPx (m_list.m_leftPx);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnOverrideWidthPx
//
//  Returns the user-resized override width (pixels) for a column, or
//  -1 when the column has no override or idx is out of range. The -1
//  sentinel tells ComputeColumnLayout to fall back to widthDip /
//  measured / stretch.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetColumnOverrideWidthPx (size_t idx) const
{
    HRESULT  hr            = S_OK;
    int      result        = -1;
    size_t   overrideCount = 0;



    overrideCount = m_overrideWPx.size();
    CBRAEx (idx < overrideCount, E_INVALIDARG);

    result = m_overrideWPx[idx];

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnEffectiveWidthPx
//
//  Current effective width of a column (override > widthDip > measured;
//  a stretch column returns whatever ComputeColumnLayout assigned this
//  frame).
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetColumnEffectiveWidthPx (size_t idx) const
{
    HRESULT           hr            = S_OK;
    int               width         = -1;
    std::vector<int>  xs;
    std::vector<int>  ws;
    int               cap           = 0;
    bool              needBar       = false;
    int               fullW         = 0;
    size_t            overrideCount = 0;



    overrideCount = m_overrideWPx.size();
    CBRAEx (idx < overrideCount, E_INVALIDARG);

    cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);

    ComputeColumnLayout ((float) fullW, xs, ws);
    width = ws[idx];

Error:
    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTotalMeasuredWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetTotalMeasuredWidthPx() const
{
    int  sum = 0;



    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        if (m_list.m_columns[c].widthDip > 0)
        {
            sum += m_list.m_scaler.ToPx (m_list.m_columns[c].widthDip);
        }
        else if (c < m_measuredWPx.size())
        {
            sum += m_measuredWPx[c];
        }
    }

    return sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureColumnsPx
//
//  Measure each column's natural width from its header + cell text.
//  The caller invokes this once after SetColumns/SetRows to populate
//  auto-fit widths, then reads GetTotalMeasuredWidthPx() to size the host
//  dialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::MeasureColumnsPx (IDxuiTextRenderer & text) const
{
    HRESULT  hr      = S_OK;
    float    fontDip = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    float    hdrDip  = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    int      padPx   = m_list.m_scaler.ToPx (s_kCellPadLeftDip) + m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    float    w       = 0.0f;
    float    h       = 0.0f;



    if (m_measuredWPx.size() != m_list.m_columns.size())
    {
        m_measuredWPx.assign (m_list.m_columns.size(), 0);
    }

    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        int  wpx = 0;

        if (m_list.m_showHeader && !m_list.m_columns[c].title.empty())
        {
            //  The title paints at regular weight, and the sort chevron sits
            //  above it, so the title's own width is all a header needs.
            hr = text.MeasureString (m_list.m_columns[c].title.c_str(), hdrDip, DxuiTheme::kBodyFace, w, h);
            IGNORE_RETURN_VALUE (hr, S_OK);

            wpx = std::max (wpx, (int) std::ceil (w));
        }

        for (const auto & row : m_list.m_rowData)
        {
            if (c < row.size() && !row[c].text.empty())
            {
                hr = text.MeasureString (row[c].text.c_str(), fontDip, m_list.GetBodyFace(), w, h);
                IGNORE_RETURN_VALUE (hr, S_OK);

                wpx = std::max (wpx, (int) std::ceil (w) + (row[c].icon ? m_list.m_scaler.ToPx (DxuiListView::s_kCellIconDip + s_kCellIconGapDip) : 0)
                                     + ((row[c].check != DxuiListViewTypes::Cell::Check::None) ? m_list.GetCheckBoxSpanPx() : 0));
            }
        }

        m_measuredWPx[c] = std::max (m_measuredWPx[c], wpx + padPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetAutoFit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::ResetAutoFit()
{
    m_autoMaxChars.clear();
    m_measuredWPx.clear();
    m_measureDirty = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateAutoFitFromRows
//
//  Grows each auto column's tracked glyph count to fit its header and
//  widest current cell. Pure length scan (O(rows) with O(1) per cell),
//  no DWrite; ComputeColumnLayout converts the count to pixels at the
//  current DPI.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::UpdateAutoFitFromRows()
{
    if (m_autoMaxChars.size() != m_list.m_columns.size())
    {
        m_autoMaxChars.assign (m_list.m_columns.size(), 0);
    }

    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        int  maxChars = m_autoMaxChars[c];

        if (m_list.m_columns[c].widthDip != 0 || m_list.m_columns[c].stretch)
        {
            continue;
        }

        if (m_list.m_showHeader)
        {
            maxChars = std::max (maxChars, (int) m_list.m_columns[c].title.size());
        }

        for (const auto & row : m_list.m_rowData)
        {
            if (c < row.size())
            {
                maxChars = std::max (maxChars, (int) row[c].text.size());
            }
        }

        m_autoMaxChars[c] = maxChars;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVisibleColumnCount
//
//  Count of currently-visible columns (per m_columns[c].visible).
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetVisibleColumnCount() const
{
    return (int) std::count_if (m_list.m_columns.begin(), m_list.m_columns.end(),
                                [] (const DxuiListViewTypes::Column & c)
                                {
                                    return c.visible;
                                });
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNthVisibleColumnIndex
//
//  Absolute m_columns index of the n-th visible column, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetNthVisibleColumnIndex (int n) const
{
    int  result = -1;
    int  seen   = 0;



    for (size_t i = 0; i < m_list.m_columns.size(); ++i)
    {
        size_t  c = (i < m_list.GetColumnOrder().size()) ? m_list.GetColumnOrder()[i] : i;

        if (!m_list.m_columns[c].visible)
        {
            continue;
        }

        if (seen == n)
        {
            result = (int) c;
            break;
        }

        ++seen;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetVisibleIndexOfColumn
//
//  Visible-column ordinal of the given absolute column, or -1 if the
//  column is hidden / out of range.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetVisibleIndexOfColumn (size_t absCol) const
{
    HRESULT  hr          = S_OK;
    int      result      = -1;
    int      seen        = 0;
    size_t   columnCount = 0;



    columnCount = m_list.m_columns.size();
    CBRAEx      (absCol < columnCount, E_INVALIDARG);
    BAIL_OUT_IF (!m_list.m_columns[absCol].visible, S_OK);

    //  In display order, as GetNthVisibleColumnIndex counts.
    for (size_t i = 0; i < columnCount; ++i)
    {
        size_t  c = (i < m_list.GetColumnOrder().size()) ? m_list.GetColumnOrder()[i] : i;

        if (c == absCol)
        {
            result = seen;
            break;
        }

        if (m_list.m_columns[c].visible)
        {
            ++seen;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnNaturalWidthPx
//
//  Natural pixel width of a single column ignoring any stretch fill:
//  the user override if set, else the fixed widthDip, else the wider of
//  the batch-measured width and the incremental char-count auto-fit.
//  Returns 0 for a hidden column. Shared by ComputeColumnLayout and
//  GetContentWidthPx so both agree on a column's intrinsic size.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetColumnNaturalWidthPx (size_t c) const
{
    int  wpx       = 0;
    int  padPx     = m_list.m_scaler.ToPx (s_kCellPadLeftDip) + m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    int  perCharPx = (int) std::ceil (m_list.m_scaler.ToPxf (m_list.m_fontDip) * s_kAutoCharWidthEm);



    if (m_list.m_columns[c].visible)
    {
        if (c < m_overrideWPx.size() && m_overrideWPx[c] >= 0)
        {
            wpx = m_overrideWPx[c];
        }
        else if (m_list.m_columns[c].widthDip > 0)
        {
            wpx = m_list.m_scaler.ToPx (m_list.m_columns[c].widthDip);
        }
        else
        {
            int  measuredPx = (c < m_measuredWPx.size()) ? m_measuredWPx[c] : 0;
            int  autoFitPx  = (c < m_autoMaxChars.size() && m_autoMaxChars[c] > 0)
                              ? (m_autoMaxChars[c] * perCharPx + padPx)
                              : 0;

            // Prefer the WIDER of the one-time DWrite measurement (accurate:
            // includes bold-header + sort-glyph reserve, captured once) and
            // the per-frame glyph-count auto-fit (grows as longer cells
            // stream in). The measurement never re-runs, so a header-dominated
            // column keeps its arrow reserve via measuredPx, while a content-
            // dominated column that gained wider rows later grows via
            // autoFitPx instead of clipping.
            wpx = m_preciseAutoFit ? measuredPx : std::max (measuredPx, autoFitPx);
        }
    }

    return wpx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetColumnContentWidthPx
//
//  What the column needs to show its title and every row's value in full,
//  measured now. It takes no account of the column's declared width, its
//  dragged width or the measurement cache, any of which fitting replaces.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetColumnContentWidthPx (IDxuiTextRenderer & text, size_t c, int firstRow, int endRow) const
{
    HRESULT            hr     = S_OK;
    float              fontPx = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    int                padPx  = m_list.m_scaler.ToPx (s_kCellPadLeftDip) + m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    int                iconPx = m_list.m_scaler.ToPx (DxuiListView::s_kCellIconDip + s_kCellIconGapDip);
    int                wpx    = 0;
    float              w      = 0.0f;
    float              h      = 0.0f;



    if (c >= m_list.m_columns.size() || !m_list.m_columns[c].visible)
    {
        return 0;
    }

    if (m_list.m_showHeader && !m_list.m_columns[c].title.empty())
    {
        hr = text.MeasureString (m_list.m_columns[c].title.c_str(), fontPx, DxuiTheme::kBodyFace, w, h);
        IGNORE_RETURN_VALUE (hr, S_OK);

        wpx = (int) std::ceil (w);
    }

    for (int r = (std::max) (firstRow, 0); r < (std::min) (endRow, m_list.GetRowCount()); r++)
    {
        const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (r);

        if (c < cells.size() && !cells[c].text.empty())
        {
            hr = text.MeasureString (cells[c].text.c_str(), fontPx, m_list.GetBodyFace(), w, h);
            IGNORE_RETURN_VALUE (hr, S_OK);

            wpx = (std::max) (wpx, (int) std::ceil (w) + ((cells[c].icon || (int) c == m_list.m_iconColumn) ? iconPx : 0)
                                + ((cells[c].check != DxuiListViewTypes::Cell::Check::None) ? m_list.GetCheckBoxSpanPx() : 0));
        }
    }

    return wpx + padPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitColumnToContent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::FitColumnToContent (size_t idx)
{
    if (idx >= m_list.m_columns.size() || !m_list.m_columns[idx].visible)
    {
        return;
    }

    //  The content has to be measured against the current rows, and only the
    //  paint pass holds a text renderer, so the column waits there for
    //  ApplyPendingFit.
    if (std::find (m_pendingFits.begin(), m_pendingFits.end(), idx) == m_pendingFits.end())
    {
        m_pendingFits.push_back (idx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitColumnToVisibleContent
//
//  A fit that costs a screenful of rows, however long the list.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::FitColumnToVisibleContent (size_t idx)
{
    if (idx >= m_list.m_columns.size() || !m_list.m_columns[idx].visible)
    {
        return;
    }

    if (std::find (m_pendingVisibleFits.begin(), m_pendingVisibleFits.end(), idx) == m_pendingVisibleFits.end())
    {
        m_pendingVisibleFits.push_back (idx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitAllColumnsToContent
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::FitAllColumnsToContent()
{
    for (size_t c = 0; c < m_list.m_columns.size(); c++)
    {
        FitColumnToContent (c);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPendingFit
//
//  Takes the width a fit asked for, once the paint pass has measured it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::ApplyPendingFit (IDxuiTextRenderer & text)
{
    int                                    minPx     = m_list.m_scaler.ToPx (DxuiListView::s_kMinColWidthDip);
    int                                    contentPx = 0;
    int                                    topRow    = m_list.GetTopRow();
    std::vector<std::pair<size_t, bool>>   pending;



    for (size_t idx : m_pendingFits)
    {
        pending.emplace_back (idx, false);
    }

    for (size_t idx : m_pendingVisibleFits)
    {
        pending.emplace_back (idx, true);
    }

    m_pendingFits.clear();
    m_pendingVisibleFits.clear();

    for (const auto & [idx, visibleRows] : pending)
    {
        contentPx = visibleRows ? GetColumnContentWidthPx (text, idx, topRow, topRow + m_list.GetVisibleRowCapacity() + 1)
                                : GetColumnContentWidthPx (text, idx);

        if (contentPx <= 0)
        {
            continue;
        }

        SetColumnOverrideWidthPx (idx, (std::max) (minPx, contentPx));

        //  Reported here rather than at the double-click, since this is where
        //  the width the host would store is finally known.
        if (m_list.m_onColumnResized)
        {
            m_list.m_onColumnResized ((int) idx, GetColumnOverrideWidthPx (idx));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetContentWidthPx
//
//  Sum of every visible column's natural width (no stretch fill). This
//  is the width the columns want, against which the horizontal scroll
//  range is measured.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetContentWidthPx() const
{
    int  total = 0;



    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        total += GetColumnNaturalWidthPx (c);
    }

    return total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCellTextRectPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListColumns::GetCellTextRectPx (int row, size_t column, RECT & outRect) const
{
    RECT  cell = {};



    //  An item's name is its label, wherever the view puts it.
    if (m_list.IsItemsView())
    {
        if (column != 0 || !m_list.m_items.GetItemRectPx (row, cell))
        {
            return false;
        }

        outRect = m_list.m_items.GetItemLabelRectPx (cell);

        return true;
    }

    int               rowH    = m_list.GetRowHeightPx();
    int               headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int               hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int               colOff  = m_list.m_hScrollEnabled ? -m_list.m_leftPx : 0;
    int               left    = 0;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    if (row < 0 || row >= m_list.GetRowCount() || m_list.m_grouping.GetLineOfRow (row) < m_list.m_topRow || m_list.m_grouping.GetLineOfRow (row) >= m_list.m_topRow + m_list.m_scrollbars.GetShownRowCount() ||
        column >= m_list.m_columns.size() || !m_list.m_columns[column].visible)
    {
        return false;
    }

    ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    if (colWPx[column] <= 0)
    {
        return false;
    }

    left = colXPx[column] + colOff + GetCellPadLeftPx (column);

    if (column < m_list.GetRowCells (row).size() && m_list.GetRowCells (row)[column].check != DxuiListViewTypes::Cell::Check::None)
    {
        left += m_list.GetCheckBoxSpanPx();
    }

    if (column < m_list.GetRowCells (row).size() && m_list.GetRowCells (row)[column].icon)
    {
        left += m_list.m_scaler.ToPx (DxuiListView::s_kCellIconDip) + m_list.GetCellIconGapPx();
    }

    outRect.left   = left;
    outRect.top    = headerH + hdrGap + (m_list.m_grouping.GetLineOfRow (row) - m_list.m_topRow) * rowH - m_list.m_scrollbars.GetRowShiftPx();
    outRect.right  = colXPx[column] + colOff + colWPx[column];
    outRect.bottom = outRect.top + rowH;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCellPadLeftPx
//
//  How far into its column a cell's text, or its icon, starts.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetCellPadLeftPx (size_t column) const
{
    if (!m_list.m_explorerDetails)
    {
        return m_list.m_scaler.ToPx (s_kCellPadLeftDip);
    }

    return (int) std::lround (m_list.m_scaler.ToPxf (((int) column == m_list.m_iconColumn) ? s_kExplorerIconPadDip : s_kExplorerCellPadDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHeaderTitlePadPx
//
//  How far into its column a header's title starts: with the cells, but for
//  Explorer's name column, whose title sits past where the icons are.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListColumns::GetHeaderTitlePadPx (size_t column) const
{
    if (m_list.m_explorerDetails && (int) column == m_list.m_iconColumn)
    {
        return (int) std::lround (m_list.m_scaler.ToPxf (s_kExplorerNameTitleDip));
    }

    return GetCellPadLeftPx (column);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeColumnLayout
//
//  Assigns each column an x-offset and width for the given content
//  width. Fixed/auto columns take their override / widthDip / measured
//  width; the first stretch column absorbs whatever space remains, down to
//  its own declared width and no further.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListColumns::ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const
{
    int  fixedTotal = 0;
    int  stretchIdx = -1;
    int  x          = m_list.GetDetailsLeftPx();



    xs.assign (m_list.m_columns.size(), 0);
    ws.assign (m_list.m_columns.size(), 0);

    for (size_t c = 0; c < m_list.m_columns.size(); ++c)
    {
        int  wpx = 0;

        if (!m_list.m_columns[c].visible)
        {
            ws[c] = 0;
            continue;
        }

        //  A stretch column the user has dragged keeps the width they gave
        //  it: absorbing the remainder is the DEFAULT, not a refusal to be
        //  resized, and a column whose drags did nothing reads as stuck.
        if (m_list.m_columns[c].stretch && stretchIdx == -1
                                 && !(c < m_overrideWPx.size() && m_overrideWPx[c] >= 0))
        {
            stretchIdx = (int) c;
            continue;
        }

        wpx = GetColumnNaturalWidthPx (c);

        ws[c]       = wpx;
        fixedTotal += wpx;
    }

    //  The stretch column gets the remaining width, but never less than its
    //  declared width. A narrow pane used to leave it zero width while
    //  GetContentWidthPx included the declared width, so the scroll range
    //  included a column that was never drawn. A stretch column declared at
    //  zero still shrinks to fit.
    if (stretchIdx >= 0)
    {
        int  rem     = (int) fullW - fixedTotal;
        int  floorPx = (m_list.m_columns[(size_t) stretchIdx].widthDip > 0)
                     ? m_list.m_scaler.ToPx (m_list.m_columns[(size_t) stretchIdx].widthDip)
                     : 0;

        ws[(size_t) stretchIdx] = (std::max) (rem, floorPx);
    }

    //  Left to right in the shown order, which a header drag changes.
    for (size_t n = 0; n < m_list.m_columns.size(); ++n)
    {
        size_t  c = (n < m_list.GetColumnOrder().size()) ? m_list.GetColumnOrder()[n] : n;

        xs[c]  = x;
        x     += ws[c];
    }
}
