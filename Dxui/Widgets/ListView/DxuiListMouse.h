#pragma once

#include "Pch.h"

class DxuiListView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListMouse
//
//  Pointer input: a press, a move, a release and the wheel routed to the
//  header, a scrollbar, a group or a row, and a drag that selects rows.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListMouse
{
public:
    explicit DxuiListMouse (DxuiListView & list);

    static constexpr int    s_kHeaderDragDip         = 5;   // how far a header moves before it is a drag

    // Mouse-event dispatch helpers (lx / ly are widget-relative px).
    bool    DispatchMouseDown      (const DxuiMouseEvent & ev, int lx, int ly, bool inside);
    bool    DispatchMouseMove      (int lx, int ly, bool inside);
    void    DragSelectTo           (int ly);
    bool    DispatchMouseUp        (int lx, int ly, bool inside);
    bool    DispatchMouseWheel     (const DxuiMouseEvent & ev, bool inside);

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    int      m_anchorRow       = -1;
    bool     m_pressModified   = false;   // the row press held Ctrl or Shift
    int      m_lastHeaderGroup = -1;
    int64_t  m_lastHeaderMs    = 0;
    int      m_resizeColumn    = -1;
    int      m_resizeStartXPx  = 0;
    int      m_resizeStartWPx  = 0;

    bool     m_activateOnDoubleClick = false;
    bool     m_dragSelecting         = false;
    int      m_lastClickRow          = -1;
    int64_t  m_lastClickMs           = 0;
    int      m_lastDividerCol        = -1;
    int64_t  m_lastDividerMs         = 0;
};
