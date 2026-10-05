#include "Pch.h"

#include "Window/DxuiToolbarWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::DxuiToolbarWindow
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarWindow::DxuiToolbarWindow()
{
    SetOnModalLoopTick ([this] { OnMoveLoopTick(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiToolbarWindow::Create (const CreateParams & params)
{
    HRESULT  hr = S_OK;



    hr = DxuiWindow::Create (params);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::Layout
//
//  The toolbar fills the window.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    if (m_toolbar != nullptr)
    {
        m_toolbar->SetHostClientRect (boundsDip);
        m_toolbar->Layout            (boundsDip, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnMouse
//
//  A press on the grab handle moves the window; the rest is the owner's.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarWindow::OnMouse (const DxuiMouseEvent & ev)
{
    bool  onGrip = m_toolbar != nullptr && m_toolbar->IsOnGrip (ev.positionDip.x, ev.positionDip.y);



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && onGrip && !m_toolbar->IsMenuOpen())
    {
        BeginMove();
        return true;
    }

    return m_onMouse ? m_onMouse (ev) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::BeginMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::BeginMove()
{
    m_isMoving = true;

    ReleaseCapture();
    PostMessage (GetHwnd(), WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiToolbarWindow::GetCursorForPoint (POINT clientPx) const
{
    if (m_toolbar != nullptr && m_toolbar->IsOnGrip (clientPx.x, clientPx.y))
    {
        return IDC_SIZEALL;
    }

    return DxuiWindow::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnMoveLoopTick
//
//  A loop the window's ends started is a resize, which is no drag to
//  report; the owner's frames go on either way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::OnMoveLoopTick()
{
    bool  buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    if (m_isMoving)
    {
        Report (m_drag.OnTick (buttonDown, GetScreenSize()));
    }

    if (m_onMoveFrame)
    {
        m_onMoveFrame();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnWindowPlaced
//
//  The move loop ended, which is where a drag ends.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::OnWindowPlaced()
{
    m_isMoving = false;

    Report (m_drag.OnLoopEnd (GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnDpiChanged
//
//  Dragged onto a monitor at another scale, the window takes the size the
//  system gives it there. That is not a resize, so the drag goes on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::OnDpiChanged (UINT newDpi)
{
    UNREFERENCED_PARAMETER (newDpi);

    m_drag.Rebase (GetScreenSize());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::PollCaptionDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::PollCaptionDrag()
{
    bool  buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    //  A move asked for after the button came up never starts its loop.
    m_isMoving = m_isMoving && buttonDown;

    Report (m_drag.OnPoll (buttonDown, GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::Report
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::Report (DxuiCaptionDragTracker::Event ev)
{
    POINT  cursor = {};



    GetCursorPos (&cursor);

    switch (ev)
    {
    case DxuiCaptionDragTracker::Event::Moved:
        if (m_onDrag)
        {
            m_onDrag (cursor);
        }

        break;

    case DxuiCaptionDragTracker::Event::Ended:
        if (m_onDragEnd)
        {
            m_onDragEnd (cursor);
        }

        break;

    case DxuiCaptionDragTracker::Event::Canceled:
        if (m_onDragCancel)
        {
            m_onDragCancel();
        }

        break;

    case DxuiCaptionDragTracker::Event::None:
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetScreenSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiToolbarWindow::GetScreenSize() const
{
    RECT  rect = GetScreenRect();



    return SIZE { rect.right - rect.left, rect.bottom - rect.top };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::SetScreenRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::SetScreenRect (const RECT & rectPx)
{
    SetWindowPos (GetHwnd(), nullptr, rectPx.left, rectPx.top, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetScreenRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarWindow::GetScreenRect() const
{
    RECT  rect = {};



    GetWindowRect (GetHwnd(), &rect);
    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::SetLengthResizable
//
//  The window's own hit test answers for its ends ahead of the usual resize
//  border, so the long sides and the corners are plain client area.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::SetLengthResizable (bool resizable)
{
    DxuiHwndSource  * source = GetPopupHost();



    if (source == nullptr)
    {
        return;
    }

    if (!resizable)
    {
        source->SetHitTestDelegate (nullptr);
        return;
    }

    source->SetHitTestDelegate ([this] (POINT screenPx) { return HitTestLength (screenPx); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::HitTestLength
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiToolbarWindow::HitTestLength (POINT screenPx) const
{
    HRESULT  hr     = S_OK;
    HWND     hwnd   = GetHwnd();
    POINT    client = screenPx;
    RECT     rect   = {};
    BOOL     isDone = FALSE;
    LRESULT  result = HTNOWHERE;



    CBR (hwnd != nullptr && m_toolbar != nullptr);

    isDone = ScreenToClient (hwnd, &client);
    CWR (isDone);

    isDone = GetClientRect (hwnd, &rect);
    CWR (isDone);

    result = ClassifyLengthResize (client, SIZE { rect.right - rect.left, rect.bottom - rect.top }, m_toolbar->IsVertical(),
                                   MulDiv (kResizeEndDp, (int) GetDpi(), USER_DEFAULT_SCREEN_DPI));

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::ClassifyLengthResize
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiToolbarWindow::ClassifyLengthResize (POINT clientPx, SIZE clientSizePx, bool vertical, int endPx)
{
    int  along  = vertical ? clientPx.y : clientPx.x;
    int  length = vertical ? clientSizePx.cy : clientSizePx.cx;



    if (clientPx.x < 0 || clientPx.y < 0 || clientPx.x >= clientSizePx.cx || clientPx.y >= clientSizePx.cy)
    {
        return HTNOWHERE;
    }

    if (along < endPx)
    {
        return vertical ? HTTOP : HTLEFT;
    }

    if (along >= length - endPx)
    {
        return vertical ? HTBOTTOM : HTRIGHT;
    }

    return HTCLIENT;
}
