#include "Pch.h"

#include "Widgets/ListView/DxuiListRows.h"
#include "Widgets/ListView/DxuiListIcons.h"
#include "Theme/DxuiRowLook.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::DxuiListRows
//
////////////////////////////////////////////////////////////////////////////////

DxuiListRows::DxuiListRows (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::OnPress
//
//  A press on a row selects it as the modifiers ask, and starts a drag that
//  extends the selection when multiple selection is on. A plain press on one
//  of several selected rows leaves the selection whole, so a drag that
//  follows takes all of it, as Explorer's does; the release collapses it to
//  the row if no drag began. Returns whether the press was on a row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnPress (const DxuiMouseEvent & ev, int row)
{
    if (row < 0)
    {
        return false;
    }

    m_list.m_focusGroup = -1;

    if (!ev.ctrl && !ev.shift && m_list.m_selectedRows.size() > 1 && m_list.IsRowSelected (row))
    {
        m_pressModified      = false;
        m_dragSelecting      = false;
        m_list.m_anchorRow   = row;
        m_list.m_selectedRow = row;
    }
    else
    {
        m_pressModified = ev.ctrl || ev.shift;
        m_list.ClickRow (row, ev.ctrl, ev.shift);
        m_dragSelecting = m_list.m_multiSelect;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::OnDrag
//
//  Returns whether a drag was extending the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnDrag (int ly)
{
    if (!m_dragSelecting)
    {
        return false;
    }

    DragSelectTo (ly);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::EndPress
//
//  Ends the press on a row as the button comes up. Returns whether it left a
//  range selected: a drag, or a Shift or Ctrl click, keeps what it left
//  selected, even a single row; only a plain click collapses the selection
//  to its row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::EndPress()
{
    bool  ranged = m_dragSelecting && (m_list.m_selectedRows.size() > 1 || m_pressModified);



    m_dragSelecting = false;
    m_pressModified = false;

    return ranged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::OnRelease
//
//  A release over a row selects it and activates it, or with double-click
//  activation, activates it on the second click on the same row inside the
//  system double-click time. Returns whether the release was over a row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnRelease (int row)
{
    bool  activate  = true;
    bool  collapsed = m_list.m_selectedRows.size() > 1;



    if (row < 0)
    {
        return false;
    }

    m_list.SetSelectedRow (row);

    //  A press on one row of several does not change the selection; the
    //  release that collapses it to the one row is the change.
    if (collapsed && m_list.m_onSelectionChanged)
    {
        m_list.m_onSelectionChanged (row);
    }

    if (m_activateOnDoubleClick)
    {
        int64_t  nowMs = (int64_t) GetTickCount64();

        activate = (row == m_lastClickRow) &&
                   (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime();

        // A completed double-click consumes the pair so a third click
        // starts a fresh count instead of chaining activations.
        m_lastClickRow = activate ? -1 : row;
        m_lastClickMs  = nowMs;
    }

    if (activate && m_list.m_onActivateRow)
    {
        m_list.m_onActivateRow (row);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::DragSelectTo
//
//  Extends the selection from the row the drag began on to the row under the
//  pointer, scrolling one row at a time when the pointer is past either edge.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::DragSelectTo (int ly)
{
    int  rowH    = m_list.GetRowHeightPx();
    int  headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int  hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int  body    = ly - headerH - hdrGap;
    int  row     = 0;
    int  line    = 0;



    //  A drag in an item view selects nothing more than the press did: rows
    //  of items have no single top-to-bottom order to extend along.
    if (rowH <= 0 || m_list.m_anchorRow < 0 || m_list.GetRowCount() == 0 || m_list.IsItemsView())
    {
        return;
    }

    line = (body < 0) ? m_list.m_topRow - 1 : m_list.m_topRow + (body + m_list.m_scrollbars.GetRowShiftPx()) / rowH;
    line = (std::max) (0, (std::min) (line, m_list.m_grouping.GetLineCount() - 1));
    row  = m_list.m_grouping.GetRowOfLine (line);

    //  On a header, the group's first row; on a collapsed one, nothing more.
    if (row < 0)
    {
        row = (std::min) (m_list.m_grouping.GetRowOfLine (line + 1), m_list.GetRowCount() - 1);
    }

    if (row < 0)
    {
        return;
    }

    if (row != m_list.m_selectedRow)
    {
        m_list.SelectRangeFromAnchor (row);
        m_list.m_selectedRow = row;
        m_list.EnsureVisible (row);

        if (m_list.m_onSelectionChanged)
        {
            m_list.m_onSelectionChanged (row);
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

void DxuiListRows::Paint (
    IDxuiPainter           & painter,
    IDxuiTextRenderer    & text,
    const DxuiListViewTypes::Palette          & pal,
    float                    x,
    float                    y,
    float                    layoutW,
    int                      firstRow,
    int                      lastRow,
    const std::vector<int> & colXPx,
    const std::vector<int> & colWPx) const
{
    HRESULT                                   hr        = S_OK;
    float                                     rowH      = (float) m_list.GetRowHeightPx();
    float                                     headerH   = (float) (m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0);
    float                                     hdrGap    = (float) (m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0);
    float                                     cellPadR  = (float) m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    float                                     fontPx    = (float) m_list.m_scaler.ToPxf (m_list.m_fontDip);
    float                                     colOff    = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    float                                     boxInsetY = m_list.m_textSelectionColors ? 0.0f : (float) m_list.m_scaler.ToPx (DxuiListView::s_kRowBoxInsetYDip);
    float                                     boxW      = layoutW;
    float                                     boxL      = (float) m_list.GetDetailsLeftPx();
    float                                     lastRight = 0.0f;
    float                                     clipTop   = 0.0f;
    float                                     clipH     = 0.0f;
    float                                     top       = 0.0f;
    bool                                      clipped   = false;
    std::vector<DxuiListViewTypes::LineSpot>  spots;



    //  A row's highlight ends where its columns do, less Explorer's margin,
    //  rather than running on across the empty width past them.
    for (size_t c = 0; c < m_list.m_columns.size() && c < colXPx.size() && c < colWPx.size(); ++c)
    {
        if (m_list.m_columns[c].visible && colWPx[c] > 0)
        {
            lastRight = (std::max) (lastRight, colOff + (float) (colXPx[c] + colWPx[c]));
        }
    }

    if (!m_list.m_textSelectionColors && lastRight > 0.0f)
    {
        boxW = std::clamp (lastRight - (float) m_list.m_scaler.ToPx (s_kRowBoxEndInsetDip), 0.0f, layoutW);
    }

    //  Where firstRow is drawn: the rows lifted at the end of the list or
    //  after a page down, and moved with a sliding page.
    top   = y + headerH + hdrGap - (float) m_list.m_scrollbars.GetRowShiftPx() + (float) m_list.m_items.GetPageSlidePx() - (float) (m_list.m_topRow - firstRow) * rowH;
    spots = m_list.m_grouping.PlaceDataLines (top, firstRow, lastRow, clipTop, clipH);

    for (const DxuiListViewTypes::LineSpot & spot : spots)
    {
        int                        r     = spot.row;
        float                      ry    = spot.y;

        if (spot.slid && !clipped)
        {
            hr = text.PushClipRect (x, clipTop, layoutW, clipH);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClipRect (x, clipTop, layoutW, clipH);
            clipped = true;
        }

        if (r < 0)
        {
            m_list.m_grouping.PaintGroupHeader (painter, text, pal, spot.group, x + colOff, x, ry, layoutW);
            continue;
        }

        const std::vector<DxuiListViewTypes::Cell>  & cells = m_list.GetRowCells (r);
        bool                                          isHov = (r == m_list.m_hovered);
        bool                       isSel = ((m_list.m_listFocused || m_list.m_alwaysShowSelection) &&
                                            (m_list.m_multiSelect ? m_list.IsRowSelected (r) : r == m_list.m_selectedRow));
        DxuiRowLook                look  = DxuiRowLook::Resolve (*m_list.m_theme, isSel, r == m_list.m_selectedRow && m_list.m_focusGroup < 0, isHov, m_list.m_listFocused);

        //  A text-like list (a hex dump) selects as text does: the text
        //  selection color, no outline.
        if (m_list.m_textSelectionColors)
        {
            look.fill = isSel ? pal.bgSel : (isHov ? pal.bgHover : 0u);
            look.edge = 0;
        }

        //  Square, as Explorer's are, and like Explorer's a little shorter
        //  than the row and ending short of the last column's edge.
        //  A drop target is drawn as a selected row other than the keyboard's:
        //  its fill, and the accent outline.
        if (r == m_list.m_dropRow)
        {
            look.fill = m_list.m_theme->ContentSelectionMulti();
            look.edge = m_list.m_theme->ContentSelectionMultiEdge();
        }

        if (look.fill != 0)
        {
            painter.FillRect (x + boxL, ry + boxInsetY, boxW - boxL, rowH - boxInsetY * 2.0f, look.fill);
        }

        if (look.edge != 0)
        {
            painter.OutlineRect (x + boxL, ry + boxInsetY, boxW - boxL, rowH - boxInsetY * 2.0f, DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), look.edge);
        }

        for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); ++c)
        {
            uint32_t      argb      = cells[c].dim ? pal.fgDim : (cells[c].argb != 0 ? cells[c].argb : pal.fg);
            float         padL      = (float) m_list.m_columnSizing.GetCellPadLeftPx (c);
            float         iconShift = 0.0f;
            float         alpha     = text.GetGlobalAlpha();
            std::wstring  shown;

            //  Explorer draws the columns after the name a step dimmer than it.
            if (m_list.m_explorerDetails && c > 0 && !cells[c].dim && cells[c].argb == 0)
            {
                argb = (pal.fg & 0x00FFFFFFu) | s_kSecondaryColumnAlpha;
            }

            if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
            {
                continue;
            }

            if (cells[c].icon && !cells[c].icon->bgraPremul.empty())
            {
                float  iconPx = m_list.m_scaler.ToPxf ((float) DxuiListView::s_kCellIconDip);

                if (cells[c].iconGhosted)
                {
                    text.SetGlobalAlpha (alpha * DxuiListIcons::s_kGhostedIconAlpha);
                }

                hr = text.DrawIconBitmap (cells[c].icon->bgraPremul.data(), cells[c].icon->width, cells[c].icon->height,
                                          x + colOff + (float) colXPx[c] + padL,
                                          ry + (rowH - iconPx) * 0.5f,
                                          iconPx, iconPx);
                IGNORE_RETURN_VALUE (hr, S_OK);

                text.SetGlobalAlpha (alpha);

                if (cells[c].iconBroken)
                {
                    DxuiListIcons::PaintBrokenBadge (text, x + colOff + (float) colXPx[c] + padL, ry + (rowH - iconPx) * 0.5f, iconPx, m_list.m_scaler.ToPxf (DxuiListView::s_kBrokenBadgeMinDip));
                }

                iconShift = iconPx + (float) m_list.GetCellIconGapPx();
            }
            else if ((int) c == m_list.m_iconColumn)
            {
                //  Its name still lines up with the others' names.
                iconShift = (float) (m_list.m_scaler.ToPx (DxuiListView::s_kCellIconDip) + m_list.GetCellIconGapPx());
            }

            // Search-match highlight: an accent band behind each matched
            // character range, painted before the text. Left-aligned columns
            // only -- the offset math measures prefixes from the cell's left.
            if (!cells[c].matches.empty() && m_list.m_columns[c].align == DxuiTextHAlign::Left)
            {
                const std::wstring &  cellText  = cells[c].text;
                float                 cellX     = x + colOff + (float) colXPx[c] + padL + iconShift;
                float                 cellMaxW  = (float) colWPx[c] - padL - cellPadR - iconShift;
                float                 bandInset = rowH * 0.14f;

                for (const std::pair<int, int> & mr : cells[c].matches)
                {
                    int      s       = std::clamp (mr.first,  0, (int) cellText.size());
                    int      e       = std::clamp (mr.second, s, (int) cellText.size());
                    float    wS      = 0.0f;
                    float    wE      = 0.0f;
                    float    hIgnore = 0.0f;
                    HRESULT  hrM     = S_OK;
                    float    hx      = 0.0f;
                    float    hw      = 0.0f;

                    if (s > 0)
                    {
                        hrM = text.MeasureString (cellText.substr (0, (size_t) s).c_str(),
                                                  fontPx, m_list.GetBodyFace(), wS, hIgnore);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    hrM = text.MeasureString (cellText.substr (0, (size_t) e).c_str(),
                                              fontPx, m_list.GetBodyFace(), wE, hIgnore);
                    IGNORE_RETURN_VALUE (hrM, S_OK);

                    hx = cellX + wS;
                    hw = wE - wS;

                    if (hx < cellX)                 { hw -= (cellX - hx); hx = cellX; }
                    if (hx + hw > cellX + cellMaxW) { hw  = cellX + cellMaxW - hx;    }

                    if (hw > 0.0f)
                    {
                        painter.FillRect (hx, ry + bandInset, hw, rowH - 2.0f * bandInset, pal.matchBg);
                    }
                }
            }

            // Muted ranges: the text is drawn a run at a time, each run placed
            // by measuring the text before it, the way a match band is placed.
            if (!cells[c].dimRanges.empty() && m_list.m_columns[c].align == DxuiTextHAlign::Left)
            {
                const std::wstring &  cellText = cells[c].text;
                float                 cellX    = x + colOff + (float) colXPx[c] + padL + iconShift;
                float                 cellMaxW = (float) colWPx[c] - padL - cellPadR - iconShift;
                int                   pos      = 0;
                size_t                next     = 0;

                while (pos < (int) cellText.size())
                {
                    bool      inRange = next < cells[c].dimRanges.size() && pos >= cells[c].dimRanges[next].first;
                    int       end     = inRange ? cells[c].dimRanges[next].second
                                                : (next < cells[c].dimRanges.size() ? cells[c].dimRanges[next].first : (int) cellText.size());
                    float     offset  = 0.0f;
                    float     hIgnore = 0.0f;
                    HRESULT   hrM     = S_OK;

                    end = std::clamp (end, pos + 1, (int) cellText.size());

                    if (pos > 0)
                    {
                        hrM = text.MeasureString (cellText.substr (0, (size_t) pos).c_str(), fontPx, m_list.GetBodyFace(), offset, hIgnore);
                        IGNORE_RETURN_VALUE (hrM, S_OK);
                    }

                    if (offset >= cellMaxW)
                    {
                        break;
                    }

                    hr = text.DrawString (cellText.substr ((size_t) pos, (size_t) (end - pos)).c_str(),
                                          cellX + offset,
                                          ry,
                                          cellMaxW - offset,
                                          rowH,
                                          inRange ? pal.fgDim : argb,
                                          fontPx,
                                          m_list.GetBodyFace(),
                                          DxuiTextHAlign::Left,
                                          DxuiTextVAlign::CenterOnCapHeight,
                                          DxuiFontWeight::Normal,
                                          false);
                    IGNORE_RETURN_VALUE (hr, S_OK);

                    next += inRange ? 1 : 0;
                    pos   = end;
                }

                continue;
            }

            //  Text wider than its column ends in an ellipsis, as Explorer's does,
            //  rather than being cut through a letter.
            shown = DxuiTextElide::ToWidth (text, cells[c].text, fontPx, m_list.GetBodyFace(),
                                            (float) colWPx[c] - padL - cellPadR - iconShift, DxuiElide::Tail);

            hr = text.DrawString (shown.c_str(),
                                  x + colOff + (float) colXPx[c] + padL + iconShift,
                                  ry,
                                  (float) colWPx[c] - padL - cellPadR - iconShift,
                                  rowH,
                                  argb,
                                  fontPx,
                                  m_list.GetBodyFace(),
                                  m_list.m_columns[c].align,
                                  DxuiTextVAlign::CenterOnCapHeight,
                                  DxuiFontWeight::Normal,
                                  false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }
}
