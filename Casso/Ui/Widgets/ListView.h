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

    void  SetColumns      (std::vector<Column> cols)       { m_columns = std::move (cols); m_measuredWPx.clear(); }
    void  SetRows         (std::vector<std::vector<Cell>> rows) { m_rows = std::move (rows); m_measuredWPx.clear(); }

    int   HoveredRow      () const                         { return m_hovered; }
    int   RowCount        () const                         { return (int) m_rows.size(); }

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
    // data-row index, or -1 if the point is outside any data row.
    int   HitTestRow (int xPx, int yPx) const
    {
        int  rowH    = m_scaler.Px (s_kRowHeightDp);
        int  headerH = m_showHeader ? m_scaler.Px (s_kHeaderHeightDp) : 0;
        int  hdrGap  = m_showHeader ? m_scaler.Px (s_kHeaderGapDp)    : 0;
        int  body    = yPx - headerH - hdrGap;
        int  idx     = (body < 0) ? -1 : (body / rowH);

        if (xPx < 0 || xPx >= (m_rect.right - m_rect.left))
        {
            return -1;
        }

        if (idx < 0 || idx >= (int) m_rows.size())
        {
            return -1;
        }

        return idx;
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

        ComputeColumnLayout (fullW, colXPx, colWPx);

        painter.FillRect (x, y, fullW, RequiredHeight_F(), bgRow);

        if (m_showHeader)
        {
            float  hy = y;

            painter.FillRect (x, hy, fullW, headerH, bgHeader);

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
            }

            painter.FillRect (x, hy + headerH - 1.0f, fullW, 1.0f, border);
        }

        for (size_t r = 0; r < m_rows.size(); ++r)
        {
            float  ry    = y + headerH + hdrGap + (float) r * rowH;
            bool   isHov = ((int) r == m_hovered);

            if (isHov)
            {
                painter.FillRect (x, ry, fullW, rowH, bgHover);
            }

            const auto & cells = m_rows[r];

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
    }

private:
    static constexpr int  s_kRowHeightDp     = 30;
    static constexpr int  s_kHeaderHeightDp  = 26;
    static constexpr int  s_kHeaderGapDp     = 2;
    static constexpr int  s_kCellPadLeftDp   = 12;
    static constexpr int  s_kCellPadRightDp  = 16;
    static constexpr float s_kFontDp         = 14.0f;
    static constexpr float s_kHeaderFontDp   = 13.0f;

    void  ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const
    {
        int  fixedTotal = 0;
        int  stretchIdx = -1;

        xs.assign (m_columns.size(), 0);
        ws.assign (m_columns.size(), 0);

        for (size_t c = 0; c < m_columns.size(); ++c)
        {
            if (m_columns[c].stretch && stretchIdx == -1)
            {
                stretchIdx = (int) c;
                continue;
            }

            int wpx = 0;
            if (m_columns[c].widthDp > 0)
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
    DpiScaler                         m_scaler;
    int                               m_hovered    = -1;
    bool                              m_showHeader = false;
};
