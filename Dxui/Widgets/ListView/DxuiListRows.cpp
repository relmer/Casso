#include "Pch.h"

#include "Widgets/ListView/DxuiListRows.h"
#include "Widgets/ListView/DxuiListIcons.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiVisualStyleCheckBox.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"
#include "Widgets/DxuiCheckbox.h"
#include "Render/DxuiStroke.h"





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
    m_rowPressed        = true;

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
//  DxuiListRows::OnCheckPress
//
//  A press on a cell's check box toggles the box. Returns whether the press
//  was on one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnCheckPress (int lx, int row)
{
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int               xAdj    = m_list.m_hScrollEnabled ? (lx + m_list.m_leftPx) : lx;
    int               boxPx   = m_list.GetCheckBoxPx();
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    if (!m_list.m_onCheckToggled || row < 0 || m_list.IsItemsView())
    {
        return false;
    }

    const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (row);

    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); c++)
    {
        int  left = (c < colXPx.size()) ? colXPx[c] + (int) GetCheckBoxLeftPx (c, (float) m_list.m_columnSizing.GetCellPadLeftPx (c)) : 0;

        if (cells[c].check == DxuiListViewTypes::Cell::Check::None || !m_list.m_columns[c].visible || colWPx[c] <= 0)
        {
            continue;
        }

        //  An item check box adds its row to the selection or takes it out, as
        //  Explorer's does; any other check box selects its row alone.
        if (xAdj >= left && xAdj < left + boxPx)
        {
            if (!(m_list.m_itemCheckBoxes && c == 0))
            {
                m_list.ClickRow (row, false, false);
            }

            m_list.m_onCheckToggled (row);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::GetCheckBoxLeftPx
//
//  Where a column's check box starts in its cell: after the cell's padding,
//  or, for File Explorer's item check box, at Explorer's inset from the
//  row's left edge.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiListRows::GetCheckBoxLeftPx (size_t column, float padL) const
{
    bool  explorerItemBox = m_list.m_explorerCheckBoxes && m_list.m_itemCheckBoxes && column == 0;



    return explorerItemBox ? (float) m_list.GetCheckBoxLeftPx() : padL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::PaintCheckBox
//
//  A fixed box is drawn checked in the disabled color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::PaintCheckBox (IDxuiPainter                     & painter,
                                  IDxuiTextRenderer                & text,
                                  DxuiListViewTypes::Cell::Check     check,
                                  float                              left,
                                  float                              rowTop,
                                  float                              rowH,
                                  bool                               overText) const
{
    HRESULT             hr      = S_OK;
    const IDxuiTheme &  theme   = *m_list.m_theme;
    float               sizePx  = m_list.m_scaler.ToPxf ((float) DxuiListView::s_kCheckBoxDip);
    uint32_t            outline = (theme.Foreground() & 0x00FFFFFFu) | 0x99000000u;
    uint32_t            fill    = (check == DxuiListViewTypes::Cell::Check::OnFixed) ? theme.ForegroundDisabled() : theme.Accent();
    DxuiVisualStyleCheckBox::State  state = (check == DxuiListViewTypes::Cell::Check::Off)     ? DxuiVisualStyleCheckBox::State::Unchecked
                                          : (check == DxuiListViewTypes::Cell::Check::OnFixed) ? DxuiVisualStyleCheckBox::State::CheckedDisabled
                                                                                               : DxuiVisualStyleCheckBox::State::Checked;



    //  File Explorer's box is the visual style's, drawn over the text layer so
    //  that it is above an icon it overlaps.
    if (m_list.m_explorerCheckBoxes)
    {
        const DxuiIconImage &  image = DxuiVisualStyleCheckBox::GetImage (m_list.m_scaler.GetDpi(), state);

        if (!image.bgraPremul.empty())
        {
            hr = text.DrawIconBitmap (image.bgraPremul.data(), image.width, image.height,
                                      std::round (left), std::round (rowTop + (rowH - (float) image.height) * 0.5f),
                                      (float) image.width, (float) image.height);
            IGNORE_RETURN_VALUE (hr, S_OK);
            return;
        }
    }

    if (overText)
    {
        DxuiCheckbox::PaintBoxOverText (text, left, rowTop + (rowH - sizePx) * 0.5f, sizePx, m_list.m_scaler.ToPxf (1.0f),
                                        check != DxuiListViewTypes::Cell::Check::Off, theme.ButtonIdle(), outline, fill, DxuiTheme::GetContrastingText (fill));
    }
    else
    {
        DxuiCheckbox::PaintBox (painter, text, left, rowTop + (rowH - sizePx) * 0.5f, sizePx, m_list.m_scaler.ToPxf (1.0f),
                                check != DxuiListViewTypes::Cell::Check::Off, theme.ButtonIdle(), outline, fill, DxuiTheme::GetContrastingText (fill));
    }
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
//  A release with no press on a row in the list before it is ignored. When a
//  popup menu closes on its press, the release goes to the list under it,
//  over the row under the menu's item, and selected that row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnRelease (int row)
{
    bool  activate  = true;
    bool  collapsed = m_list.m_selectedRows.size() > 1;
    bool  pressed   = m_rowPressed;



    m_rowPressed = false;

    if (row < 0 || !pressed)
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
    int  visible = -1;



    //  A drag in an item view selects nothing more than the press did: rows
    //  of items have no single top-to-bottom order to extend along.
    if (rowH <= 0 || m_list.m_anchorRow < 0 || m_list.GetRowCount() == 0 || m_list.IsItemsView())
    {
        return;
    }

    line = (body < 0) ? m_list.m_topRow - 1 : m_list.m_topRow + (body + m_list.m_scrollbars.GetRowShiftPx()) / rowH;

    //  Wrapped rows are found by their heights; past the last one in view is
    //  the row after it.
    if (IsWrapped() && body >= 0)
    {
        visible = FindVisibleRow (body);
        line    = m_list.m_topRow + ((visible >= 0) ? visible : m_list.m_scrollbars.GetVisibleRowCapacity());
    }

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
//  DxuiListRows::IsWrapped
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::IsWrapped() const
{
    return m_wrapsText && !m_list.m_virtual && !m_list.IsItemsView() && !m_list.HasGroupLines();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::IsWrappedCell
//
//  A stretch-column cell that leaves elision unset. Its column's elision
//  applies only while the list doesn't wrap.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::IsWrappedCell (size_t column, const DxuiListViewTypes::Cell & cell) const
{
    return IsWrapped()
        && column < m_list.m_columns.size()
        && m_list.m_columns[column].stretch
        && cell.elide == DxuiElide::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::GetRowHeightPx
//
//  The uniform row height, or while wrapping, the row's measured height.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListRows::GetRowHeightPx (int row) const
{
    int  rowH = m_list.GetRowHeightPx();



    return (IsWrapped() && row >= 0 && row < (int) m_rowHeightsPx.size()) ? (std::max) (rowH, m_rowHeightsPx[(size_t) row]) : rowH;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::GetRowsHeightPx
//
//  The height of the rows from first up to end.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListRows::GetRowsHeightPx (int first, int end) const
{
    int  height = 0;



    for (int r = (std::max) (first, 0); r < end; ++r)
    {
        height += GetRowHeightPx (r);
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::CountRowsFitting
//
//  How many whole rows from topRow fit in bodyH. A row taller than the body
//  counts as one, so it is painted cut off.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListRows::CountRowsFitting (int topRow, int bodyH) const
{
    int  rows  = m_list.GetRowCount();
    int  used  = 0;
    int  count = 0;



    for (int r = (std::max) (topRow, 0); r < rows && used + GetRowHeightPx (r) <= bodyH; ++r)
    {
        used += GetRowHeightPx (r);
        count++;
    }

    return (count == 0 && bodyH > 0 && topRow < rows) ? 1 : count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::GetTopRowEndingAt
//
//  The smallest top row for which lastRow is the last whole row in view.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListRows::GetTopRowEndingAt (int lastRow, int bodyH) const
{
    int  top  = std::clamp (lastRow, 0, (std::max) (m_list.GetRowCount() - 1, 0));
    int  used = GetRowHeightPx (top);



    while (top > 0 && used + GetRowHeightPx (top - 1) <= bodyH)
    {
        top--;
        used += GetRowHeightPx (top);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::FindVisibleRow
//
//  The row at bodyY below the header, counted from the top row, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiListRows::FindVisibleRow (int bodyY) const
{
    int  found = -1;
    int  top   = 0;



    for (int r = m_list.m_topRow; bodyY >= 0 && found < 0 && r < m_list.GetRowCount() && top <= bodyY; ++r)
    {
        found = (bodyY < top + GetRowHeightPx (r)) ? (r - m_list.m_topRow) : -1;
        top  += GetRowHeightPx (r);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::MeasureForPaint
//
//  Measures the columns and, while wrapping, each row's height, then
//  resolves the scroll layout for them. When the wrapped rows are taller
//  than the body, the scroll bar narrows the wrapped column, so the rows
//  are measured again at that width.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListViewTypes::ScrollLayout DxuiListRows::MeasureForPaint (IDxuiTextRenderer & text) const
{
    DxuiListViewTypes::ScrollLayout  layout;
    int                              pass   = 0;



    if (m_list.m_theme != nullptr && !m_list.m_columns.empty())
    {
        m_list.m_columnSizing.EnsureMeasured (text);
    }

    layout = m_list.m_scrollbars.ComputeScrollLayout();

    for (pass = 0; IsWrapped() && m_list.m_theme != nullptr && pass < 2; ++pass)
    {
        MeasureRowHeights (text, layout.viewportW);
        layout = m_list.m_scrollbars.ComputeScrollLayout();
    }

    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::MeasureRowHeights
//
//  Each row's height for its wrapped cells: the uniform row height, plus
//  the height of every line past the first.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::MeasureRowHeights (IDxuiTextRenderer & text, int viewportW) const
{
    HRESULT           hr       = S_OK;
    int               rowH     = m_list.GetRowHeightPx();
    float             fontPx   = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    float             cellPadR = (float) m_list.m_scaler.ToPx (DxuiListView::s_kCellPadRightDip);
    int               wrapW    = -1;
    float             lineW    = 0.0f;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    m_list.m_columnSizing.ComputeColumnLayout ((float) viewportW, colXPx, colWPx);

    for (size_t c = 0; c < m_list.m_columns.size() && c < colWPx.size() && wrapW < 0; ++c)
    {
        wrapW = (m_list.m_columns[c].visible && m_list.m_columns[c].stretch) ? colWPx[c] : -1;
    }

    if (wrapW == m_wrapWidthPx && m_rowHeightsPx.size() == m_list.m_rowData.size())
    {
        return;
    }

    m_wrapWidthPx = wrapW;
    m_rowHeightsPx.assign (m_list.m_rowData.size(), rowH);

    hr = text.MeasureString (L"Ag", fontPx, m_list.GetBodyFace(), lineW, m_wrapLinePx);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (size_t r = 0; r < m_list.m_rowData.size(); ++r)
    {
        for (size_t c = 0; c < m_list.m_columns.size() && c < m_list.m_rowData[r].size() && c < colWPx.size(); ++c)
        {
            float  cellW = (float) colWPx[c] - (float) m_list.m_columnSizing.GetCellPadLeftPx (c) - cellPadR;
            float  textW = 0.0f;
            float  textH = 0.0f;

            if (IsWrappedCell (c, m_list.m_rowData[r][c]) && cellW > 0.0f)
            {
                hr = text.MeasureStringWrapped (m_list.m_rowData[r][c].text.c_str(), fontPx, m_list.GetBodyFace(), cellW, textW, textH);
                IGNORE_RETURN_VALUE (hr, S_OK);

                m_rowHeightsPx[r] = (std::max) (m_rowHeightsPx[r], rowH + (int) std::ceil ((std::max) (0.0f, textH - m_wrapLinePx)));
            }
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

    m_clipped.clear();

    //  Wrapped rows are placed one under another by their own heights.
    if (IsWrapped())
    {
        top = y + headerH + hdrGap;

        for (int r = (std::max) (firstRow, 0); r < (std::min) (lastRow, m_list.GetRowCount()); ++r)
        {
            spots.push_back ({ r, -1, top, false });
            top += (float) GetRowHeightPx (r);
        }
    }
    else
    {
        spots = m_list.m_grouping.PlaceDataLines (top, firstRow, lastRow, clipTop, clipH);
    }

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
        float                                         thisH = (float) GetRowHeightPx (r);
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
            painter.FillRect (x + boxL, ry + boxInsetY, boxW - boxL, thisH - boxInsetY * 2.0f, look.fill);
        }

        if (look.edge != 0)
        {
            painter.OutlineRect (x + boxL, ry + boxInsetY, boxW - boxL, thisH - boxInsetY * 2.0f, DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), look.edge);
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

            if (m_list.HasCheckBoxSpace (cells[c], c))
            {
                if (cells[c].check != DxuiListViewTypes::Cell::Check::None)
                {
                    PaintCheckBox (painter, text, cells[c].check, x + colOff + (float) colXPx[c] + GetCheckBoxLeftPx (c, padL), ry, rowH);
                }

                padL += (float) m_list.GetCheckBoxSpanPx();
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
                        painter.FillRect    (hx, ry + bandInset, hw, rowH - 2.0f * bandInset, pal.matchBg);
                        painter.OutlineRect (hx, ry + bandInset, hw, rowH - 2.0f * bandInset, 1.0f, pal.matchEdge);
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
            //  rather than being cut through a letter, unless the column or cell
            //  cuts it another way or the list wraps it. A cell cut short is
            //  noted, for a tip with its full text.
            {
                DxuiElide  mode    = (cells[c].elide != DxuiElide::None) ? cells[c].elide
                                   : (m_list.m_columns[c].elide != DxuiElide::None) ? m_list.m_columns[c].elide : DxuiElide::Tail;
                bool       isWrap  = IsWrappedCell (c, cells[c]);
                bool       isTall  = thisH > rowH;
                float      linePad = (rowH - m_wrapLinePx) / 2.0f;
                float      cellX   = x + colOff + (float) colXPx[c] + padL + iconShift;

                shown = isWrap ? cells[c].text : DxuiTextElide::ToWidth (text, cells[c].text, fontPx, m_list.GetBodyFace(),
                                                                          (float) colWPx[c] - padL - cellPadR - iconShift, mode);

                if (shown != cells[c].text)
                {
                    m_clipped.push_back ({ r, c });
                }

                if (cells[c].isHeading)
                {
                    float  size  = fontPx * 0.3f;
                    float  cx    = cellX + size * 1.5f;
                    float  cy    = ry + rowH / 2.0f;
                    float  thick = (std::max) (1.0f, m_list.m_scaler.ToPxf (1.0f));

                    if (cells[c].isOpen)
                    {
                        DxuiStroke::Segment (painter, cx - size, cy - size / 2.0f, cx, cy + size / 2.0f, thick, argb);
                        DxuiStroke::Segment (painter, cx, cy + size / 2.0f, cx + size, cy - size / 2.0f, thick, argb);
                    }
                    else
                    {
                        DxuiStroke::Segment (painter, cx - size / 2.0f, cy - size, cx + size / 2.0f, cy, thick, argb);
                        DxuiStroke::Segment (painter, cx + size / 2.0f, cy, cx - size / 2.0f, cy + size, thick, argb);
                    }
                }

                hr = text.DrawString (shown.c_str(),
                                      cellX,
                                      isTall ? ry + linePad : ry,
                                      (float) colWPx[c] - padL - cellPadR - iconShift,
                                      isTall ? thisH - 2.0f * linePad : rowH,
                                      argb,
                                      fontPx,
                                      m_list.GetBodyFace(),
                                      m_list.m_columns[c].align,
                                      isTall ? DxuiTextVAlign::Top : DxuiTextVAlign::CenterOnCapHeight,
                                      cells[c].isStrong ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal,
                                      isWrap);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClipRect();
    }
}
