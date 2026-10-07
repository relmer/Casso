#include "Pch.h"

#include "Window/DxuiToolbarDragSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::BeginDocked
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::BeginDocked (POINT screenPx, const RECT & toolbarPx, bool vertical, UINT dpi)
{
    int  along  = vertical ? screenPx.y - toolbarPx.top  : screenPx.x - toolbarPx.left;
    int  across = vertical ? screenPx.x - toolbarPx.left : screenPx.y - toolbarPx.top;



    m_dpi      = (std::max) (dpi, 1u);
    m_vertical = vertical;
    m_lastPx   = screenPx;
    m_pressPx  = screenPx;
    m_hasMoved = false;
    m_grabDip  = POINT { MulDiv (along, USER_DEFAULT_SCREEN_DPI, (int) m_dpi), MulDiv (across, USER_DEFAULT_SCREEN_DPI, (int) m_dpi) };
    m_phase    = Phase::Docked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::BeginFloating
//
//  The grab counts from the toolbar, which starts its window's resizable
//  end in from the window's own end.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::BeginFloating (POINT screenPx, const RECT & windowPx, bool vertical, UINT dpi)
{
    int  inset  = 0;
    int  along  = 0;
    int  across = 0;



    m_dpi = (std::max) (dpi, 1u);
    inset = m_site.GetFloatInsetPx (m_dpi);

    along  = (vertical ? screenPx.y - windowPx.top  : screenPx.x - windowPx.left) - inset;
    across = vertical ? screenPx.x - windowPx.left : screenPx.y - windowPx.top;

    m_vertical = vertical;
    m_lastPx   = screenPx;
    m_pressPx  = screenPx;
    m_hasMoved = false;
    m_grabDip  = POINT { MulDiv (along, USER_DEFAULT_SCREEN_DPI, (int) m_dpi), MulDiv (across, USER_DEFAULT_SCREEN_DPI, (int) m_dpi) };
    m_phase    = Phase::Floating;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::OnMessage
//
//  A move with the button up, or a frame that finds it up, is a release
//  that never arrived. Losing the mouse while the button is down cancels
//  the drag; losing it with the button up is the window letting go of it
//  on the way to the release, which follows.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDragSession::OnMessage (const Message & msg)
{
    if (m_phase == Phase::Idle)
    {
        return false;
    }

    if (m_phase == Phase::Canceled)
    {
        return OnSwallowed (msg);
    }

    switch (msg.id)
    {
    case WM_MOUSEMOVE:
        if (msg.buttonDown)
        {
            Move (msg.screenPx);
        }
        else
        {
            Release (msg.screenPx);
        }

        return true;

    case WM_LBUTTONUP:
        Release (msg.screenPx);
        return true;

    case WM_TIMER:
        if (!msg.buttonDown)
        {
            Release (m_lastPx);
        }

        return false;

    case WM_CAPTURECHANGED:
        if (msg.buttonDown)
        {
            Cancel (true);
        }

        return false;

    case WM_CANCELMODE:
    case WM_ENTERSIZEMOVE:
        Cancel (msg.buttonDown);
        return false;

    case WM_KEYDOWN:
        if (msg.key != VK_ESCAPE)
        {
            return false;
        }

        Cancel (msg.buttonDown);
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::OnSwallowed
//
//  A canceled drag keeps the rest of its press, so the release does not
//  land as a click on whatever is under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDragSession::OnSwallowed (const Message & msg)
{
    bool  isMouse = msg.id == WM_MOUSEMOVE || msg.id == WM_LBUTTONUP;



    if (msg.id == WM_LBUTTONUP || !msg.buttonDown)
    {
        m_phase = Phase::Idle;
    }

    return isMouse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::Move
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::Move (POINT screenPx)
{
    m_lastPx   = screenPx;
    m_hasMoved = m_hasMoved || screenPx.x != m_pressPx.x || screenPx.y != m_pressPx.y;

    if (m_phase == Phase::Docked)
    {
        if (m_site.IsPulledOut (screenPx))
        {
            TearOff (screenPx);
        }
        else
        {
            m_site.SlideDocked (screenPx);
        }

        return;
    }

    m_vertical = m_site.PickVertical (screenPx, m_vertical);

    PlaceAt     (screenPx);
    ShowPreview (screenPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::TearOff
//
//  The window is made at the docked toolbar's scale, under the pointer. Made
//  on a monitor at another scale it takes that monitor's DPI, so it is
//  placed again at that scale.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::TearOff (POINT screenPx)
{
    UINT  dpi    = m_site.GetFloatDpi();
    bool  isMade = false;



    isMade = m_site.TryTearOff (GetRectAt (screenPx, dpi), m_vertical);

    if (!isMade)
    {
        m_site.SlideDocked (screenPx);
        return;
    }

    m_phase = Phase::Floating;

    if (m_site.GetFloatDpi() != dpi)
    {
        PlaceAt (screenPx);
    }

    ShowPreview (screenPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::PlaceAt
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::PlaceAt (POINT screenPx)
{
    m_site.PlaceFloat (GetRectAt (screenPx, m_site.GetFloatDpi()), m_vertical);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::GetRectAt
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDragSession::GetRectAt (POINT screenPx, UINT dpi) const
{
    UINT  scale = (std::max) (dpi, 1u);



    return GetWindowRect (screenPx, m_grabDip, m_site.GetFloatSizePx (m_vertical, scale), m_site.GetFloatInsetPx (scale), m_vertical, scale);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::ShowPreview
//
//  Where a release here would dock the toolbar, asked of the pointer alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::ShowPreview (POINT screenPx)
{
    DxuiToolbarDock  dock;
    bool             isNewBand = false;
    bool             isNear    = false;



    isNear = m_site.TryPickDrop (screenPx, m_grabDip.x, dock, isNewBand);

    m_site.ShowDropPreview (isNear, dock, isNewBand);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::Release
//
//  The one place a floating toolbar docks: the release, over a band.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::Release (POINT screenPx)
{
    DxuiToolbarDock  dock;
    bool             isNewBand = false;
    bool             isNear    = false;
    Phase            phase     = m_phase;



    m_phase    = Phase::Idle;
    m_lastPx   = screenPx;
    m_hasMoved = m_hasMoved || screenPx.x != m_pressPx.x || screenPx.y != m_pressPx.y;

    if (phase == Phase::Docked)
    {
        m_site.PutDown();
        return;
    }

    //  A click on the handle of a bar left floating over a band is no drop.
    isNear = m_hasMoved && m_site.TryPickDrop (screenPx, m_grabDip.x, dock, isNewBand);

    m_site.ShowDropPreview (false, dock, false);

    if (isNear)
    {
        m_site.DropDocked (dock, isNewBand);
    }
    else
    {
        m_site.LeaveFloating();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarDragSession::Cancel (bool buttonDown)
{
    m_phase = buttonDown ? Phase::Canceled : Phase::Idle;

    m_site.ShowDropPreview (false, DxuiToolbarDock {}, false);
    m_site.Restore();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::TryGetDpiRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarDragSession::TryGetDpiRect (UINT newDpi, RECT & outRectPx) const
{
    if (m_phase != Phase::Floating)
    {
        return false;
    }

    outRectPx = GetRectAt (m_lastPx, newDpi);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSession::GetWindowRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarDragSession::GetWindowRect (POINT pointerPx, POINT grabDip, SIZE sizePx, int insetPx, bool vertical, UINT dpi)
{
    int  scale  = (int) (std::max) (dpi, 1u);
    int  along  = insetPx + MulDiv (grabDip.x, scale, USER_DEFAULT_SCREEN_DPI);
    int  across = MulDiv (grabDip.y, scale, USER_DEFAULT_SCREEN_DPI);
    int  left   = pointerPx.x - (vertical ? across : along);
    int  top    = pointerPx.y - (vertical ? along : across);



    return RECT { left, top, left + sizePx.cx, top + sizePx.cy };
}





