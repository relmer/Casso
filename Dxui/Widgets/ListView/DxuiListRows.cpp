#include "Pch.h"

#include "Widgets/ListView/DxuiListRows.h"
#include "Widgets/ListView/DxuiListIcons.h"
#include "Widgets/ListView/DxuiListFind.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiSystemSettings.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiVisualStyleCheckBox.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiIconImage.h"
#include "Widgets/DxuiCheckbox.h"





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
//  A press on a cell's check box toggles the box; a fixed one takes the
//  press and stays as it is. An item check box adds its row to the
//  selection or takes it out, as Explorer's does, by way of the host. Any
//  other check box selects its row alone, unless SetSelectOnCheckPress turns
//  that off, as Visual Studio's Breakpoints window has it.
//  Returns whether the press was on one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::OnCheckPress (int lx, int ly, int row)
{
    RECT  box = {};



    if (row < 0 || m_list.IsItemsView())
    {
        return false;
    }

    const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (row);

    for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); c++)
    {
        DxuiListViewTypes::Cell::Check  check = cells[c].check;

        if (!GetCheckRectPx (row, c, box) || lx < box.left || lx >= box.right || ly < box.top || ly >= box.bottom)
        {
            continue;
        }

        if (check == DxuiListViewTypes::Cell::Check::OnFixed || check == DxuiListViewTypes::Cell::Check::OffFixed)
        {
            return true;
        }

        if (!(m_list.m_itemCheckBoxes && c == 0) && m_selectOnCheckPress)
        {
            m_list.ClickRow (row, false, false);
        }

        if (m_list.m_onCheckToggled)
        {
            m_list.m_onCheckToggled (row, c, check != DxuiListViewTypes::Cell::Check::On);
        }

        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::GetCheckRectPx
//
//  Where a cell's check box is drawn and hit, relative to the list's
//  top-left. The press takes the whole height of the row across the box's
//  width, as a Windows list's does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::GetCheckRectPx (int row, size_t column, RECT & outRect) const
{
    int               cap     = m_list.m_scrollbars.GetVisibleRowCapacity();
    bool              needBar = (m_list.m_grouping.GetLineCount() > cap) && (cap > 0);
    int               fullW   = (m_list.m_boundsDip.right - m_list.m_boundsDip.left) - (needBar ? m_list.GetScrollbarWidthPx() : 0);
    int               colOff  = m_list.m_hScrollEnabled ? -m_list.m_leftPx : 0;
    int               boxPx   = m_list.GetCheckBoxPx();
    int               rowH    = m_list.GetRowHeightPx();
    int               line    = 0;
    int               top     = 0;
    std::vector<int>  colXPx;
    std::vector<int>  colWPx;



    if (m_list.IsItemsView() || row < 0 || row >= m_list.GetRowCount() || column >= m_list.m_columns.size() ||
        !m_list.m_columns[column].visible || column >= m_list.GetRowCells (row).size() ||
        m_list.GetRowCells (row)[column].check == DxuiListViewTypes::Cell::Check::None)
    {
        return false;
    }

    line = m_list.m_grouping.GetLineOfRow (row);

    if (line < m_list.m_topRow || line >= m_list.m_topRow + m_list.m_scrollbars.GetShownRowCount())
    {
        return false;
    }

    m_list.m_columnSizing.ComputeColumnLayout ((float) fullW, colXPx, colWPx);

    if (colWPx[column] <= 0)
    {
        return false;
    }

    top     = m_list.GetHeaderHeightPx() + (m_list.m_showHeader ? m_list.GetHeaderGapPx() : 0) + (line - m_list.m_topRow) * rowH - m_list.m_scrollbars.GetRowShiftPx();
    outRect = RECT { colXPx[column] + colOff + (int) GetCheckBoxLeftPx (column, (float) m_list.m_columnSizing.GetCellPadLeftPx (column)), top + (rowH - boxPx) / 2, 0, 0 };

    outRect.right  = outRect.left + boxPx;
    outRect.bottom = outRect.top  + boxPx;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows::ToggleChecks
//
//  Space toggles the check boxes of the selected rows: each row's first
//  box, to the opposite of the keyboard row's box. A row whose
//  first box is fixed is left alone. Without `apply`, only whether there is
//  a box to toggle.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListRows::ToggleChecks (bool apply)
{
    std::vector<int>     rows   = { m_list.m_selectedRow };
    std::optional<bool>  target;



    //  The keyboard row first, so its state decides; then the rest once each.
    for (int row : m_list.m_selectedRows)
    {
        if (row != m_list.m_selectedRow)
        {
            rows.push_back (row);
        }
    }

    for (int row : rows)
    {
        if (row < 0 || row >= m_list.GetRowCount())
        {
            continue;
        }

        const std::vector<DxuiListViewTypes::Cell> &  cells = m_list.GetRowCells (row);

        for (size_t c = 0; c < cells.size() && c < m_list.m_columns.size(); c++)
        {
            DxuiListViewTypes::Cell::Check  check = cells[c].check;

            if (check == DxuiListViewTypes::Cell::Check::None)
            {
                continue;
            }

            if (check == DxuiListViewTypes::Cell::Check::OnFixed || check == DxuiListViewTypes::Cell::Check::OffFixed)
            {
                break;
            }

            if (!target.has_value())
            {
                target = check != DxuiListViewTypes::Cell::Check::On;
            }

            if (apply && m_list.m_onCheckToggled)
            {
                m_list.m_onCheckToggled (row, c, *target);
            }

            break;
        }
    }

    return target.has_value();
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
    bool                isFixed = check == DxuiListViewTypes::Cell::Check::OnFixed || check == DxuiListViewTypes::Cell::Check::OffFixed;
    bool                isOn    = check == DxuiListViewTypes::Cell::Check::On || check == DxuiListViewTypes::Cell::Check::OnFixed;
    uint32_t            fill    = isFixed ? theme.ForegroundDisabled() : theme.Accent();
    DxuiVisualStyleCheckBox::State  state = !isOn                                              ? DxuiVisualStyleCheckBox::State::Unchecked
                                          : (check == DxuiListViewTypes::Cell::Check::OnFixed) ? DxuiVisualStyleCheckBox::State::CheckedDisabled
                                                                                               : DxuiVisualStyleCheckBox::State::Checked;



    //  File Explorer's box is the visual style's, drawn over the text layer so
    //  that it is above an icon it overlaps. The style has no clear box that
    //  is disabled, so a fixed clear box is drawn as the others are, in the
    //  disabled color.
    if (isFixed && !isOn)
    {
        outline = theme.ForegroundDisabled();
    }
    else if (m_list.m_explorerCheckBoxes)
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
                                        isOn, theme.ButtonIdle(), outline, fill, DxuiTheme::GetContrastingText (fill));
    }
    else
    {
        //  A details row shorter than the box with its margin has a smaller
        //  box, so the box is inside the row's fill.
        sizePx = std::min (sizePx, std::floor (rowH - 2.0f * m_list.m_scaler.ToPxf ((float) s_kCheckBoxRowMarginDip)));

        DxuiCheckbox::PaintBox (painter, text, left, rowTop + (rowH - sizePx) * 0.5f, sizePx, m_list.m_scaler.ToPxf (1.0f),
                                isOn, theme.ButtonIdle(), outline, fill, DxuiTheme::GetContrastingText (fill));
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

bool DxuiListRows::OnRelease (int row, int lx, int ly)
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
                   (nowMs - m_lastClickMs) <= (int64_t) DxuiSystemSettings::Instance().GetDoubleClickMs();

        // A completed double-click consumes the pair so a third click
        // starts a fresh count instead of chaining activations.
        m_lastClickRow = activate ? -1 : row;
        m_lastClickMs  = nowMs;
    }

    //  In text, a double-click is a word, as it is in an editor.
    if (activate && m_activateOnDoubleClick && m_list.m_textSelect.IsEnabled())
    {
        m_list.m_textSelect.SelectWordAt (lx, ly);
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
//  Paint
//
//  Paints the visible data rows in [firstRow, lastRow): selection and
//  hover backgrounds followed by each cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::Paint (
    IDxuiPainter                     & painter,
    IDxuiTextRenderer                & text,
    const DxuiListViewTypes::Palette & pal,
    float                              x,
    float                              y,
    float                              layoutW,
    int                                firstRow,
    int                                lastRow,
    const std::vector<int>           & colXPx,
    const std::vector<int>           & colWPx) const
{
    HRESULT                                   hr        = S_OK;
    float                                     rowH      = (float) m_list.GetRowHeightPx();
    float                                     headerH   = (float) m_list.GetHeaderHeightPx();
    float                                     hdrGap    = (float) (m_list.m_showHeader ? m_list.GetHeaderGapPx() : 0);
    float                                     colOff    = m_list.m_hScrollEnabled ? -(float) m_list.m_leftPx : 0.0f;
    float                                     boxInsetY = m_list.m_textSelectionColors ? 0.0f : (float) m_list.m_scaler.ToPx (DxuiListView::s_kRowBoxInsetYDip);
    float                                     boxW      = layoutW;
    float                                     boxL      = (float) m_list.GetDetailsLeftPx();
    int                                       firstX    = m_list.GetDetailsLeftPx() + m_list.m_columnSizing.GetPaneLeadPx();
    float                                     lastRight = 0.0f;
    float                                     clipTop   = 0.0f;
    float                                     clipH     = 0.0f;
    float                                     top       = 0.0f;
    bool                                      clipped   = false;
    std::vector<DxuiListViewTypes::LineSpot>  spots;



    m_list.m_measureText = &text;

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
        int    r  = spot.row;
        float  ry = spot.y;

        if (spot.slid && !clipped)
        {
            hr = text.PushClipRect (x, clipTop, layoutW, clipH);
            IGNORE_RETURN_VALUE (hr, S_OK);
            painter.PushClip (x, clipTop, layoutW, clipH);
            clipped = true;
        }

        if (r < 0)
        {
            m_list.m_grouping.PaintGroupHeader (painter, text, pal, spot.group, x + colOff, x, ry, layoutW);
            continue;
        }

        const std::vector<DxuiListViewTypes::Cell>  & cells      = m_list.GetRowCells (r);
        bool                                          isHov      = (r == m_list.m_hovered);
        //  Selected text is the selection while there is any; the row it
        //  began on is not marked as well.
        bool                                          isSel      = ((m_list.m_listFocused || m_list.m_alwaysShowSelection) && !m_list.m_textSelect.HasSelection() &&
                                                                    (m_list.m_multiSelect ? m_list.IsRowSelected (r) : r == m_list.m_selectedRow));
        //  A row an in-place tip is up for is drawn in the selection color,
        //  the tip's fill, from its highlight column on, so the row and the
        //  tip read as one block.
        bool                                          isLit      = r == m_list.m_highlightRow;
        DxuiRowLook                                   look       = DxuiRowLook::Resolve (*m_list.m_theme, isSel, r == m_list.m_selectedRow && m_list.m_focusGroup < 0, isHov && !isLit, m_list.m_listFocused);
        size_t                                        lastColumn = 0;
        float                                         rowInsetY  = boxInsetY;
        CellSpot                                      place;

        //  In a selection of several rows, the row the keyboard is on has the
        //  strong outline, as a focus rectangle marks it, and the others have
        //  the plain selection's.
        if (isSel && m_list.m_listFocused && m_list.m_multiSelect && m_list.m_selectedRows.size() > 1)
        {
            look.edge = (r == m_list.m_selectedRow) ? m_list.m_theme->ContentSelectionMultiEdge() : m_list.m_theme->ContentSelectionEdge();
        }

        //  A text-like list (a hex dump) selects as text does: the text
        //  selection color, no outline.
        if (m_list.m_textSelectionColors)
        {
            look.fill = isSel ? pal.bgSel : ((isHov && !isLit) ? pal.bgHover : 0u);
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

        //  A check box is nearly as tall as the row, so the fill of a row with
        //  one is the row's whole height; inset, the box was taller than the fill.
        rowInsetY = std::ranges::any_of (cells, [] (const DxuiListViewTypes::Cell & cell) { return cell.check != DxuiListViewTypes::Cell::Check::None; }) ? 0.0f : boxInsetY;

        if (look.fill != 0)
        {
            painter.FillRect (x + boxL, ry + rowInsetY, boxW - boxL, rowH - rowInsetY * 2.0f, look.fill);
        }

        if (look.edge != 0)
        {
            painter.OutlineRect (x + boxL, ry + rowInsetY, boxW - boxL, rowH - rowInsetY * 2.0f, DxuiRowLook::GetOutlinePx (m_list.m_scaler.ToPxf (1.0f)), look.edge);
        }

        //  The last column drawn is filled to the row's end, so a filled row
        //  runs to the list's edge whatever its columns add up to.
        for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); ++c)
        {
            lastColumn = (m_list.m_columns[c].visible && colWPx[c] > 0) ? c : lastColumn;
        }

        place.rowLeft = x + colOff;
        place.rowEnd  = x + layoutW;
        place.top     = ry;
        place.height  = rowH;
        place.rowFill = DxuiColor::Composite (look.fill, pal.bgRow);
        place.isLit   = isLit;

        for (size_t c = 0; c < m_list.m_columns.size() && c < cells.size(); ++c)
        {
            if (!m_list.m_columns[c].visible || colWPx[c] <= 0)
            {
                continue;
            }

            place.left    = x + colOff + (float) colXPx[c];
            place.width   = (float) colWPx[c];
            place.isFirst = colXPx[c] == firstX;
            place.isLast  = c == lastColumn;
            place.isLit   = isLit && c >= m_list.m_highlightColumn;

            PaintCell (painter, text, pal, cells[c], r, c, place);

            //  A spanning cell runs to the row's end and ends the row.
            if (cells[c].spansRow)
            {
                break;
            }
        }
    }

    if (clipped)
    {
        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
        painter.PopClip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintCell
//
//  A cell's fill, its check box, icon and swatch, any selected characters,
//  background ranges and match bands behind its text, and the text: in one
//  run, or a run for each change of color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::PaintCell (
    IDxuiPainter                     & painter,
    IDxuiTextRenderer                & text,
    const DxuiListViewTypes::Palette & pal,
    const DxuiListViewTypes::Cell    & cell,
    int                                row,
    size_t                             c,
    const CellSpot                   & place) const
{
    HRESULT                                      hr        = S_OK;
    const DxuiDpiScaler                        & scaler    = m_list.m_scaler;
    float                                        fontPx    = scaler.ToPxf (m_list.m_fontDip);
    float                                        padL      = (float) m_list.m_columnSizing.GetCellPadLeftPx (c);
    float                                        padR      = (float) m_list.m_columnSizing.GetCellPadRightPx();
    float                                        trailPx   = scaler.ToPxf ((float) cell.trailDip);
    float                                        cellW     = cell.spansRow ? (std::max) (place.width, place.rowEnd - place.left) : place.width;
    float                                        iconShift = 0.0f;
    float                                        alpha     = text.GetGlobalAlpha();
    bool                                         isLeft    = m_list.m_columns[c].align == DxuiTextHAlign::Left;
    bool                                         isPaler   = m_list.m_explorerDetails && c > 0 && !cell.dim && cell.argb == 0;
    uint32_t                                     argb      = (cell.argb != 0) ? cell.argb : (cell.dim ? pal.fgDim : pal.fg);
    uint32_t                                     under     = place.isLit ? DxuiColor::Composite (pal.bgSel, pal.bgRow)
                                                           : (cell.background != 0) ? DxuiColor::Composite (cell.background, place.rowFill) : place.rowFill;
    const wchar_t                              * face      = (cell.face != nullptr) ? cell.face : m_list.GetBodyFace();
    float                                        textX     = 0.0f;
    float                                        textW     = 0.0f;
    std::vector<std::tuple<int, int, uint32_t>>  bands;



    //  Whatever the row is filled with, the text reads on it. Explorer draws
    //  the columns after the name a step dimmer than it.
    argb = isPaler ? (pal.fg & 0x00FFFFFFu) | s_kSecondaryColumnAlpha : GetReadableInk (argb, under);

    //  The first column's fill runs back over the pane lead to the list's
    //  left, so a colored cell still meets the pane's edge.
    if (place.isLit)
    {
        painter.FillRect (place.left, place.top, place.isLast ? (std::max) (cellW, place.rowEnd - place.left) : cellW, place.height, pal.bgSel);
    }
    else if (cell.background != 0)
    {
        float  fillX = place.isFirst ? place.rowLeft : place.left;
        float  fillW = cellW + place.left - fillX;

        painter.FillRect (fillX, place.top, place.isLast ? (std::max) (fillW, place.rowEnd - fillX) : fillW, place.height, cell.background);
    }

    if (m_list.HasCheckBoxSpace (cell, c))
    {
        if (cell.check != DxuiListViewTypes::Cell::Check::None)
        {
            PaintCheckBox (painter, text, cell.check, place.left + GetCheckBoxLeftPx (c, padL), place.top, place.height);
        }

        padL += (float) m_list.GetCheckBoxSpanPx();
    }

    if (cell.icon && !cell.icon->bgraPremul.empty())
    {
        float  iconPx = scaler.ToPxf ((float) DxuiListView::s_kCellIconDip);
        float  iconW  = iconPx * (float) cell.icon->width / (float) (std::max) (1, cell.icon->height);
        float  iconX  = place.left + padL;
        float  iconY  = place.top + (place.height - iconPx) * 0.5f;

        //  A glyph margin centers its icon's first square on a set line; a
        //  wider icon runs on to the right.
        if (c == 0 && m_list.m_glyphCenterDip > 0.0f)
        {
            iconX = place.rowLeft + scaler.ToPxf (m_list.m_glyphCenterDip) - iconPx * 0.5f;
        }

        if (cell.iconGhosted)
        {
            text.SetGlobalAlpha (alpha * DxuiListIcons::s_kGhostedIconAlpha);
        }

        hr = text.DrawIconBitmap (cell.icon->bgraPremul.data(), cell.icon->width, cell.icon->height, iconX, iconY, iconW, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);

        text.SetGlobalAlpha (alpha);

        if (cell.iconBroken)
        {
            DxuiListIcons::PaintBrokenBadge (text, iconX, iconY, iconPx, scaler.ToPxf (DxuiListView::s_kBrokenBadgeMinDip));
        }

        iconShift = iconPx + (float) m_list.GetCellIconGapPx();
    }
    else if ((int) c == m_list.m_iconColumn)
    {
        //  Its name still lines up with the others' names.
        iconShift = (float) (scaler.ToPx (DxuiListView::s_kCellIconDip) + m_list.GetCellIconGapPx());
    }

    if (cell.swatch != 0)
    {
        float  swatchPx = scaler.ToPxf ((float) DxuiListColumns::s_kCellSwatchDip);

        painter.FillRect (place.left + padL + iconShift, place.top + std::round ((place.height - swatchPx) * 0.5f), swatchPx, swatchPx, cell.swatch);

        iconShift += swatchPx + scaler.ToPxf ((float) DxuiListColumns::s_kCellIconGapDip);
    }

    textX = place.left + padL + iconShift;
    textW = cellW - padL - padR - iconShift - trailPx;

    //  Selected characters, as a text box draws them: the selection color
    //  behind the text.
    if (int s = 0, e = 0; isLeft && m_list.m_textSelect.GetCellSelection (row, c, (int) cell.text.size(), s, e))
    {
        FillTextRange (painter, text, cell.text, face, textX, textW, s, e, place.top, place.height, 0.0f, pal.textSel, 0);
    }

    // Background ranges: a fill behind each range of characters, the row's
    // height tall.
    if (isLeft)
    {
        for (const auto & [first, last, rangeArgb] : cell.backgroundRanges)
        {
            if ((rangeArgb >> 24) != 0)
            {
                FillTextRange (painter, text, cell.text, face, textX, textW, first, last, place.top, place.height, 0.0f, rangeArgb, 0);
            }
        }
    }

    // Search-match highlight: a band behind each matched character range,
    // painted before the text: the matches set on the cell, and the find's, the
    // selected one on a separate fill. A right-aligned or centered cell's
    // text starts after the space its width spares.
    bands = m_list.m_find.GetMatchBands (row, c, cell, pal.matchBg, under);

    if (!bands.empty())
    {
        float  bandX = textX;
        float  fullW = 0.0f;
        float  fullH = 0.0f;

        if (!isLeft)
        {
            hr = text.MeasureString (cell.text.c_str(), fontPx, face, fullW, fullH);
            IGNORE_RETURN_VALUE (hr, S_OK);

            bandX += (m_list.m_columns[c].align == DxuiTextHAlign::Right) ? (std::max) (0.0f, textW - fullW) : (std::max) (0.0f, (textW - fullW) * 0.5f);
        }

        for (const auto & [first, last, bandArgb] : bands)
        {
            FillTextRange (painter, text, cell.text, face, bandX, textW - (bandX - textX), first, last, place.top, place.height,
                           place.height * 0.14f, bandArgb, (bandArgb == pal.matchBg) ? pal.matchEdge : 0);
        }
    }

    // Muted ranges, colored ranges and matches: the text is drawn a run at a
    // time, each run placed by measuring the text before it.
    if ((!cell.dimRanges.empty() || !cell.colorRanges.empty() || !cell.backgroundRanges.empty() || !bands.empty()) && isLeft)
    {
        PaintTextRuns (text, pal, cell, face, argb, under, bands, textX, textW, place.top, place.height);
        return;
    }

    //  Text wider than its column ends in an ellipsis, as Explorer's does,
    //  rather than being cut through a letter. A fixed-width list, a hex dump
    //  or a disassembly, is cut at the column's edge as a text editor's is,
    //  and its cells are not measured on every paint.
    hr = text.DrawString (m_list.m_monospace ? cell.text.c_str() : DxuiTextElide::ToWidth (text, cell.text, fontPx, face, textW, DxuiElide::Tail).c_str(),
                          textX,
                          place.top,
                          textW,
                          place.height,
                          argb,
                          fontPx,
                          face,
                          m_list.m_columns[c].align,
                          DxuiTextVAlign::CenterOnCapHeight,
                          cell.weight,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillTextRange
//
//  A fill behind the characters [first, last) of a cell's text, placed by
//  measuring the text before them and cut at the cell's text width, inset
//  from the row's top and bottom, and outlined when an edge color is given.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::FillTextRange (
    IDxuiPainter         & painter,
    IDxuiTextRenderer    & text,
    const std::wstring   & cellText,
    const wchar_t        * face,
    float                  textX,
    float                  textW,
    int                    first,
    int                    last,
    float                  top,
    float                  height,
    float                  insetY,
    uint32_t               fill,
    uint32_t               edge) const
{
    HRESULT  hr      = S_OK;
    float    fontPx  = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    int      s       = std::clamp (first, 0, (int) cellText.size());
    int      e       = std::clamp (last,  s, (int) cellText.size());
    float    wS      = 0.0f;
    float    wE      = 0.0f;
    float    hIgnore = 0.0f;
    float    hx      = 0.0f;
    float    hw      = 0.0f;



    if (s > 0)
    {
        hr = text.MeasureString (cellText.substr (0, (size_t) s).c_str(), fontPx, face, wS, hIgnore);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.MeasureString (cellText.substr (0, (size_t) e).c_str(), fontPx, face, wE, hIgnore);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hx = textX + wS;
    hw = (std::min) (wE - wS, textX + textW - hx);

    if (hw <= 0.0f)
    {
        return;
    }

    painter.FillRect (hx, top + insetY, hw, height - 2.0f * insetY, fill);

    if (edge != 0)
    {
        painter.OutlineRect (hx, top + insetY, hw, height - 2.0f * insetY, 1.0f, edge);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintTextRuns
//
//  A cell's text in runs of one color: the cell's, a colored range's, the
//  muted color over a muted range, each made readable on the fill under it,
//  and over a match band an ink that reads on the band.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListRows::PaintTextRuns (
    IDxuiTextRenderer                                  & text,
    const DxuiListViewTypes::Palette                   & pal,
    const DxuiListViewTypes::Cell                      & cell,
    const wchar_t                                      * face,
    uint32_t                                             argb,
    uint32_t                                             under,
    const std::vector<std::tuple<int, int, uint32_t>>  & bands,
    float                                                textX,
    float                                                textW,
    float                                                top,
    float                                                height) const
{
    HRESULT                hr     = S_OK;
    float                  fontPx = m_list.m_scaler.ToPxf (m_list.m_fontDip);
    const std::wstring   & s      = cell.text;
    int                    pos    = 0;
    std::vector<uint32_t>  colors (s.size(), argb);
    std::vector<uint32_t>  fills  = DxuiListFind::GetCharacterFills (cell, under);



    for (const auto & [first, last, rangeArgb] : cell.colorRanges)
    {
        for (int k = (std::max) (0, first); k < last && k < (int) colors.size(); k++)
        {
            colors[(size_t) k] = rangeArgb;
        }
    }

    for (const std::pair<int, int> & range : cell.dimRanges)
    {
        for (int k = (std::max) (0, range.first); k < range.second && k < (int) colors.size(); k++)
        {
            colors[(size_t) k] = pal.fgDim;
        }
    }

    for (size_t k = 0; k < colors.size(); k++)
    {
        colors[k] = GetReadableInk (colors[k], fills[k]);
    }

    for (const auto & [first, last, bandArgb] : bands)
    {
        for (int k = (std::max) (0, first); k < last && k < (int) colors.size(); k++)
        {
            colors[(size_t) k] = DxuiColor::ChooseInkFor (bandArgb, 0xFF101010u, 0xFFF8F8F8u);
        }
    }

    while (pos < (int) s.size())
    {
        int    end     = pos + 1;
        float  offset  = 0.0f;
        float  hIgnore = 0.0f;

        while (end < (int) s.size() && colors[(size_t) end] == colors[(size_t) pos])
        {
            end++;
        }

        if (pos > 0)
        {
            hr = text.MeasureString (s.substr (0, (size_t) pos).c_str(), fontPx, face, offset, hIgnore);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (offset >= textW)
        {
            break;
        }

        hr = text.DrawString (s.substr ((size_t) pos, (size_t) (end - pos)).c_str(),
                              textX + offset,
                              top,
                              textW - offset,
                              height,
                              colors[(size_t) pos],
                              fontPx,
                              face,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::CenterOnCapHeight,
                              (cell.weight == DxuiFontWeight::Italic) ? DxuiFontWeight::Italic : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        pos = end;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReadableInk
//
//  An ink moved, as little as it takes, to read on a fill; the results are
//  cached by ink and fill, since a list draws the same few pairs on every
//  paint.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiListRows::GetReadableInk (uint32_t ink, uint32_t fill) const
{
    constexpr float  kTextContrast = 4.5f;   // WCAG AA for text
    uint64_t         key           = ((uint64_t) ink << 32) | fill;
    auto             found         = m_readableInks.find (key);



    if (found != m_readableInks.end())
    {
        return found->second;
    }

    return m_readableInks[key] = DxuiColor::ComputeInkForContrast (ink, fill, kTextContrast);
}
