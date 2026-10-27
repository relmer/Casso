#include "Pch.h"

#include "Widgets/ListView/DxuiListMouse.h"
#include "Core/DxuiClipboard.h"
#include "Theme/DxuiTheme.h"
#include "Core/DxuiTextElide.h"
#include "Theme/DxuiRowLook.h"
#include "Theme/DxuiColor.h"
#include "Widgets/DxuiListView.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiSystemSettings.h"
#include "Widgets/DxuiListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse::DxuiListMouse
//
////////////////////////////////////////////////////////////////////////////////

DxuiListMouse::DxuiListMouse (DxuiListView & list)
    : m_list (list)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse::DispatchMouseDown
//
//  Left-press priority chain: the scrollbars, then a column-resize
//  divider, then a header (sort), then a data row (select). A left-press
//  anywhere inside the list is consumed so the host can claim focus.
//  (Right-click stays with the host: its menu content is host-specific.)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListMouse::DispatchMouseDown (const DxuiMouseEvent & ev, int lx, int ly, bool inside)
{
    HRESULT  hr        = S_OK;
    bool     handled   = false;
    int      grabTol   = m_list.m_scaler.ToPx (DxuiListView::s_kResizeGrabDip);
    int      resizeCol = -1;
    int      headerCol = -1;
    int      row       = -1;
    int      group     = -1;



    BAIL_OUT_IF (ev.button != DxuiMouseButton::Left || !inside, S_OK);

    handled = m_list.m_scrollbars.DispatchScrollbarPress (lx, ly);
    BAIL_OUT_IF (handled, S_OK);

    resizeCol = m_list.m_header.HitTestColumnResize (lx, ly, grabTol);

    //  A second press on the same divider inside the double-click time fits
    //  the column to its content, as it does in Explorer, instead of starting
    //  another drag. The pair is consumed so a third press counts afresh.
    if (resizeCol >= 0)
    {
        int64_t  nowMs = (int64_t) GetTickCount64();
        bool     fit   = (resizeCol == m_lastDividerCol)
                         && (nowMs - m_lastDividerMs) <= (int64_t) GetDoubleClickTime();

        m_lastDividerCol = fit ? -1 : resizeCol;
        m_lastDividerMs  = nowMs;

        if (fit)
        {
            m_list.m_columnSizing.FitColumnToContent ((size_t) resizeCol);
            handled = true;
            BAIL_OUT_IF (true, S_OK);
        }
    }

    if (resizeCol >= 0)
    {
        m_resizeColumn   = resizeCol;
        m_resizeStartXPx = lx;
        m_resizeStartWPx = m_list.m_columnSizing.GetColumnEffectiveWidthPx ((size_t) resizeCol);
        handled          = true;
        BAIL_OUT_IF (true, S_OK);
    }

    headerCol = m_list.m_header.HitTestHeaderColumn (lx, ly);

    //  Sorted on the release rather than here, since the press may instead
    //  start a drag that moves the column.
    if (headerCol >= 0)
    {
        m_list.m_headerPressCol = headerCol;
        m_list.m_headerPressXPx = lx;
        m_list.m_headerDragXPx  = lx;
        m_list.m_headerDragging = false;
        handled                 = true;
        BAIL_OUT_IF (true, S_OK);
    }

    row   = m_list.HitTestRow (lx, ly);
    group = m_list.m_grouping.HitTestGroupHeader (lx, ly);

    if (group >= 0)
    {
        int64_t  nowMs  = (int64_t) GetTickCount64();
        bool     twice  = group == m_lastHeaderGroup && (nowMs - m_lastHeaderMs) <= (int64_t) GetDoubleClickTime();

        //  The chevron, or a second click on the header, opens or closes the
        //  group; a click on the header otherwise selects its rows.
        m_lastHeaderGroup = twice ? -1 : group;
        m_lastHeaderMs    = nowMs;

        //  List's headers are labels only, as Explorer's are: they select.
        if (!m_list.m_flow.HasListGroups() && (twice || m_list.m_grouping.HitTestGroupChevron (lx, group)))
        {
            m_list.m_grouping.SetGroupCollapsed (group, !m_list.m_grouping.IsGroupCollapsed (group));
            m_list.m_focusGroup = group;
        }
        else
        {
            m_list.m_grouping.SelectGroup (group);
        }
    }
    else if (row >= 0 && !ev.ctrl && !ev.shift && m_list.m_selectedRows.size() > 1 && m_list.IsRowSelected (row))
    {
        //  A plain press on one of several selected rows leaves the selection
        //  whole, so a drag that follows takes all of it, as Explorer's does.
        //  The release collapses it to the row if no drag began.
        m_list.m_focusGroup  = -1;
        m_pressModified      = false;
        m_dragSelecting      = false;
        m_anchorRow          = row;
        m_list.m_selectedRow = row;
    }
    else if (row >= 0)
    {
        m_list.m_focusGroup = -1;
        m_pressModified     = ev.ctrl || ev.shift;
        m_list.ClickRow (row, ev.ctrl, ev.shift);
        m_dragSelecting = m_list.m_multiSelect;
    }
    else if (m_list.m_multiSelect && (m_list.IsItemsView() || ly >= m_list.GetHeaderHeightPx() + (m_list.m_showHeader ? m_list.GetHeaderGapPx() : 0)))
    {
        //  A press on empty space starts a rubber band, as in Explorer: between
        //  items, below the last row, or past the last column.
        m_list.m_items.BeginSelectionBand (lx, ly, ev.ctrl);
    }
    else if (!ev.ctrl && !ev.shift && !m_list.m_selectedRows.empty())
    {
        //  A plain click on the empty space below the rows selects nothing.
        m_list.ClearSelection();
    }

    handled = true;

Error:
    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse::DispatchMouseMove
//
//  Drives an in-progress column-resize or thumb drag; otherwise tracks
//  the hovered row while the pointer is inside. Returns true while
//  dragging or hovering inside the list.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListMouse::DispatchMouseMove (int lx, int ly, bool inside)
{
    bool  handled = true;
    int   minColW = m_list.m_scaler.ToPx (DxuiListView::s_kMinColWidthDip);
    int   newColW = 0;



    if (m_resizeColumn >= 0)
    {
        newColW = std::max (minColW, m_resizeStartWPx + (lx - m_resizeStartXPx));
        m_list.m_columnSizing.SetColumnOverrideWidthPx ((size_t) m_resizeColumn, newColW);
    }
    else if (m_list.m_headerPressCol >= 0)
    {
        m_list.m_headerDragXPx  = lx;
        m_list.m_headerDragging = m_list.m_headerDragging || std::abs (lx - m_list.m_headerPressXPx) >= m_list.m_scaler.ToPx (s_kHeaderDragDip);

        if (m_list.m_headerDragging)
        {
            m_list.m_header.UpdateHeaderSlides();
        }
    }
    else if (m_list.m_vertDragging)
    {
        m_list.m_scrollbars.UpdateThumbDrag (ly);
    }
    else if (m_list.m_horzDragging)
    {
        m_list.m_scrollbars.UpdateHorzThumbDrag (lx);
    }
    else if (m_list.m_bandActive)
    {
        m_list.m_items.UpdateSelectionBand (lx, ly);
    }
    else if (m_dragSelecting)
    {
        DragSelectTo (ly);
    }
    else if (inside)
    {
        m_list.SetHoveredRow (m_list.HitTestRow (lx, ly));
        m_list.m_hoverGroup = m_list.m_grouping.HitTestGroupHeader (lx, ly);
    }
    else
    {
        m_list.SetHoveredRow (-1);
        m_list.m_hoverGroup = -1;
        handled             = false;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse::DragSelectTo
//
//  Extends the selection from the row the drag began on to the row under the
//  pointer, scrolling one row at a time when the pointer is past either edge.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiListMouse::DragSelectTo (int ly)
{
    int  rowH    = m_list.GetRowHeightPx();
    int  headerH = m_list.m_showHeader ? m_list.GetHeaderBarPx() : 0;
    int  hdrGap  = m_list.m_showHeader ? m_list.GetHeaderGapPx()    : 0;
    int  body    = ly - headerH - hdrGap;
    int  row     = 0;
    int  line    = 0;



    //  A drag in an item view selects nothing more than the press did: rows
    //  of items have no single top-to-bottom order to extend along.
    if (rowH <= 0 || m_anchorRow < 0 || m_list.GetRowCount() == 0 || m_list.IsItemsView())
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
//  DxuiListMouse::DispatchMouseUp
//
//  Ends an in-progress column-resize or thumb drag; otherwise a release
//  over a data row selects it and raises onActivateRow. Returns true
//  when a drag ended or a row was activated.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListMouse::DispatchMouseUp (int lx, int ly, bool inside)
{
    bool  handled = true;
    int   row     = inside ? m_list.HitTestRow (lx, ly) : -1;
    bool  ranged  = m_dragSelecting && (m_list.m_selectedRows.size() > 1 || m_pressModified);



    //  A drag, or a Shift or Ctrl click, keeps what it left selected, even a
    //  single row; only a plain click collapses the selection to its row.
    m_dragSelecting = false;
    m_pressModified = false;

    if (m_list.m_bandActive)
    {
        m_list.m_bandActive = false;
        m_list.m_bandBase.clear();
        handled = true;
    }
    else if (ranged)
    {
        handled = true;
    }
    else if (m_list.m_scrollRepeat != DxuiListViewTypes::ScrollRepeat::None)
    {
        m_list.m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
    }
    else if (m_list.m_headerPressCol >= 0)
    {
        m_list.m_header.EndHeaderPress (lx, ly);
    }
    else if (m_resizeColumn >= 0)
    {
        int  resizedCol   = m_resizeColumn;
        int  resizedWidth = m_list.m_columnSizing.GetColumnEffectiveWidthPx ((size_t) resizedCol);

        m_resizeColumn = -1;

        if (m_list.m_onColumnResized)
        {
            m_list.m_onColumnResized (resizedCol, resizedWidth);
        }
    }
    else if (m_list.m_vertDragging)
    {
        m_list.EndThumbDrag();
    }
    else if (m_list.m_horzDragging)
    {
        m_list.EndHorzThumbDrag();
    }
    else if (row >= 0)
    {
        bool  activate  = true;
        bool  collapsed = m_list.m_selectedRows.size() > 1;

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
    }
    else
    {
        handled = false;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse::DispatchMouseWheel
//
//  Scrolls the list when the pointer is inside: Shift+wheel scrolls
//  horizontally, otherwise vertically. ev.wheelDelta is in notches and
//  is converted to the raw WHEEL_DELTA units the scroll helpers expect.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiListMouse::DispatchMouseWheel (const DxuiMouseEvent & ev, bool inside)
{
    HRESULT  hr       = S_OK;
    bool     handled  = false;
    int      hStep    = m_list.m_scaler.ToPx (DxuiListView::s_kHScrollStepDip);
    int      rawDelta = (int) (ev.wheelDelta * (float) WHEEL_DELTA);



    BAIL_OUT_IF (!inside, S_OK);

    if (ev.wheelHorizontal)
    {
        // Trackpad / tilt-wheel horizontal (WM_MOUSEHWHEEL): a positive
        // delta means "scroll right", the opposite of the Shift+wheel
        // convention below (positive = toward the start), so negate.
        m_list.m_scrollbars.ScrollByWheelDeltaHorizontal (-rawDelta, hStep);
    }
    else if (ev.shift)
    {
        m_list.m_scrollbars.ScrollByWheelDeltaHorizontal (rawDelta, hStep);
    }
    else
    {
        m_list.m_scrollbars.ScrollByWheelDelta (rawDelta, m_list.m_scrollbars.GetWheelLinesPerNotch());
    }

    handled = true;

Error:
    return handled;
}
