#pragma once

#include "Pch.h"

#include "../Chrome/ChromeTheme.h"
#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListView
//
//  Multi-column scrollable-less themed list. Replaces the ad-hoc
//  row-painting loops in the dialog and (eventually) Disk II debug
//  panel code paths. One row per data item, optional bold header row,
//  per-cell text + optional dim color override.
//
//  Sizing model: each column has a fixed width in DIPs, except one
//  column (designated via widthDp == 0) which stretches to fill the
//  remaining horizontal space. Row height and gap are uniform.
//
//  Interaction: HitTestRow(xPx, yPx) returns the data-row index under
//  a body-relative point, or -1 for the header / blank space. The
//  consumer owns hover/selection state and pushes it back in via
//  SetHoveredRow.
//
////////////////////////////////////////////////////////////////////////////////

class ListView
{
public:
    struct Column
    {
        std::wstring                  title;
        int                           widthDp = 0;     // 0 = auto-fit content (or stretch if stretch=true)
        bool                          stretch = false; // when true, column absorbs any remaining width after fixed/auto
        DwriteTextRenderer::HAlign    align   = DwriteTextRenderer::HAlign::Left;
        bool                          visible = true;
    };

    struct Cell
    {
        std::wstring  text;
        bool          dim = false;     // muted color (e.g. "(Download)" hint)
    };


    void  SetDpi          (UINT dpi)                       { m_scaler.SetDpi (dpi); }
    void  SetRect         (const RECT & rect)              { m_rect = rect; }
    void  SetTheme        (const ChromeTheme * theme)      { m_theme = theme; }
    void  SetShowHeader   (bool b)                         { m_showHeader = b; }
    void  SetHoveredRow   (int row)                        { m_hovered = row; }
    void  SetSortIndicator (int column, bool descending)   { m_sortColumn = column; m_sortDescending = descending; }

    void  SetColumnVisible (size_t idx, bool visible)
    {
        if (idx < m_columns.size())
        {
            m_columns[idx].visible = visible;
        }
    }
    bool  ColumnVisible    (size_t idx) const
    {
        return (idx < m_columns.size()) && m_columns[idx].visible;
    }

    void  SetColumns      (std::vector<Column> cols)       { m_columns = std::move (cols); m_measuredWPx.clear(); m_overrideWPx.assign (m_columns.size(), -1); }
    void  SetRows         (std::vector<std::vector<Cell>> rows)
    {
        bool  wasSticky = m_stickyTail;
        m_rows = std::move (rows);
        m_measuredWPx.clear();
        int  maxTop = MaxTopRow();
        if (wasSticky)        { m_topRow = maxTop; }
        if (m_topRow > maxTop) { m_topRow = maxTop; }
        if (m_topRow < 0)      { m_topRow = 0; }
        m_stickyTail = (m_topRow >= maxTop);
    }

    // User-resized column width (pixels). -1 means clear override and
    // fall back to widthDp / measured / stretch. Caller is responsible
    // for clamping to a sensible minimum (the widget enforces >= 1px).
    void  SetColumnOverrideWidthPx (size_t idx, int px)
    {
        if (idx >= m_columns.size()) { return; }
        if (m_overrideWPx.size() < m_columns.size())
        {
            m_overrideWPx.assign (m_columns.size(), -1);
        }
        m_overrideWPx[idx] = (px < 1 && px != -1) ? 1 : px;
    }
    int   ColumnOverrideWidthPx (size_t idx) const
    {
        if (idx >= m_overrideWPx.size()) { return -1; }
        return m_overrideWPx[idx];
    }
    // Current effective width of a column (override > widthDp > measured;
    // stretch returns whatever ComputeColumnLayout assigned this frame).
    int   ColumnEffectiveWidthPx (size_t idx) const
    {
        std::vector<int>  xs;
        std::vector<int>  ws;
        int  cap     = VisibleRowCapacity();
        bool needBar = ((int) m_rows.size() > cap) && (cap > 0);
        int  fullW   = (m_rect.right - m_rect.left) - (needBar ? ScrollbarWidthPx() : 0);

        if (idx >= m_columns.size()) { return 0; }
        ComputeColumnLayout ((float) fullW, xs, ws);
        return ws[idx];
    }

