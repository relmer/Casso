#pragma once

#include "Pch.h"
#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListRows
//
//  The data rows: pressing, dragging and activating them in every view, and
//  drawing them in Details.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListRows
{
public:
    explicit DxuiListRows (DxuiListView & list);

    //  A row's highlight ends this far short of the last column's right edge,
    //  and the columns after the name are drawn at this strength, as
    //  Explorer's are against its full white.
    static constexpr int       s_kRowBoxEndInsetDip    = 4;
    static constexpr uint32_t  s_kSecondaryColumnAlpha = 0xCC000000u;

    //  A wrapped column's narrowest width before the list scrolls sideways.
    static constexpr int       s_kWrapMinWidthDip      = 160;

    //  A press on a row, the drag that extends the selection from it, and the
    //  release that activates it; EndPress ends the press before the release
    //  goes anywhere else.
    bool  OnPress   (const DxuiMouseEvent & ev, int row);
    bool  OnDrag    (int ly);
    bool  EndPress  ();
    bool  OnRelease (int row);

    //  A press on a cell's check box, in a list with check boxes.
    bool  OnCheckPress (int lx, int row);

    //  Ends a drag that is extending the selection, keeping what it selected.
    void  EndDragSelect            () { m_dragSelecting = false; }
    bool  IsDragSelecting          () const { return m_dragSelecting; }
    void  ClearRowPress            () { m_rowPressed = false; }
    void  SetActivateOnDoubleClick (bool enabled) { m_activateOnDoubleClick = enabled; }

    //  Word wrap in a stretch column: a pushed Details list with no groups,
    //  whose rows grow by the lines their wrapped cells take. While a list
    //  wraps, its rows are not lifted at the bottom and a page doesn't slide.
    void  SetWrapsText             (bool enabled) { m_wrapsText = enabled; m_wrapWidthPx = -1; }
    void  InvalidateRowHeights     () { m_wrapWidthPx = -1; }
    bool  IsWrapped                () const;
    bool  IsWrappedCell            (size_t column, const DxuiListViewTypes::Cell & cell) const;
    int   GetRowHeightPx           (int row) const;
    int   GetRowsHeightPx          (int first, int end) const;
    int   CountRowsFitting         (int topRow, int bodyH) const;
    int   GetTopRowEndingAt        (int lastRow, int bodyH) const;
    int   FindVisibleRow           (int bodyY) const;

    //  The scroll layout for a paint, with each row's height measured first
    //  while the list wraps.
    DxuiListViewTypes::ScrollLayout  MeasureForPaint (IDxuiTextRenderer & text) const;

    //  Whether the last paint cut the cell at a row and column short.
    bool  IsCellClipped            (int row, size_t column) const { return std::find (m_clipped.begin(), m_clipped.end(), std::pair<int, size_t> { row, column }) != m_clipped.end(); }

    void  Paint (IDxuiPainter                     & painter,
                 IDxuiTextRenderer                & text,
                 const DxuiListViewTypes::Palette & pal,
                 float                              x,
                 float                              y,
                 float                              layoutW,
                 int                                firstRow,
                 int                                lastRow,
                 const std::vector<int>           & colXPx,
                 const std::vector<int>           & colWPx) const;

    //  An item check box, centered in the height from rowTop, at left.
    void  PaintCheckBox (IDxuiPainter                     & painter,
                         IDxuiTextRenderer                & text,
                         DxuiListViewTypes::Cell::Check     check,
                         float                              left,
                         float                              rowTop,
                         float                              rowH,
                         bool                               overText = false) const;

private:
    //  Extends the selection from the row the drag began on to the row under
    //  the pointer.
    void  DragSelectTo (int ly);

    //  Where a column's check box starts in its cell.
    float GetCheckBoxLeftPx (size_t column, float padL) const;

    //  Each row's height at the wrapped column's width, measured again only
    //  when the rows or that width change.
    void  MeasureRowHeights (IDxuiTextRenderer & text, int viewportW) const;

    DxuiListView  & m_list;

    bool     m_activateOnDoubleClick = false;
    bool     m_wrapsText             = false;
    bool     m_dragSelecting         = false;
    bool     m_pressModified         = false;   // Ctrl or Shift was down for the row press
    bool     m_rowPressed            = false;   // set by a press on a row in the list; a release without one is ignored
    int      m_lastClickRow          = -1;
    int64_t  m_lastClickMs           = 0;

    //  Each row's height from the last paint while wrapping, the wrapped
    //  column's width it was measured at (-1 when the rows changed), one
    //  text line's height, and the cells the last paint cut short.
    mutable std::vector<int>                     m_rowHeightsPx;
    mutable int                                  m_wrapWidthPx = -1;
    mutable float                                m_wrapLinePx  = 0.0f;
    mutable std::vector<std::pair<int, size_t>>  m_clipped;
};
