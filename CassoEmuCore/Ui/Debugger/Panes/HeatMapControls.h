#pragma once

#include "Ui/Debugger/Panes/HeatMapZoomWidget.h"

class HeatMapGrid;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls
//
//  The heat map's scrollbar down the right of the map and the zoom widget in
//  its bottom-right corner. Both are placed from the grid: the scrollbar each
//  time the map is scrolled or laid out, and the zoom widget each time the
//  map is laid out.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapControls
{
public:
    void   SyncScrollbar   (const HeatMapGrid & grid, long contentHeight);
    void   PlaceZoomWidget (const HeatMapGrid & grid);

    //  One frame of the widening of a scrollbar under the mouse and of the
    //  zoom slider's fade; true while either is moving.
    bool   Tick            (int64_t nowMs);

    //  Over the zoom's button, how to zoom.
    bool   TryGetZoomTipAt (POINT point, bool isVisible, RECT & anchor, std::wstring & text) const;

    DxuiScrollbar            & GetScrollbar  ()       { return m_vertBar; }
    const DxuiScrollbar      & GetScrollbar  () const { return m_vertBar; }
    HeatMapZoomWidget        & GetZoomWidget ()       { return m_zoomWidget; }
    const HeatMapZoomWidget  & GetZoomWidget () const { return m_zoomWidget; }

private:
    static RECT  GetZoomCorner (const HeatMapGrid & grid);

    DxuiScrollbar        m_vertBar;
    HeatMapZoomWidget    m_zoomWidget;
    RECT                 m_zoomPlacedFor = {};   // the bounds the zoom widget was placed in
};
