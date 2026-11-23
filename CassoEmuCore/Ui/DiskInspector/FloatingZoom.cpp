#include "Pch.h"

#include "Ui/DiskInspector/FloatingZoom.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoom::FloatingZoom
//
////////////////////////////////////////////////////////////////////////////////

FloatingZoom::FloatingZoom()
{
    m_control = CreateChild<DxuiZoomControl>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoom::Place
//
////////////////////////////////////////////////////////////////////////////////

void FloatingZoom::Place (POINT bottomRightPx, int heightPx, int padPx, const DxuiDpiScaler & scaler)
{
    int  width = m_control->GetShownWidthPx (scaler) + 2 * padPx;
    int  right = bottomRightPx.x;
    int  top   = bottomRightPx.y - heightPx;



    m_scaler = scaler;
    m_corner = bottomRightPx;
    m_height = heightPx;
    m_pad    = padPx;

    Layout             ({ right - width, top, right, bottomRightPx.y }, scaler);
    m_control->Layout  ({ right - width + padPx, top + padPx, right - padPx, bottomRightPx.y - padPx }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoom::Update
//
//  The slider goes by where the cursor is, not the last mouse event, as the
//  status bar's does, so it stays through a tooltip that ends the window's
//  hover. The percentage is the map's zoom except while the slider is being
//  dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingZoom::Update (HWND owner, double zoom, double mostZoom)
{
    POINT  cursor    = {};
    bool   isCursor  = false;
    bool   isChanged = false;



    if (mostZoom != m_mostZoom)
    {
        m_mostZoom = mostZoom;
        m_control->SetRange (1.0, std::max (mostZoom, 1.0), true);
    }

    if (!m_control->IsDragging())
    {
        m_control->SetZoom (zoom);
    }

    isCursor  = m_scaler.GetDpi() != 0 && owner != nullptr && IsVisible() && GetCursorPos (&cursor) && ScreenToClient (owner, &cursor);
    isChanged = isCursor && m_control->UpdateHover (cursor);

    if (isChanged)
    {
        Place (m_corner, m_height, m_pad, m_scaler);
    }

    return isChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingZoom::GetClosedWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int FloatingZoom::GetClosedWidthPx (int padPx, const DxuiDpiScaler & scaler)
{
    return DxuiZoomControl::GetPercentWidthPx (scaler) + 2 * padPx;
}
