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

    //  A press on a row, the drag that extends the selection from it, and the
    //  release that activates it; EndPress ends the press before the release
    //  goes anywhere else.
    bool  OnPress   (const DxuiMouseEvent & ev, int row);
    bool  OnDrag    (int ly);
    bool  EndPress  ();
    bool  OnRelease (int row);

    //  Ends a drag that is extending the selection, keeping what it selected.
    void  EndDragSelect            () { m_dragSelecting = false; }
    bool  IsDragSelecting          () const { return m_dragSelecting; }
    void  SetActivateOnDoubleClick (bool enabled) { m_activateOnDoubleClick = enabled; }

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

private:
    //  Extends the selection from the row the drag began on to the row under
    //  the pointer.
    void  DragSelectTo (int ly);

    DxuiListView  & m_list;

    bool     m_activateOnDoubleClick = false;
    bool     m_dragSelecting         = false;
    bool     m_pressModified         = false;   // Ctrl or Shift was down for the row press
    int      m_lastClickRow          = -1;
    int64_t  m_lastClickMs           = 0;
};