    // Returns the index of a column whose right-edge separator the
    // point is hovering, suitable for starting a column-resize drag.
    // Returns -1 if the point is outside the header strip or not near
    // any resize handle. tolerancePx is the half-width of the hit zone
    // straddling the separator. The last visible column has no right
    // separator (nothing to its right to push against) and is excluded.
    int   HitTestColumnResize (int xPx, int yPx, int tolerancePx) const
    {
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  cap     = VisibleRowCapacity();
        bool needBar = ((int) m_rows.size() > cap) && (cap > 0);
        int  fullW   = (m_rect.right - m_rect.left) - (needBar ? ScrollbarWidthPx() : 0);
        std::vector<int>  colXPx;
        std::vector<int>  colWPx;
        int  lastVisible = -1;

        if (!m_showHeader || headerH <= 0)            { return -1; }
        if (yPx < 0 || yPx >= headerH)                { return -1; }
        if (xPx < 0 || xPx >= fullW)                  { return -1; }

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
            if (!m_columns[c].visible || colWPx[c] <= 0) { continue; }
            if ((int) c == lastVisible)                   { continue; }
            int rightEdge = colXPx[c] + colWPx[c];
            if (xPx >= rightEdge - tolerancePx && xPx <= rightEdge + tolerancePx)
            {
                return (int) c;
            }
        }
        return -1;
    }

    int   HoveredRow      () const                         { return m_hovered; }
    int   RowCount        () const                         { return (int) m_rows.size(); }
    size_t        ColumnCount  () const                    { return m_columns.size(); }
    const Column & ColumnAt    (size_t idx) const          { return m_columns[idx]; }
    int           HeaderHeightPx () const                  { return m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0; }
    bool          ShowHeader     () const                  { return m_showHeader; }

    // Keyboard-driven selection / focus state. The host (panel) drives
    // these via Tab navigation; the widget owns rendering only.
    int   SelectedRow            () const                  { return m_selectedRow; }
    bool  ListFocused            () const                  { return m_listFocused; }
    int   FocusedHeaderColumn    () const                  { return m_focusedHeaderCol; }
    int   FocusedDividerColumn   () const                  { return m_focusedDividerCol; }

    void  SetListFocused           (bool b)                { m_listFocused = b; }
    void  SetFocusedHeaderColumn   (int c)                 { m_focusedHeaderCol  = (c < 0) ? -1 : c; }
    void  SetFocusedDividerColumn  (int c)                 { m_focusedDividerCol = (c < 0) ? -1 : c; }

    void  SetSelectedRow (int r)
    {
        int  rows = (int) m_rows.size();
        if (rows <= 0)            { m_selectedRow = -1; return; }
        if (r < -1)               { r = -1; }
        if (r >= rows)            { r = rows - 1; }
        m_selectedRow = r;
        if (r < 0) { return; }

        int  cap = VisibleRowCapacity();
        if (cap <= 0) { return; }
        if (r < m_topRow)                  { SetTopRow (r); }
        else if (r >= m_topRow + cap)      { SetTopRow (r - cap + 1); }
    }

    // Count of currently-visible columns (per m_columns[c].visible).
    int   VisibleColumnCount () const
    {
        int  n = 0;
        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            if (m_columns[c].visible) { ++n; }
        }
        return n;
    }

    // Absolute m_columns index of the n-th visible column, or -1.
    int   NthVisibleColumnIndex (int n) const
    {
        int  seen = 0;
        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            if (!m_columns[c].visible) { continue; }
            if (seen == n) { return (int) c; }
            ++seen;
        }
        return -1;
    }

    // Inverse: visible-column ordinal of the given absolute column,
    // or -1 if the column is hidden / out of range.
    int   VisibleIndexOfColumn (int absCol) const
    {
        if (absCol < 0 || (size_t) absCol >= m_columns.size()) { return -1; }
        if (!m_columns[(size_t) absCol].visible)                { return -1; }
        int  seen = 0;
        for (int c = 0; c < absCol; ++c)
        {
            if (m_columns[(size_t) c].visible) { ++seen; }
        }
        return seen;
    }

    // Vertical scroll API. The widget keeps a top-row index into
    // m_rows; Paint clips to [m_topRow, m_topRow + visibleCap) and
    // paints a scrollbar at the right edge when RowCount exceeds
    // capacity. "Sticky tail" auto-pins the view to the bottom when
    // new rows arrive while the user is already parked at the tail.
    int   ScrollbarWidthPx () const                        { return m_scaler.Px (s_kScrollbarWidthDp); }
    int   TopRow           () const                        { return m_topRow; }
    int   VisibleRowCapacity () const
    {
        int  rowH    = m_scaler.Px (s_kRowHeightDp);
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0;
        int  body    = (m_rect.bottom - m_rect.top) - headerH - hdrGap;
        if (rowH <= 0 || body <= 0) { return 0; }
        return body / rowH;
    }
    int   MaxTopRow        () const
    {
        int  cap  = VisibleRowCapacity();
        int  rows = (int) m_rows.size();
        return (rows > cap) ? (rows - cap) : 0;
    }
    bool  IsAtBottom       () const                        { return m_topRow >= MaxTopRow(); }
    void  EnableStickyTail (bool b)                        { m_stickyTail = b; }
    bool  StickyTail       () const                        { return m_stickyTail; }
    void  SetTopRow        (int topRow)
    {
        int  maxTop = MaxTopRow();
        if (topRow < 0)      { topRow = 0; }
        if (topRow > maxTop) { topRow = maxTop; }
        m_topRow = topRow;
        m_stickyTail = (m_topRow >= maxTop);
    }
    void  ScrollByRows     (int delta)                     { SetTopRow (m_topRow + delta); }
    void  ScrollByWheelDelta (int wheelDelta, int linesPerNotch = 3)
    {
        if (wheelDelta == 0) { return; }
        int  notches = wheelDelta / WHEEL_DELTA;
        if (notches == 0)
        {
            notches = (wheelDelta > 0) ? 1 : -1;
        }
        ScrollByRows (-notches * linesPerNotch);
    }

    // Measure each column's natural width from its header + cell text.
    // Caller invokes this once after SetColumns/SetRows to populate
    // auto-fit widths. The widget then uses these for layout and the
    // caller can read TotalMeasuredWidthPx() to size the host dialog.
    void  MeasureColumnsPx (DwriteTextRenderer & text)
    {
        HRESULT  hr        = S_OK;
        float    fontDip   = (float) m_scaler.Pxf (s_kFontDp);
        float    hdrDip    = (float) m_scaler.Pxf (s_kHeaderFontDp);
        int      padPx     = m_scaler.Px (s_kCellPadLeftDp) + m_scaler.Px (s_kCellPadRightDp);
        float    w         = 0.0f;
        float    h         = 0.0f;

        m_measuredWPx.assign (m_columns.size(), 0);

        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            int wpx = 0;

            if (m_showHeader && !m_columns[c].title.empty())
            {
                IGNORE_RETURN_VALUE (hr, text.MeasureString (m_columns[c].title.c_str(),
                                                             hdrDip, L"Segoe UI", w, h));
                // MeasureString uses regular weight; the header paints
                // bold, which is ~10% wider. Bump to avoid wrapping.
                wpx = std::max (wpx, (int) std::ceil (w * 1.12f));
            }

            for (const auto & row : m_rows)
            {
                if (c < row.size() && !row[c].text.empty())
                {
                    IGNORE_RETURN_VALUE (hr, text.MeasureString (row[c].text.c_str(),
                                                                 fontDip, L"Segoe UI", w, h));
                    wpx = std::max (wpx, (int) std::ceil (w));
                }
            }

            m_measuredWPx[c] = wpx + padPx;
        }
    }

    int   TotalMeasuredWidthPx () const
    {
        int sum = 0;
        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            if (m_columns[c].widthDp > 0)
            {
                sum += m_scaler.Px (m_columns[c].widthDp);
            }
            else if (c < m_measuredWPx.size())
            {
                sum += m_measuredWPx[c];
            }
        }
        return sum;
    }

    // Number of data rows that can fit within the given pixel height
    // (subtracting header + header gap). Caller uses this for manual
    // tail-virtualization: push only the most recent N rows.
    int   RequiredRowsForHeightPx (int heightPx) const
    {
        int  rowH    = m_scaler.Px (s_kRowHeightDp);
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0;
        int  body    = heightPx - headerH - hdrGap;
        if (rowH <= 0 || body <= 0) { return 0; }
        return body / rowH;
    }

    // Body-height required to host the current header + rows at the
    // current DPI. Caller uses this to size customBodyMinSizePx.
    int   RequiredHeightPx () const
    {
        int  rows     = (int) m_rows.size();
        int  rowH     = m_scaler.Px (s_kRowHeightDp);
        int  headerH  = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  hdrGap   = m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0;
        return headerH + hdrGap + rows * rowH;
    }

    // xPx/yPx are relative to the list's rect.left/top. Returns the
    // column index (visible columns only count their painted region;
    // hidden columns have zero width and never match) of the header
    // cell under the point, or -1 if the point is not inside the
    // header strip. Use this to implement per-column sort on click.
    int   HitTestHeaderColumn (int xPx, int yPx) const
    {
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  cap     = VisibleRowCapacity();
        bool needBar = ((int) m_rows.size() > cap) && (cap > 0);
        int  fullW   = (m_rect.right - m_rect.left) - (needBar ? ScrollbarWidthPx() : 0);
        std::vector<int>  colXPx;
        std::vector<int>  colWPx;

        if (!m_showHeader || headerH <= 0)
        {
            return -1;
        }
        if (yPx < 0 || yPx >= headerH)
        {
            return -1;
        }
        if (xPx < 0 || xPx >= fullW)
        {
            return -1;
        }

        ComputeColumnLayout ((float) fullW, colXPx, colWPx);

        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            if (!m_columns[c].visible || colWPx[c] <= 0)
            {
                continue;
            }
            if (xPx >= colXPx[c] && xPx < colXPx[c] + colWPx[c])
            {
                return (int) c;
            }
        }
        return -1;
    }

    // xPx/yPx are relative to the list's rect.left/top. Returns the
    // data-row index (into m_rows), or -1 if the point is outside any
    // currently-visible data row. The returned index already accounts
    // for the current scroll offset.
    int   HitTestRow (int xPx, int yPx) const
    {
        int  rowH    = m_scaler.Px (s_kRowHeightDp);
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0;
        int  body    = yPx - headerH - hdrGap;
        int  visIdx  = (body < 0 || rowH <= 0) ? -1 : (body / rowH);
        int  cap     = VisibleRowCapacity();
        int  abs     = (visIdx < 0) ? -1 : (m_topRow + visIdx);
        int  rowW    = (m_rect.right - m_rect.left) - ((int) m_rows.size() > cap ? ScrollbarWidthPx() : 0);

        if (xPx < 0 || xPx >= rowW)                  { return -1; }
        if (visIdx < 0 || visIdx >= cap)             { return -1; }
        if (abs < 0 || abs >= (int) m_rows.size())   { return -1; }

        return abs;
    }

    void  Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
    {
        HRESULT  hr        = S_OK;
        float    x         = (float) m_rect.left;
        float    y         = (float) m_rect.top;
        float    fullW     = (float) (m_rect.right - m_rect.left);
        float    rowH      = (float) m_scaler.Px (s_kRowHeightDp);
        float    headerH   = (float) (m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0);
        float    hdrGap    = (float) (m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0);
        float    cellPadL  = (float) m_scaler.Px (s_kCellPadLeftDp);
        float    cellPadR  = (float) m_scaler.Px (s_kCellPadRightDp);
        float    fontPx    = (float) m_scaler.Pxf (s_kFontDp);
        float    hdrFontPx = (float) m_scaler.Pxf (s_kHeaderFontDp);
        uint32_t fg        = 0;
        uint32_t fgDim     = 0;
        uint32_t hdrFg     = 0;
        uint32_t bgRow     = 0;
        uint32_t bgHover   = 0;
        uint32_t bgHeader  = 0;
        uint32_t border    = 0;
        std::vector<int>  colXPx;
        std::vector<int>  colWPx;
        int      visibleCap = VisibleRowCapacity();
        int      totalRows  = (int) m_rows.size();
        bool     needBar    = (totalRows > visibleCap) && (visibleCap > 0);
        float    barW       = needBar ? (float) ScrollbarWidthPx() : 0.0f;
        float    layoutW    = fullW - barW;


        if (m_theme == nullptr || m_columns.empty())
        {
            return;
        }

        fg       = m_theme->dropdownItemTextArgb;
        fgDim    = (fg & 0x00FFFFFFu) | 0xA0000000u;
        hdrFg    = m_theme->titleTextArgb;
        bgRow    = m_theme->dropdownBgArgb;
        bgHover  = m_theme->navHoverArgb;
        bgHeader = (bgRow & 0x00FFFFFFu) | 0xFF000000u;
        border   = (fg    & 0x00FFFFFFu) | 0x30000000u;

        ComputeColumnLayout (layoutW, colXPx, colWPx);

        painter.FillRect (x, y, fullW, (float) (m_rect.bottom - m_rect.top), bgRow);

        if (m_showHeader)
        {
            float  hy = y;

            painter.FillRect (x, hy, layoutW, headerH, bgHeader);

            for (size_t c = 0; c < m_columns.size(); ++c)
            {
                IGNORE_RETURN_VALUE (hr, text.DrawString (m_columns[c].title.c_str(),
                                                          x + (float) colXPx[c] + cellPadL,
                                                          hy,
                                                          (float) colWPx[c] - cellPadL - cellPadR,
                                                          headerH,
                                                          hdrFg, hdrFontPx, L"Segoe UI",
                                                          m_columns[c].align,
                                                          DwriteTextRenderer::VAlign::Center,
                                                          DWRITE_FONT_WEIGHT_BOLD));

                if ((int) c == m_sortColumn && m_columns[c].visible && colWPx[c] > 0)
                {
                    const wchar_t * glyph = m_sortDescending ? L"\u25BC" : L"\u25B2";
                    float           gw    = (float) m_scaler.Px (s_kSortGlyphWidthDp);

                    IGNORE_RETURN_VALUE (hr, text.DrawString (glyph,
                                                              x + (float) colXPx[c] + (float) colWPx[c] - cellPadR - gw,
                                                              hy,
                                                              gw,
                                                              headerH,
                                                              hdrFg, hdrFontPx, L"Segoe UI",
                                                              DwriteTextRenderer::HAlign::Right,
                                                              DwriteTextRenderer::VAlign::Center,
                                                              DWRITE_FONT_WEIGHT_BOLD));
                }
            }

            painter.FillRect (x, hy + headerH - 1.0f, layoutW, 1.0f, border);

            // Faint vertical separators between header columns so the
            // user can see where each column ends (and where the resize
            // handle lives).
            for (size_t c = 0; c < m_columns.size(); ++c)
            {
                if (!m_columns[c].visible || colWPx[c] <= 0) { continue; }
                float sepX = x + (float) colXPx[c] + (float) colWPx[c] - 1.0f;
                painter.FillRect (sepX, hy + 2.0f, 1.0f, headerH - 4.0f, border);
            }

            // Keyboard focus markers. The 1px header rectangle marks
            // the header that Space will sort on; the brighter 2px
            // vertical bar marks the column-right divider that
            // Left/Right will resize.
            uint32_t focusArgb = (fg & 0x00FFFFFFu) | 0xC0000000u;

            if (m_focusedHeaderCol >= 0 && (size_t) m_focusedHeaderCol < m_columns.size()
                && m_columns[(size_t) m_focusedHeaderCol].visible
                && colWPx[(size_t) m_focusedHeaderCol] > 0)
            {
                float fx = x + (float) colXPx[(size_t) m_focusedHeaderCol];
                float fw = (float) colWPx[(size_t) m_focusedHeaderCol];
                painter.FillRect (fx,             hy,               fw,   1.0f,        focusArgb);
                painter.FillRect (fx,             hy + headerH - 1, fw,   1.0f,        focusArgb);
                painter.FillRect (fx,             hy,               1.0f, headerH,     focusArgb);
                painter.FillRect (fx + fw - 1.0f, hy,               1.0f, headerH,     focusArgb);
            }

            if (m_focusedDividerCol >= 0 && (size_t) m_focusedDividerCol < m_columns.size()
                && m_columns[(size_t) m_focusedDividerCol].visible
                && colWPx[(size_t) m_focusedDividerCol] > 0)
            {
                float dx = x + (float) colXPx[(size_t) m_focusedDividerCol]
                             + (float) colWPx[(size_t) m_focusedDividerCol] - 2.0f;
                painter.FillRect (dx, hy + 1.0f, 2.0f, headerH - 2.0f, focusArgb);
            }
        }

        int  firstRow = m_topRow;
        int  lastRow  = std::min (totalRows, m_topRow + (visibleCap > 0 ? visibleCap : totalRows));

        for (int r = firstRow; r < lastRow; ++r)
        {
            if (r < 0 || (size_t) r >= m_rows.size()) { continue; }
            float  ry    = y + headerH + hdrGap + (float) (r - firstRow) * rowH;
            bool   isHov = (r == m_hovered);
            bool   isSel = (m_listFocused && r == m_selectedRow);

            if (isSel)
            {
                uint32_t selArgb = (bgHover & 0x00FFFFFFu) | 0xFF000000u;
                painter.FillRect (x, ry, layoutW, rowH, selArgb);
            }
            if (isHov)
            {
                painter.FillRect (x, ry, layoutW, rowH, bgHover);
            }

            const auto & cells = m_rows[(size_t) r];

            for (size_t c = 0; c < m_columns.size() && c < cells.size(); ++c)
            {
                uint32_t  argb = cells[c].dim ? fgDim : fg;

                IGNORE_RETURN_VALUE (hr, text.DrawString (cells[c].text.c_str(),
                                                          x + (float) colXPx[c] + cellPadL,
                                                          ry,
                                                          (float) colWPx[c] - cellPadL - cellPadR,
                                                          rowH,
                                                          argb, fontPx, L"Segoe UI",
                                                          m_columns[c].align,
                                                          DwriteTextRenderer::VAlign::CenterOnCapHeight));
            }
        }

        if (needBar)
        {
            float    bx          = x + fullW - barW;
            float    by          = y + headerH + hdrGap;
            float    bh          = (float) (m_rect.bottom - m_rect.top) - headerH - hdrGap;
            uint32_t trackArgb   = (fg & 0x00FFFFFFu) | 0x18000000u;
            uint32_t thumbArgb   = (fg & 0x00FFFFFFu) | 0x80000000u;
            float    thumbH      = std::max (16.0f, bh * (float) visibleCap / (float) totalRows);
            int      maxTop      = MaxTopRow();
            float    travel      = bh - thumbH;
            float    thumbY      = by + ((maxTop > 0) ? travel * (float) m_topRow / (float) maxTop : 0.0f);

            if (bh > 0.0f && barW > 0.0f)
            {
                painter.FillRect (bx, by, barW, bh, trackArgb);
                painter.FillRect (bx + 1.0f, thumbY, barW - 2.0f, thumbH, thumbArgb);
            }
        }
    }

