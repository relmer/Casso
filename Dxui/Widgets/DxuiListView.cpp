#include "Pch.h"

#include "Widgets/DxuiListView.h"

#include "UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetRect
//
//  SetRows may have run before the host knew the paint rect; it would
//  have clamped m_topRow against a zero-capacity rect and, with
//  sticky-tail on, pinned m_topRow past the end. Re-clamp now that the
//  real capacity is known, preserving sticky-tail intent.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetRect (const RECT & rect)
{
    bool  wasSticky = m_stickyTail;
    int   maxTop    = 0;



    SetBounds (rect);
    maxTop = GetMaxTopRow();

    if (wasSticky || m_topRow > maxTop)
    {
        m_topRow = maxTop;
    }

    if (m_topRow < 0)
    {
        m_topRow = 0;
    }

    m_stickyTail = (m_topRow >= maxTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetColumns (std::vector<Column> cols)
{
    m_columns = std::move (cols);
    m_measuredWPx.clear();
    m_overrideWPx.assign (m_columns.size(), -1);

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
    bool  wasSticky = m_stickyTail;
    int   maxTop    = 0;



    m_rows = std::move (rows);
    maxTop = GetMaxTopRow();

    if (wasSticky || m_topRow > maxTop)
    {
        m_topRow = maxTop;
    }

    if (m_topRow < 0)
    {
        m_topRow = 0;
    }

    m_stickyTail = (m_topRow >= maxTop);
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
    bool  wasSticky = m_stickyTail;
    int   maxTop    = 0;



    if (rows.empty())
    {
        return;
    }

    m_rows.insert (m_rows.end(),
                   std::make_move_iterator (rows.begin()),
                   std::make_move_iterator (rows.end()));
    m_measuredWPx.clear();
    maxTop = GetMaxTopRow();

    if (wasSticky || m_topRow > maxTop)
    {
        m_topRow = maxTop;
    }

    if (m_topRow < 0)
    {
        m_topRow = 0;
    }

    m_stickyTail = (m_topRow >= maxTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColumnVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetColumnVisible (size_t idx, bool visible)
{
    HRESULT  hr = S_OK;



    CBRAEx (idx < m_columns.size(), E_INVALIDARG);

    m_columns[idx].visible = visible;

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

void DxuiListView::SetColumnOverrideWidthPx (size_t idx, int px)
{
    HRESULT hr = S_OK;



    CBRAEx (idx < m_columns.size(), E_INVALIDARG);

    if (m_overrideWPx.size() < m_columns.size())
    {
        m_overrideWPx.assign (m_columns.size(), -1);
    }

    m_overrideWPx[idx] = (px < 1 && px != -1) ? 1 : px;

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

int DxuiListView::GetColumnOverrideWidthPx (size_t idx) const
{
    HRESULT  hr     = S_OK;
    int      result = -1;



    CBRAEx (idx < m_overrideWPx.size(), E_INVALIDARG);

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

int DxuiListView::GetColumnEffectiveWidthPx (size_t idx) const
{
    HRESULT           hr      = S_OK;
    int               width   = -1;
    std::vector<int>  xs;
    std::vector<int>  ws;
    int               cap     = 0;
    bool              needBar = false;
    int               fullW   = 0;



    CBRAEx (idx < m_overrideWPx.size(), E_INVALIDARG);

    cap     = GetVisibleRowCapacity();
    needBar = ((int) m_rows.size() > cap) && (cap > 0);
    fullW   = (m_boundsDip.right - m_boundsDip.left) - (needBar ? GetScrollbarWidthPx() : 0);

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

int DxuiListView::GetTotalMeasuredWidthPx () const
{
    int  sum = 0;



    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        if (m_columns[c].widthDip > 0)
        {
            sum += m_scaler.Px (m_columns[c].widthDip);
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

void DxuiListView::MeasureColumnsPx (IDxuiTextRenderer & text)
{
    HRESULT  hr      = S_OK;
    float    fontDip = (float) m_scaler.Pxf (s_kFontDip);
    float    hdrDip  = (float) m_scaler.Pxf (s_kHeaderFontDip);
    int      padPx   = m_scaler.Px (s_kCellPadLeftDip) + m_scaler.Px (s_kCellPadRightDip);
    float    w       = 0.0f;
    float    h       = 0.0f;



    if (m_measuredWPx.size() != m_columns.size())
    {
        m_measuredWPx.assign (m_columns.size(), 0);
    }

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        int  wpx = 0;

        if (m_showHeader && !m_columns[c].title.empty())
        {
            hr = text.MeasureString (m_columns[c].title.c_str(), hdrDip, L"Segoe UI", w, h);
            IGNORE_RETURN_VALUE (hr, S_OK);

            // MeasureString uses regular weight; the header paints bold,
            // which is ~10% wider. Bump to avoid wrapping.
            wpx = std::max (wpx, (int) std::ceil (w * 1.12f));
        }

        for (const auto & row : m_rows)
        {
            if (c < row.size() && !row[c].text.empty())
            {
                hr = text.MeasureString (row[c].text.c_str(), fontDip, L"Segoe UI", w, h);
                IGNORE_RETURN_VALUE (hr, S_OK);

                wpx = std::max (wpx, (int) std::ceil (w));
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

void DxuiListView::ResetAutoFit ()
{
    m_autoMaxChars.clear();
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

void DxuiListView::UpdateAutoFitFromRows ()
{
    if (m_autoMaxChars.size() != m_columns.size())
    {
        m_autoMaxChars.assign (m_columns.size(), 0);
    }

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        int  maxChars = m_autoMaxChars[c];

        if (m_columns[c].widthDip != 0 || m_columns[c].stretch)
        {
            continue;
        }

        if (m_showHeader)
        {
            maxChars = std::max (maxChars, (int) m_columns[c].title.size());
        }

        for (const auto & row : m_rows)
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

int DxuiListView::GetVisibleColumnCount () const
{
    return (int) std::count_if (m_columns.begin(), m_columns.end(),
                                [] (const Column & c)
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

int DxuiListView::GetNthVisibleColumnIndex (int n) const
{
    int  result = -1;
    int  seen   = 0;



    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        if (!m_columns[c].visible)
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

int DxuiListView::GetVisibleIndexOfColumn (size_t absCol) const
{
    HRESULT  hr     = S_OK;
    int      result = -1;
    int      seen   = 0;



    CBRAEx      (absCol < m_columns.size(), E_INVALIDARG);
    BAIL_OUT_IF (!m_columns[absCol].visible, S_OK);

    for (auto col : m_columns)
    {
        if (col.visible)
        {
            ++seen;
        }
    }

    result = seen;

Error:
    return result;
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
    HRESULT  hr   = S_OK;
    int      rows = (int) m_rows.size();
    int      cap  = 0;



    if (r >= rows)
    {
        r = rows - 1;
    }

    if (r < 0)
    {
        r = -1;
    }

    BAIL_OUT_IF (r < 0, S_OK);

    cap = GetVisibleRowCapacity();
    BAIL_OUT_IF (cap <= 0, S_OK);

    if (r < m_topRow)
    {
        SetTopRow (r);
    }
    else if (r >= m_topRow + cap)
    {
        SetTopRow (r - cap + 1);
    }

Error:
    m_selectedRow = r;
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColumnNaturalWidthPx
//
//  Natural pixel width of a single column ignoring any stretch fill:
//  the user override if set, else the fixed widthDip, else the wider of
//  the batch-measured width and the incremental char-count auto-fit.
//  Returns 0 for a hidden column. Shared by ComputeColumnLayout and
//  GetContentWidthPx so both agree on a column's intrinsic size.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::ColumnNaturalWidthPx (size_t c) const
{
    int  wpx       = 0;
    int  padPx     = m_scaler.Px (s_kCellPadLeftDip) + m_scaler.Px (s_kCellPadRightDip);
    int  perCharPx = (int) std::ceil (m_scaler.Pxf (s_kFontDip) * s_kAutoCharWidthEm);



    if (m_columns[c].visible)
    {
        if (c < m_overrideWPx.size() && m_overrideWPx[c] >= 0)
        {
            wpx = m_overrideWPx[c];
        }
        else if (m_columns[c].widthDip > 0)
        {
            wpx = m_scaler.Px (m_columns[c].widthDip);
        }
        else if (c < m_measuredWPx.size() && m_measuredWPx[c] > 0)
        {
            wpx = m_measuredWPx[c];
        }
        else if (c < m_autoMaxChars.size() && m_autoMaxChars[c] > 0)
        {
            wpx = m_autoMaxChars[c] * perCharPx + padPx;
        }
    }

    return wpx;
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

int DxuiListView::GetContentWidthPx () const
{
    int  total = 0;



    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        total += ColumnNaturalWidthPx (c);
    }

    return total;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ComputeScrollLayout
//
//  Resolves the mutually-dependent vertical / horizontal scrollbar
//  state for the current rect. The horizontal bar (when enabled) steals
//  one bar-thickness of row capacity from the bottom; the vertical bar
//  steals one bar-thickness of viewport width from the right. Two
//  passes settle the dependency. With horizontal scroll disabled the
//  result is identical to the legacy vertical-only math.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ScrollLayout DxuiListView::ComputeScrollLayout () const
{
    ScrollLayout  layout;
    int           fullW = m_boundsDip.right  - m_boundsDip.left;
    int           fullH = m_boundsDip.bottom - m_boundsDip.top;
    int           barW  = GetScrollbarWidthPx();
    int           rowH  = m_scaler.Px (s_kRowHeightDip);
    int           hgTop = m_showHeader ? (m_scaler.Px (s_kHeaderHeightDip) + m_scaler.Px (s_kHeaderGapDip)) : 0;
    int           rows  = (int) m_rows.size();
    int           pass  = 0;



    layout.contentW  = GetContentWidthPx();
    layout.viewportW = fullW;

    for (pass = 0; pass < 2; ++pass)
    {
        int  viewportW = fullW - (layout.vBar ? barW : 0);
        int  bodyH     = 0;
        int  cap       = 0;

        layout.hBar     = m_hScrollEnabled && (viewportW > 0) && (layout.contentW > viewportW);
        bodyH           = (fullH - hgTop) - (layout.hBar ? barW : 0);
        cap             = (rowH > 0 && bodyH > 0) ? (bodyH / rowH) : 0;
        layout.rowCap   = cap;
        layout.vBar     = (cap > 0) && (rows > cap);
        layout.viewportW = fullW - (layout.vBar ? barW : 0);
    }

    return layout;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetVisibleRowCapacity
//
//  Number of data rows that fit in the current body height (minus the
//  horizontal scrollbar's reserved strip when it is showing).
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetVisibleRowCapacity () const
{
    return ComputeScrollLayout().rowCap;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxLeftPx
//
//  Largest valid horizontal offset: the excess of the natural content
//  width over the viewport content width (which excludes the vertical
//  scrollbar). Zero when the content fits.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetMaxLeftPx () const
{
    ScrollLayout  layout = ComputeScrollLayout();
    int           excess = layout.contentW - layout.viewportW;



    return (excess > 0) ? excess : 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetLeftPx
//
//  Sets the horizontal scroll offset, clamped to [0, GetMaxLeftPx()].
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetLeftPx (int leftPx)
{
    int  maxLeft = GetMaxLeftPx();



    if (leftPx < 0)
    {
        leftPx = 0;
    }

    if (leftPx > maxLeft)
    {
        leftPx = maxLeft;
    }

    m_leftPx = leftPx;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ScrollByWheelDeltaHorizontal
//
//  Shift-wheel horizontal scroll: a positive wheelDelta scrolls left
//  (toward the start), mirroring the vertical wheel's up-scrolls-up.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ScrollByWheelDeltaHorizontal (int wheelDelta, int pxPerNotch)
{
    HRESULT  hr      = S_OK;
    int      notches = 0;



    BAIL_OUT_IF (wheelDelta == 0, S_OK);

    notches = wheelDelta / WHEEL_DELTA;
    if (notches == 0)
    {
        notches = (wheelDelta > 0) ? 1 : -1;
    }

    SetLeftPx (m_leftPx - notches * pxPerNotch);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxTopRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetMaxTopRow () const
{
    int  cap  = GetVisibleRowCapacity();
    int  rows = (int) m_rows.size();



    return (rows > cap) ? (rows - cap) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTopRow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::SetTopRow (int topRow)
{
    int  maxTop = GetMaxTopRow();



    if (topRow < 0)
    {
        topRow = 0;
    }

    if (topRow > maxTop)
    {
        topRow = maxTop;
    }

    m_topRow = topRow;
    m_stickyTail = (m_topRow >= maxTop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScrollByWheelDelta
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ScrollByWheelDelta (int wheelDelta, int linesPerNotch)
{
    HRESULT  hr      = S_OK;
    int      notches = 0;



    BAIL_OUT_IF (wheelDelta == 0, S_OK);

    notches = wheelDelta / WHEEL_DELTA;
    if (notches == 0)
    {
        notches = (wheelDelta > 0) ? 1 : -1;
    }

    ScrollByRows (-notches * linesPerNotch);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsScrollbarVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsScrollbarVisible () const
{
    int  cap = GetVisibleRowCapacity();



    return (cap > 0) && ((int) m_rows.size() > cap);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScrollbarGeometry
//
//  Computes the geometry of every interactive scrollbar region in
//  coordinates relative to the widget rect's top-left. Up/down arrow
//  buttons bracket the track; the thumb travels within the track only.
//  arrowH == 0 means the bar is too short for arrow buttons and the
//  track spans the full bar height.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::ScrollbarMetrics DxuiListView::GetScrollbarGeometry () const
{
    HRESULT           hr      = S_OK;
    ScrollbarMetrics  m;
    ScrollLayout      layout  = ComputeScrollLayout();
    int               fullW   = m_boundsDip.right - m_boundsDip.left;
    int               barW    = GetScrollbarWidthPx();
    int               hBarH   = layout.hBar ? barW : 0;
    int               headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int               hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0;
    int               by      = headerH + hdrGap;
    int               bh      = (m_boundsDip.bottom - m_boundsDip.top) - by - hBarH;
    int               total   = (int) m_rows.size();
    int               cap     = layout.rowCap;
    int               maxTop  = GetMaxTopRow();
    int               arrowH  = barW;
    float             thumbH  = 0.0f;
    float             travel  = 0.0f;



    BAIL_OUT_IF (!IsScrollbarVisible() || bh <= 0 || barW <= 0 || total <= 0, S_OK);

    if (bh < arrowH * 2 + s_kMinThumbPx + 2)
    {
        arrowH = 0;
    }

    m.trackTop = by + arrowH;
    m.trackH   = bh - arrowH * 2;

    if (m.trackH < 0)
    {
        m.trackH = 0;
    }

    thumbH = std::max ((float) s_kMinThumbPx, (float) m.trackH * (float) cap / (float) total);

    if (thumbH > (float) m.trackH)
    {
        thumbH = (float) m.trackH;
    }

    travel = (float) m.trackH - thumbH;

    m.visible      = true;
    m.barX         = fullW - barW;
    m.barW         = barW;
    m.arrowH       = arrowH;
    m.upArrowTop   = by;
    m.downArrowTop = by + bh - arrowH;
    m.thumbH       = thumbH;
    m.thumbTop     = (float) m.trackTop + ((maxTop > 0) ? travel * (float) m_topRow / (float) maxTop : 0.0f);

Error:
    return m;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarThumb
//
//  True if (xPx, yPx), relative to the widget rect, lies on the thumb.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestScrollbarThumb (int xPx, int yPx) const
{
    HRESULT           hr     = S_OK;
    bool              result = false;
    ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (float) yPx >= m.thumbTop && (float) yPx < m.thumbTop + m.thumbH;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarTrack
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestScrollbarTrack (int xPx, int yPx) const
{
    HRESULT           hr     = S_OK;
    bool              result = false;
    ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.trackTop && yPx < m.trackTop + m.trackH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarArrowUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestScrollbarArrowUp (int xPx, int yPx) const
{
    HRESULT           hr     = S_OK;
    bool              result = false;
    ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowH <= 0, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.upArrowTop && yPx < m.upArrowTop + m.arrowH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestScrollbarArrowDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestScrollbarArrowDown (int xPx, int yPx) const
{
    HRESULT           hr     = S_OK;
    bool              result = false;
    ScrollbarMetrics  m      = GetScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowH <= 0, S_OK);
    BAIL_OUT_IF (xPx < m.barX || xPx >= m.barX + m.barW, S_OK);

    result = (yPx >= m.downArrowTop && yPx < m.downArrowTop + m.arrowH);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PageFromTrackClick
//
//  Pages the view by one visible-row capacity toward a track click
//  above (page up) or below (page down) the thumb.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PageFromTrackClick (int yPx)
{
    HRESULT           hr  = S_OK;
    ScrollbarMetrics  m   = GetScrollbarGeometry();
    int               cap = GetVisibleRowCapacity();



    BAIL_OUT_IF (!m.visible || cap <= 0, S_OK);

    if ((float) yPx < m.thumbTop)
    {
        SetTopRow (m_topRow - cap);
    }
    else
    {
        SetTopRow (m_topRow + cap);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginThumbDrag
//
//  grabYPx is the y inside the widget where the user grabbed the thumb;
//  we remember the offset between the click and the thumb's current top
//  so the thumb doesn't jump on the first move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::BeginThumbDrag (int grabYPx)
{
    ScrollbarMetrics  m = GetScrollbarGeometry();



    m_dragging   = true;
    m_dragGrabDy = (float) grabYPx - m.thumbTop;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::UpdateThumbDrag (int yPx)
{
    HRESULT           hr       = S_OK;
    ScrollbarMetrics  m        = GetScrollbarGeometry();
    int               maxTop   = GetMaxTopRow();
    float             travel   = 0.0f;
    float             thumbTop = 0.0f;
    float             ratio    = 0.0f;



    BAIL_OUT_IF (!m_dragging || !m.visible, S_OK);

    travel   = (float) m.trackH - m.thumbH;
    thumbTop = (float) yPx - m_dragGrabDy;
    ratio    = (travel > 0.0f) ? ((thumbTop - (float) m.trackTop) / travel) : 0.0f;

    SetTopRow ((int) std::lround (ratio * (float) maxTop));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHScrollbarVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::IsHScrollbarVisible () const
{
    return ComputeScrollLayout().hBar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHScrollbarGeometry
//
//  Horizontal counterpart to GetScrollbarGeometry: the bar runs along
//  the bottom of the list, spanning the viewport content width (full
//  width minus the vertical scrollbar). Left/right arrow buttons bracket
//  the track; the thumb travels within the track only. arrowW == 0 means
//  the bar is too short for arrow buttons.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::HScrollbarMetrics DxuiListView::GetHScrollbarGeometry () const
{
    HRESULT            hr       = S_OK;
    HScrollbarMetrics  m;
    ScrollLayout       layout   = ComputeScrollLayout();
    int                fullH    = m_boundsDip.bottom - m_boundsDip.top;
    int                barH     = GetScrollbarWidthPx();
    int                viewW    = layout.viewportW;
    int                contentW = layout.contentW;
    int                arrowW   = barH;
    int                maxLeft  = 0;
    float              thumbW   = 0.0f;
    float              travel   = 0.0f;



    BAIL_OUT_IF (!layout.hBar || viewW <= 0 || barH <= 0 || contentW <= 0, S_OK);

    if (viewW < arrowW * 2 + s_kMinThumbPx + 2)
    {
        arrowW = 0;
    }

    m.trackLeft = arrowW;
    m.trackW    = viewW - arrowW * 2;

    if (m.trackW < 0)
    {
        m.trackW = 0;
    }

    thumbW = std::max ((float) s_kMinThumbPx, (float) m.trackW * (float) viewW / (float) contentW);

    if (thumbW > (float) m.trackW)
    {
        thumbW = (float) m.trackW;
    }

    travel  = (float) m.trackW - thumbW;
    maxLeft = (contentW > viewW) ? (contentW - viewW) : 0;

    m.visible     = true;
    m.barY        = fullH - barH;
    m.barH        = barH;
    m.arrowW      = arrowW;
    m.leftArrowX  = 0;
    m.rightArrowX = viewW - arrowW;
    m.thumbW      = thumbW;
    m.thumbLeft   = (float) m.trackLeft + ((maxLeft > 0) ? travel * (float) m_leftPx / (float) maxLeft : 0.0f);

Error:
    return m;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHScrollbarThumb
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestHScrollbarThumb (int xPx, int yPx) const
{
    HRESULT            hr     = S_OK;
    bool               result = false;
    HScrollbarMetrics  m      = GetHScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (float) xPx >= m.thumbLeft && (float) xPx < m.thumbLeft + m.thumbW;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHScrollbarTrack
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestHScrollbarTrack (int xPx, int yPx) const
{
    HRESULT            hr     = S_OK;
    bool               result = false;
    HScrollbarMetrics  m      = GetHScrollbarGeometry();



    BAIL_OUT_IF (!m.visible, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.trackLeft && xPx < m.trackLeft + m.trackW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHScrollbarArrowLeft
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestHScrollbarArrowLeft (int xPx, int yPx) const
{
    HRESULT            hr     = S_OK;
    bool               result = false;
    HScrollbarMetrics  m      = GetHScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowW <= 0, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.leftArrowX && xPx < m.leftArrowX + m.arrowW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHScrollbarArrowRight
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListView::HitTestHScrollbarArrowRight (int xPx, int yPx) const
{
    HRESULT            hr     = S_OK;
    bool               result = false;
    HScrollbarMetrics  m      = GetHScrollbarGeometry();



    BAIL_OUT_IF (!m.visible || m.arrowW <= 0, S_OK);
    BAIL_OUT_IF (yPx < m.barY || yPx >= m.barY + m.barH, S_OK);

    result = (xPx >= m.rightArrowX && xPx < m.rightArrowX + m.arrowW);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PageFromHTrackClick
//
//  Pages the view by one viewport width toward a horizontal track click
//  left (page left) or right (page right) of the thumb.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PageFromHTrackClick (int xPx)
{
    HRESULT            hr     = S_OK;
    HScrollbarMetrics  m      = GetHScrollbarGeometry();
    int                viewW  = ComputeScrollLayout().viewportW;



    BAIL_OUT_IF (!m.visible || viewW <= 0, S_OK);

    if ((float) xPx < m.thumbLeft)
    {
        SetLeftPx (m_leftPx - viewW);
    }
    else
    {
        SetLeftPx (m_leftPx + viewW);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginHThumbDrag
//
//  grabXPx is the x inside the widget where the user grabbed the thumb;
//  we remember the offset between the click and the thumb's current left
//  so the thumb doesn't jump on the first move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::BeginHThumbDrag (int grabXPx)
{
    HScrollbarMetrics  m = GetHScrollbarGeometry();



    m_hDragging   = true;
    m_hDragGrabDx = (float) grabXPx - m.thumbLeft;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateHThumbDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::UpdateHThumbDrag (int xPx)
{
    HRESULT            hr        = S_OK;
    HScrollbarMetrics  m         = GetHScrollbarGeometry();
    int                maxLeft   = GetMaxLeftPx();
    float              travel    = 0.0f;
    float              thumbLeft = 0.0f;
    float              ratio     = 0.0f;



    BAIL_OUT_IF (!m_hDragging || !m.visible, S_OK);

    travel    = (float) m.trackW - m.thumbW;
    thumbLeft = (float) xPx - m_hDragGrabDx;
    ratio     = (travel > 0.0f) ? ((thumbLeft - (float) m.trackLeft) / travel) : 0.0f;

    SetLeftPx ((int) std::lround (ratio * (float) maxLeft));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequiredRowsForHeightPx
//
//  Number of data rows that fit within the given pixel height
//  (subtracting header + header gap). The caller uses this for manual
//  tail-virtualization: push only the most recent N rows.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetRequiredRowsForHeightPx (int heightPx) const
{
    HRESULT  hr      = S_OK;
    int      result  = 0;
    int      rowH    = m_scaler.Px (s_kRowHeightDip);
    int      headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int      hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0;
    int      body    = heightPx - headerH - hdrGap;



    BAIL_OUT_IF (rowH <= 0 || body <= 0, S_OK);

    result = body / rowH;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRequiredHeightPx
//
//  Body height required to host the current header + rows at the
//  current DPI. The caller uses this to size customBodyMinSizePx.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::GetRequiredHeightPx () const
{
    int  rows    = (int) m_rows.size();
    int  rowH    = m_scaler.Px (s_kRowHeightDip);
    int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int  hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0;



    return headerH + hdrGap + rows * rowH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestColumnResize
//
//  Returns the index of a column whose right-edge separator the point
//  is hovering, suitable for starting a column-resize drag, or -1 if
//  the point is outside the header strip or not near a resize handle.
//  tolerancePx is the half-width of the hit zone straddling the
//  separator. The last visible column has no right separator and is
//  excluded.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestColumnResize (int xPx, int yPx, int tolerancePx) const
{
    HRESULT  hr          = S_OK;
    int      result      = -1;
    int      headerH     = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int      cap         = GetVisibleRowCapacity();
    bool     needBar     = ((int) m_rows.size() > cap) && (cap > 0);
    int      fullW       = (m_boundsDip.right - m_boundsDip.left) - (needBar ? GetScrollbarWidthPx() : 0);
    int      xAdj        = m_hScrollEnabled ? (xPx + m_leftPx) : xPx;
    int      lastVisible = -1;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    BAIL_OUT_IF (!m_showHeader || headerH <= 0, S_OK);
    BAIL_OUT_IF (yPx < 0 || yPx >= headerH, S_OK);
    BAIL_OUT_IF (xPx < 0 || xPx >= fullW, S_OK);

    ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        if (m_columns[c].visible && colWPx[c] > 0)
        {
            lastVisible = (int) c;
        }
    }

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        if (!m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if ((int) c == lastVisible)
        {
            continue;
        }

        int  rightEdge = colXPx[c] + colWPx[c];
        if (xAdj >= rightEdge - tolerancePx && xAdj <= rightEdge + tolerancePx)
        {
            result = (int) c;
            break;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestHeaderColumn
//
//  xPx/yPx are relative to the list's rect.left/top. Returns the column
//  index of the header cell under the point (visible columns only), or
//  -1 if the point is not inside the header strip. Use this to
//  implement per-column sort on click.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestHeaderColumn (int xPx, int yPx) const
{
    HRESULT          hr      = S_OK;
    int              result  = -1;
    int              headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int              cap     = GetVisibleRowCapacity();
    bool             needBar = ((int) m_rows.size() > cap) && (cap > 0);
    int              fullW   = (m_boundsDip.right - m_boundsDip.left) - (needBar ? GetScrollbarWidthPx() : 0);
    int              xAdj    = m_hScrollEnabled ? (xPx + m_leftPx) : xPx;
    std::vector<int> colXPx;
    std::vector<int> colWPx;



    BAIL_OUT_IF (!m_showHeader || headerH <= 0, S_OK);
    BAIL_OUT_IF (yPx < 0 || yPx >= headerH, S_OK);
    BAIL_OUT_IF (xPx < 0 || xPx >= fullW, S_OK);

    ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        if (!m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if (xAdj >= colXPx[c] && xAdj < colXPx[c] + colWPx[c])
        {
            result = (int) c;
            break;
        }
    }

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTestRow
//
//  xPx/yPx are relative to the list's rect.left/top. Returns the
//  data-row index (into m_rows), accounting for the current scroll
//  offset, or -1 if the point is outside any visible data row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListView::HitTestRow (int xPx, int yPx) const
{
    HRESULT  hr      = S_OK;
    int      result  = -1;
    int      rowH    = m_scaler.Px (s_kRowHeightDip);
    int      headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0;
    int      hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0;
    int      body    = yPx - headerH - hdrGap;
    int      visIdx  = (body < 0 || rowH <= 0) ? -1 : (body / rowH);
    int      cap     = GetVisibleRowCapacity();
    int      abs     = (visIdx < 0) ? -1 : (m_topRow + visIdx);
    int      rowW    = (m_boundsDip.right - m_boundsDip.left) - ((int) m_rows.size() > cap ? GetScrollbarWidthPx() : 0);



    BAIL_OUT_IF (xPx < 0 || xPx >= rowW, S_OK);
    BAIL_OUT_IF (visIdx < 0 || visIdx >= cap, S_OK);
    BAIL_OUT_IF (abs < 0 || abs >= (int) m_rows.size(), S_OK);

    result = abs;

Error:
    return result;
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
    ScrollLayout     layout     = ComputeScrollLayout();
    float            x          = (float) m_boundsDip.left;
    float            y          = (float) m_boundsDip.top;
    float            fullW      = (float) (m_boundsDip.right - m_boundsDip.left);
    float            fullH      = (float) (m_boundsDip.bottom - m_boundsDip.top);
    int              visibleCap = layout.rowCap;
    int              totalRows  = (int) m_rows.size();
    int              firstRow   = m_topRow;
    int              lastRow    = std::min (totalRows, m_topRow + (visibleCap > 0 ? visibleCap : totalRows));
    float            barW       = layout.vBar ? (float) GetScrollbarWidthPx() : 0.0f;
    float            hBarH      = layout.hBar ? (float) GetScrollbarWidthPx() : 0.0f;
    float            layoutW    = fullW - barW;
    float            contentH   = fullH - hBarH;
    bool             clip       = m_hScrollEnabled;
    Palette          pal        = {};
    std::vector<int> colXPx;
    std::vector<int> colWPx;



    BAIL_OUT_IF (m_theme == nullptr || m_columns.empty(), S_OK);

    pal = MakePalette();

    ComputeColumnLayout (layoutW, colXPx, colWPx);

    painter.FillRect (x, y, fullW, fullH, pal.bgRow);

    if (clip)
    {
        hr = text.PushClipRect (x, y, layoutW, contentH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_showHeader)
    {
        PaintHeader (painter, text, pal, x, y, layoutW, colXPx, colWPx);
        PaintHeaderFocusMarkers (painter, pal, x, y, colXPx, colWPx);
    }

    PaintDataRows (painter, text, pal, x, y, layoutW, firstRow, lastRow, colXPx, colWPx);

    if (clip)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    PaintScrollbar  (painter, pal, x, y);
    PaintHScrollbar (painter, pal, x, y);

    if (layout.vBar && layout.hBar)
    {
        uint32_t  cornerArgb = (pal.fg & 0x00FFFFFFu) | 0x18000000u;

        painter.FillRect (x + layoutW, y + contentH, barW, hBarH, cornerArgb);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakePalette
//
//  Derives the per-element ARGB colors from the active theme. Called
//  once per paint; assumes the theme has already been null-checked.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView::Palette DxuiListView::MakePalette () const
{
    Palette  pal = {};



    pal.fg       = m_theme->Foreground();
    pal.fgDim    = (pal.fg & 0x00FFFFFFu) | 0xA0000000u;
    pal.hdrFg    = m_theme->HeadingForeground();
    pal.bgRow    = m_theme->BackgroundElevated();
    pal.bgHover  = m_theme->HoverBackground();
    pal.bgHeader = (pal.bgRow & 0x00FFFFFFu) | 0xFF000000u;
    pal.border   = (pal.fg    & 0x00FFFFFFu) | 0x30000000u;

    return pal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHeader
//
//  Paints the header background, the column titles, the active sort
//  glyph, the bottom border, and the faint vertical column separators.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintHeader (
    IDxuiPainter           & painter,
    IDxuiTextRenderer    & text,
    const Palette          & pal,
    float                    x,
    float                    y,
    float                    layoutW,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    HRESULT  hr        = S_OK;
    float    headerH   = (float) m_scaler.Px (s_kHeaderHeightDip);
    float    cellPadL  = (float) m_scaler.Px (s_kCellPadLeftDip);
    float    cellPadR  = (float) m_scaler.Px (s_kCellPadRightDip);
    float    hdrFontPx = (float) m_scaler.Pxf (s_kHeaderFontDip);
    float    colOff    = m_hScrollEnabled ? -(float) m_leftPx : 0.0f;



    painter.FillRect (x, y, layoutW, headerH, pal.bgHeader);

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        bool   hasSort     = ((int) c == m_sortColumn) && m_columns[c].visible && (colWPx[c] > 0);
        float  sortGlyphW  = (float) m_scaler.Px (s_kSortGlyphWidthDip);
        float  sortReserve = hasSort ? (sortGlyphW + cellPadR) : 0.0f;
        float  titleW      = (float) colWPx[c] - cellPadL - cellPadR - sortReserve;

        if (!m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        if (titleW < 0.0f)
        {
            titleW = 0.0f;
        }

        hr = text.DrawString (m_columns[c].title.c_str(),
                              x + colOff + (float) colXPx[c] + cellPadL,
                              y,
                              titleW,
                              headerH,
                              pal.hdrFg, hdrFontPx, L"Segoe UI",
                              m_columns[c].align,
                              DxuiTextVAlign::Center,
                              DWRITE_FONT_WEIGHT_BOLD,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (hasSort)
        {
            const wchar_t * glyph = m_sortDescending ? s_kpszTriangleDown : s_kpszTriangleUp;
            float           gw    = sortGlyphW;

            hr = text.DrawString (glyph,
                                  x + colOff + (float) colXPx[c] + (float) colWPx[c] - cellPadR - gw,
                                  y,
                                  gw,
                                  headerH,
                                  pal.hdrFg, hdrFontPx, L"Segoe UI",
                                  DxuiTextHAlign::Right,
                                  DxuiTextVAlign::Center,
                                  DWRITE_FONT_WEIGHT_BOLD);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    painter.FillRect (x, y + headerH - 1.0f, layoutW, 1.0f, pal.border);

    // Faint vertical separators between header columns so the user
    // can see where each column ends (and where the resize handle
    // lives). The separators scroll with the columns, so CPU-clip any
    // that fall outside the content viewport (IDxuiPainter has no clip).
    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        float  sepX = 0.0f;

        if (!m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        sepX = x + colOff + (float) colXPx[c] + (float) colWPx[c] - 1.0f;

        if (sepX < x || sepX >= x + layoutW)
        {
            continue;
        }

        painter.FillRect (sepX, y + 2.0f, 1.0f, headerH - 4.0f, pal.border);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHeaderFocusMarkers
//
//  Overlays the keyboard-focus markers: a 1px rectangle around the
//  header that Space will sort on, and a brighter 2px bar on the
//  column divider that Left/Right will resize.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintHeaderFocusMarkers (
    IDxuiPainter           & painter,
    const Palette          & pal,
    float                    x,
    float                    y,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    float     headerH   = (float) m_scaler.Px (s_kHeaderHeightDip);
    uint32_t  focusArgb = (pal.fg & 0x00FFFFFFu) | 0xC0000000u;
    float     colOff    = m_hScrollEnabled ? -(float) m_leftPx : 0.0f;
    float     clipR     = x + (float) ComputeScrollLayout().viewportW;



    if (m_focusedHeaderCol >= 0 
        && (size_t) m_focusedHeaderCol < m_columns.size()
        && m_columns[(size_t) m_focusedHeaderCol].visible
        && colWPx[(size_t) m_focusedHeaderCol] > 0)
    {
        float  fx    = x + colOff + (float) colXPx[(size_t) m_focusedHeaderCol];
        float  fw    = (float) colWPx[(size_t) m_focusedHeaderCol];
        float  left  = std::max (fx, x);
        float  right = std::min (fx + fw, clipR);

        if (right > left)
        {
            painter.FillRect (left, y,               right - left, 1.0f,    focusArgb);
            painter.FillRect (left, y + headerH - 1, right - left, 1.0f,    focusArgb);

            if (fx >= x && fx < clipR)
            {
                painter.FillRect (fx, y, 1.0f, headerH, focusArgb);
            }

            if (fx + fw - 1.0f >= x && fx + fw - 1.0f < clipR)
            {
                painter.FillRect (fx + fw - 1.0f, y, 1.0f, headerH, focusArgb);
            }
        }
    }

    if (m_focusedDividerCol >= 0 && (size_t) m_focusedDividerCol < m_columns.size()
        && m_columns[(size_t) m_focusedDividerCol].visible
        && colWPx[(size_t) m_focusedDividerCol] > 0)
    {
        float  dx = x + colOff + (float) colXPx[(size_t) m_focusedDividerCol]
                      + (float) colWPx[(size_t) m_focusedDividerCol] - 2.0f;

        if (dx >= x && dx < clipR)
        {
            painter.FillRect (dx, y + 1.0f, 2.0f, headerH - 2.0f, focusArgb);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintDataRows
//
//  Paints the visible data rows in [firstRow, lastRow): selection and
//  hover backgrounds followed by each cell's text.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintDataRows (
    IDxuiPainter           & painter,
    IDxuiTextRenderer    & text,
    const Palette          & pal,
    float                    x,
    float                    y,
    float                    layoutW,
    int                      firstRow,
    int                      lastRow,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    HRESULT  hr       = S_OK;
    float    rowH     = (float) m_scaler.Px (s_kRowHeightDip);
    float    headerH  = (float) (m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0);
    float    hdrGap   = (float) (m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0);
    float    cellPadL = (float) m_scaler.Px (s_kCellPadLeftDip);
    float    cellPadR = (float) m_scaler.Px (s_kCellPadRightDip);
    float    fontPx   = (float) m_scaler.Pxf (s_kFontDip);
    float    colOff   = m_hScrollEnabled ? -(float) m_leftPx : 0.0f;



    for (int r = firstRow; r < lastRow; ++r)
    {
        float  ry    = 0.0f;
        bool   isHov = false;
        bool   isSel = false;

        if (r < 0 || (size_t) r >= m_rows.size())
        {
            continue;
        }

        ry    = y + headerH + hdrGap + (float) (r - firstRow) * rowH;
        isHov = (r == m_hovered);
        isSel = (m_listFocused && r == m_selectedRow);

        if (isSel)
        {
            uint32_t  selArgb = (pal.bgHover & 0x00FFFFFFu) | 0xFF000000u;

            painter.FillRect (x, ry, layoutW, rowH, selArgb);
        }

        if (isHov)
        {
            painter.FillRect (x, ry, layoutW, rowH, pal.bgHover);
        }

        const auto & cells = m_rows[(size_t) r];

        for (size_t c = 0; c < m_columns.size() && c < cells.size(); ++c)
        {
            uint32_t  argb = cells[c].dim ? pal.fgDim : pal.fg;

            if (!m_columns[c].visible || colWPx[c] <= 0)
            {
                continue;
            }

            hr = text.DrawString (cells[c].text.c_str(),
                                  x + colOff + (float) colXPx[c] + cellPadL,
                                  ry,
                                  (float) colWPx[c] - cellPadL - cellPadR,
                                  rowH,
                                  argb, 
                                  fontPx, 
                                  L"Segoe UI",
                                  m_columns[c].align,
                                  DxuiTextVAlign::CenterOnCapHeight,
                                  DWRITE_FONT_WEIGHT_NORMAL,
                                  false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintScrollbar
//
//  Paints the scrollbar track, thumb, and (when the bar is tall
//  enough) the up/down arrow buttons. No-op when the bar is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintScrollbar (
    IDxuiPainter  & painter,
    const Palette & pal,
    float           x,
    float           y) const
{
    HRESULT           hr        = S_OK;
    ScrollLayout      layout    = ComputeScrollLayout();
    ScrollbarMetrics  m         = GetScrollbarGeometry();
    float             headerH   = (float) (m_showHeader ? m_scaler.Px (s_kHeaderHeightDip) : 0);
    float             hdrGap    = (float) (m_showHeader ? m_scaler.Px (s_kHeaderGapDip)    : 0);
    float             hBarH     = layout.hBar ? (float) GetScrollbarWidthPx() : 0.0f;
    uint32_t          trackArgb = (pal.fg & 0x00FFFFFFu) | 0x18000000u;
    uint32_t          thumbArgb = (pal.fg & 0x00FFFFFFu) | 0x80000000u;
    uint32_t          arrowArgb = (pal.fg & 0x00FFFFFFu) | 0xC0000000u;
    float             bx        = 0.0f;
    float             barTop    = 0.0f;
    float             barH      = 0.0f;



    BAIL_OUT_IF (!m.visible, S_OK);

    bx     = x + (float) m.barX;
    barTop = y + headerH + hdrGap;
    barH   = (float) (m_boundsDip.bottom - m_boundsDip.top) - headerH - hdrGap - hBarH;

    painter.FillRect (bx, barTop, (float) m.barW, barH, trackArgb);
    painter.FillRect (bx + 1.0f, y + m.thumbTop, (float) m.barW - 2.0f, m.thumbH, thumbArgb);

    if (m.arrowH > 0)
    {
        PaintScrollArrow (painter, bx, y + (float) m.upArrowTop,   (float) m.barW, (float) m.arrowH, true,  arrowArgb);
        PaintScrollArrow (painter, bx, y + (float) m.downArrowTop, (float) m.barW, (float) m.arrowH, false, arrowArgb);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintHScrollbar
//
//  Paints the horizontal scrollbar track, thumb, and (when the bar is
//  wide enough) the left/right arrow buttons along the bottom of the
//  list. No-op when the bar is hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintHScrollbar (
    IDxuiPainter  & painter,
    const Palette & pal,
    float           x,
    float           y) const
{
    HRESULT            hr        = S_OK;
    HScrollbarMetrics  m         = GetHScrollbarGeometry();
    uint32_t           trackArgb = (pal.fg & 0x00FFFFFFu) | 0x18000000u;
    uint32_t           thumbArgb = (pal.fg & 0x00FFFFFFu) | 0x80000000u;
    uint32_t           arrowArgb = (pal.fg & 0x00FFFFFFu) | 0xC0000000u;
    float              by        = 0.0f;
    float              trackX    = 0.0f;



    BAIL_OUT_IF (!m.visible, S_OK);

    by     = y + (float) m.barY;
    trackX = x + (float) m.trackLeft;

    painter.FillRect (trackX, by, (float) m.trackW, (float) m.barH, trackArgb);
    painter.FillRect (x + m.thumbLeft, by + 1.0f, m.thumbW, (float) m.barH - 2.0f, thumbArgb);

    if (m.arrowW > 0)
    {
        PaintScrollArrowH (painter, x + (float) m.leftArrowX,  by, (float) m.arrowW, (float) m.barH, true,  arrowArgb);
        PaintScrollArrowH (painter, x + (float) m.rightArrowX, by, (float) m.arrowW, (float) m.barH, false, arrowArgb);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeColumnLayout
//
//  Assigns each column an x-offset and width for the given content
//  width. Fixed/auto columns take their override / widthDip / measured
//  width; the first stretch column absorbs whatever space remains.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const
{
    int  fixedTotal = 0;
    int  stretchIdx = -1;
    int  x          = 0;



    xs.assign (m_columns.size(), 0);
    ws.assign (m_columns.size(), 0);

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        int  wpx = 0;

        if (!m_columns[c].visible)
        {
            ws[c] = 0;
            continue;
        }

        if (m_columns[c].stretch && stretchIdx == -1)
        {
            stretchIdx = (int) c;
            continue;
        }

        wpx = ColumnNaturalWidthPx (c);

        ws[c]       = wpx;
        fixedTotal += wpx;
    }

    if (stretchIdx >= 0)
    {
        int  rem = (int) fullW - fixedTotal;
        ws[(size_t) stretchIdx] = (rem > 0) ? rem : 0;
    }

    for (size_t c = 0; c < m_columns.size(); ++c)
    {
        xs[c]  = x;
        x     += ws[c];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintScrollArrow
//
//  Paints an upward (up == true) or downward triangle centered in the
//  given arrow-button rect, drawn as a stack of 1px-tall rows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintScrollArrow (IDxuiPainter & painter,
                                 float          ax,
                                 float          ay,
                                 float          aw,
                                 float          ah,
                                 bool           up,
                                 uint32_t       argb) const
{
    int    glyphH = std::max (3, (int) std::lround (ah * 0.30f));
    int    glyphW = glyphH * 2;
    float  gy     = ay + (ah - (float) glyphH) / 2.0f;
    int    r      = 0;



    for (r = 0; r < glyphH; r++)
    {
        float  frac = up ? (float) (r + 1) / (float) glyphH
                         : (float) (glyphH - r) / (float) glyphH;
        float  w    = (float) glyphW * frac;
        float  rx   = ax + (aw - w) / 2.0f;

        painter.FillRect (rx, gy + (float) r, w, 1.0f, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintScrollArrowH
//
//  Paints a left (left == true) or right triangle centered in the given
//  arrow-button rect, drawn as a row of 1px-wide columns.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::PaintScrollArrowH (IDxuiPainter & painter,
                                  float          ax,
                                  float          ay,
                                  float          aw,
                                  float          ah,
                                  bool           left,
                                  uint32_t       argb) const
{
    int    glyphW = std::max (3, (int) std::lround (aw * 0.30f));
    int    glyphH = glyphW * 2;
    float  gx     = ax + (aw - (float) glyphW) / 2.0f;
    int    c      = 0;



    for (c = 0; c < glyphW; c++)
    {
        float  frac = left ? (float) (c + 1) / (float) glyphW
                           : (float) (glyphW - c) / (float) glyphW;
        float  h    = (float) glyphH * frac;
        float  cy   = ay + (ah - h) / 2.0f;

        painter.FillRect (gx + (float) c, cy, 1.0f, h, argb);
    }
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
    SetBounds (boundsDip);
    SetRect   (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListView::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme == nullptr)
    {
        m_theme = &theme;
    }
    static_cast<const DxuiListView *> (this)->Paint (painter, text);
}
