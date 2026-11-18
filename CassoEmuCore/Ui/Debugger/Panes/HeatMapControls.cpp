#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapControls.h"
#include "Ui/Debugger/Panes/HeatMapGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls::SyncScrollbar
//
//  The bar in pixels: the rows' height, the part in view and the scroll.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapControls::SyncScrollbar (const HeatMapGrid & grid, long contentHeight)
{
    int             bar   = grid.GetScaler().ToPx (HeatMapGrid::kScrollbarDip);
    int             pitch = grid.GetPitch();
    RECT            map   = grid.GetMapRect();
    DxuiScrollInfo  info;



    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;

    m_vertBar.Configure (DxuiScrollbar::Orientation::Vertical, bar, bar, pitch);
    m_vertBar.SetTrack  (grid.GetVerticalBarRect());

    info.nMax  = (int) contentHeight;
    info.nPage = (UINT) std::max (0L, map.bottom - map.top);
    info.nPos  = (int) grid.GetScrollTop();
    m_vertBar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls::PlaceZoomWidget
//
//  In the corner, except while its slider is open in a pane the same size:
//  zooming there brings the scrollbar and takes it away, and the slider
//  stays under the pointer rather than moving with the corner.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapControls::PlaceZoomWidget (const HeatMapGrid & grid)
{
    const RECT  & bounds = grid.GetBounds();



    if (m_zoomWidget.IsOpen() && EqualRect (&m_zoomPlacedFor, &bounds))
    {
        return;
    }

    m_zoomPlacedFor = bounds;
    m_zoomWidget.Place (GetZoomCorner (grid), bounds, grid.GetScaler());
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls::Tick
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapControls::Tick (int64_t nowMs)
{
    bool  isBarMoving  = m_vertBar.Tick (nowMs);
    bool  isZoomMoving = m_zoomWidget.Tick (nowMs);



    return isBarMoving || isZoomMoving;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls::TryGetZoomTipAt
//
//  Over the zoom's button, how to zoom: the tip anchored to the button.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapControls::TryGetZoomTipAt (POINT point, bool isVisible, RECT & anchor, std::wstring & text) const
{
    if (!isVisible || m_zoomWidget.HitTest (point) != HeatMapZoomWidget::Hit::Button)
    {
        return false;
    }

    anchor = m_zoomWidget.GetButtonRect();
    text   = HeatMapZoomWidget::kpszTip;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapControls::GetZoomCorner
//
//  The pane's bottom right, inside the scrollbar when it is visible, so the
//  zoom widget is placed its margin in from the corner. The map starts
//  fitted, with no scrollbar, so that is where it usually is.
//
////////////////////////////////////////////////////////////////////////////////

RECT HeatMapControls::GetZoomCorner (const HeatMapGrid & grid)
{
    const RECT           & bounds = grid.GetBounds();
    const DxuiDpiScaler  & scaler = grid.GetScaler();
    long                   right  = bounds.right - (grid.HasVerticalScroll() ? scaler.ToPx (HeatMapGrid::kScrollbarDip) : 0);



    return { bounds.left + grid.GetGutterPx(), bounds.top + scaler.ToPx (HeatMapGrid::kBarDip), right, bounds.bottom };
}