private:
    static constexpr int  s_kRowHeightDp     = 30;
    static constexpr int  s_kHeaderHeightDp  = 26;
    static constexpr int  s_kHeaderGapDp     = 2;
    static constexpr int  s_kCellPadLeftDp   = 12;
    static constexpr int  s_kCellPadRightDp  = 16;
    static constexpr int  s_kSortGlyphWidthDp = 10;
    static constexpr int  s_kScrollbarWidthDp = 10;
    static constexpr float s_kFontDp         = 13.0f;
    static constexpr float s_kHeaderFontDp   = 13.0f;

    void  ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const
    {
        int  fixedTotal = 0;
        int  stretchIdx = -1;

        xs.assign (m_columns.size(), 0);
        ws.assign (m_columns.size(), 0);

        for (size_t c = 0; c < m_columns.size(); ++c)
        {
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

            int wpx = 0;
            if (c < m_overrideWPx.size() && m_overrideWPx[c] >= 0)
            {
                wpx = m_overrideWPx[c];
            }
            else if (m_columns[c].widthDp > 0)
            {
                wpx = m_scaler.Px (m_columns[c].widthDp);
            }
            else if (c < m_measuredWPx.size())
            {
                wpx = m_measuredWPx[c];
            }

            ws[c]       = wpx;
            fixedTotal += wpx;
        }

        if (stretchIdx >= 0)
        {
            int rem = (int) fullW - fixedTotal;
            ws[(size_t) stretchIdx] = (rem > 0) ? rem : 0;
        }

        int x = 0;
        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            xs[c]  = x;
            x     += ws[c];
        }
    }

    float RequiredHeight_F () const { return (float) RequiredHeightPx(); }

    RECT                              m_rect       = {};
    const ChromeTheme               * m_theme      = nullptr;
    std::vector<Column>               m_columns;
    std::vector<std::vector<Cell>>    m_rows;
    std::vector<int>                  m_measuredWPx;
    std::vector<int>                  m_overrideWPx;
    DpiScaler                         m_scaler;
    int                               m_hovered        = -1;
    int                               m_selectedRow    = -1;
    int                               m_sortColumn     = -1;
    bool                              m_sortDescending = false;
    bool                              m_showHeader     = false;
    int                               m_topRow         = 0;
    bool                              m_stickyTail     = true;
    bool                              m_listFocused      = false;
    int                               m_focusedHeaderCol  = -1;
    int                               m_focusedDividerCol = -1;
};
