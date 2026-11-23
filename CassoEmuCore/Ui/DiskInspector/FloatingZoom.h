#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/FloatingGroup.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoom
//
//  A map's zoom in a floating group at the bottom right of the map: the
//  window status bar's zoom control, at rest only the percentage. With the
//  pointer over the percentage, the group grows to the left around the
//  slider and the - and + buttons, and the percentage doesn't move.
//
////////////////////////////////////////////////////////////////////////////////

class FloatingZoom : public FloatingGroup
{
public:
    using ZoomFn = DxuiZoomControl::ZoomFn;
    using StepFn = DxuiZoomControl::StepFn;

    FloatingZoom();

    void  SetOnZoom (ZoomFn fn) { m_control->SetOnZoom (std::move (fn)); }
    void  SetOnStep (StepFn fn) { m_control->SetOnStep (std::move (fn)); }

    //  The group placed with its bottom-right corner at a point, heightPx
    //  tall, with padPx around the control.
    void  Place (POINT bottomRightPx, int heightPx, int padPx, const DxuiDpiScaler & scaler);

    //  Sets the map's zoom and its range, and opens or closes the slider by
    //  where the cursor is. True when the group grew or shrank.
    bool  Update (HWND owner, double zoom, double mostZoom);

    //  The group's width when it is closed, the percentage alone.
    static int  GetClosedWidthPx (int padPx, const DxuiDpiScaler & scaler);

    const DxuiZoomControl & GetControl () const { return *m_control; }

private:
    DxuiZoomControl  * m_control  = nullptr;
    DxuiDpiScaler      m_scaler;
    POINT              m_corner   = {};
    int                m_height   = 0;
    int                m_pad      = 0;
    double             m_mostZoom = 0;
};
