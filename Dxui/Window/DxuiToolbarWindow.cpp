#include "Pch.h"

#include "Window/DxuiToolbarWindow.h"
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::DxuiToolbarWindow
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarWindow::DxuiToolbarWindow()
{
    SetOnModalLoopTick ([this]
    {
        if (m_onMoveFrame)
        {
            m_onMoveFrame();
        }
    });
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
    RECT  bar = {};



    if (m_toolbar == nullptr)
    {
        return;
    }

    bar = GetToolbarBounds (boundsDip);

    m_toolbar->SetHostClientRect (boundsDip);
    m_toolbar->Layout            (bar, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetToolbarBounds
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarWindow::GetToolbarBounds (const RECT & clientPx) const
{
    bool  vertical = m_toolbar != nullptr && m_toolbar->IsVertical();



    return GetToolbarRect (clientPx, vertical, GetEndPx());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnMouse
//
//  A press on an end sizes the window, and one on the grab handle goes to
//  the owner, which moves it; the rest is the owner's too.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarWindow::OnMouse (const DxuiMouseEvent & ev)
{
    bool  onGrip = m_toolbar != nullptr && m_toolbar->IsOnGrip (ev.positionDip.x, ev.positionDip.y);



    if (OnSizingMouse (ev))
    {
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && onGrip && !m_toolbar->IsMenuOpen() && m_onGripPress)
    {
        m_onGripPress (ClientToScreenPx (ev.positionDip));
        return true;
    }

    return m_onMouse ? m_onMouse (ev) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnSizingMouse
//
//  A press on an end starts a size drag, which the window's own capture
//  carries: each move puts that end where the pointer is, along the length
//  only, and the release, or a move with the button found up, ends it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarWindow::OnSizingMouse (const DxuiMouseEvent & ev)
{
    POINT    screen   = ClientToScreenPx (ev.positionDip);
    bool     vertical = m_toolbar != nullptr && m_toolbar->IsVertical();
    int      delta    = 0;
    int      minimum  = 0;
    LRESULT  end      = HTNOWHERE;



    if (m_sizingEnd == HTNOWHERE)
    {
        end = HitTestEnd (ev.positionDip);

        if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left || GetResizeCursor (end) == nullptr)
        {
            return false;
        }

        m_sizingEnd    = end;
        m_sizeFromPx   = screen;
        m_sizeFromRect = GetScreenRect();
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up || (ev.kind == DxuiMouseEventKind::Move && ev.button != DxuiMouseButton::Left))
    {
        m_sizingEnd = HTNOWHERE;
        return true;
    }

    if (ev.kind != DxuiMouseEventKind::Move)
    {
        return true;
    }

    delta   = vertical ? screen.y - m_sizeFromPx.y : screen.x - m_sizeFromPx.x;
    minimum = vertical ? m_sizeFromRect.right - m_sizeFromRect.left : m_sizeFromRect.bottom - m_sizeFromRect.top;

    SetScreenRect (GetResizedRect (m_sizeFromRect, m_sizingEnd, delta, minimum));
    Invalidate();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::ClientToScreenPx
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiToolbarWindow::ClientToScreenPx (POINT clientPx) const
{
    POINT  screen = clientPx;



    ClientToScreen (GetHwnd(), &screen);
    return screen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiToolbarWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = GetResizeCursor ((m_sizingEnd != HTNOWHERE) ? m_sizingEnd : HitTestEnd (clientPx));



    if (cursor != nullptr)
    {
        return cursor;
    }

    if (m_toolbar != nullptr && m_toolbar->IsOnGrip (clientPx.x, clientPx.y))
    {
        return IDC_SIZEALL;
    }

    return DxuiWindow::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::OnDpiChanging
//
//  Dragged onto a monitor at another scale, or with a monitor's scale
//  changed under it, the window takes the rectangle its owner gives it at
//  the new scale in place of the one the system suggests, so it is sized
//  once and its first frame there is drawn at that size.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::OnDpiChanging (UINT newDpi, RECT & inOutRectPx)
{
    if (m_onDpi)
    {
        m_onDpi (newDpi, inOutRectPx);
    }
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
//  The window sizes itself rather than through the system's size loop. It
//  is made without a sizing frame, so that loop never starts for it:
//  DefWindowProc runs SC_SIZE only for a window with WS_THICKFRAME, and
//  that style would bring a frame along the long sides too.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarWindow::SetLengthResizable (bool resizable)
{
    m_lengthResizable = resizable;

    if (!resizable)
    {
        m_sizingEnd = HTNOWHERE;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetEndPx
//
//  How long each end is in this window's pixels; 0 for a window that does
//  not resize.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarWindow::GetEndPx() const
{
    return m_lengthResizable ? MulDiv (kResizeEndDp, (int) GetDpi(), USER_DEFAULT_SCREEN_DPI) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::HitTestEnd
//
////////////////////////////////////////////////////////////////////////////////

LRESULT DxuiToolbarWindow::HitTestEnd (POINT clientPx) const
{
    RECT  rect = {};



    if (!m_lengthResizable || m_toolbar == nullptr || !GetClientRect (GetHwnd(), &rect))
    {
        return HTNOWHERE;
    }

    return ClassifyLengthResize (clientPx, SIZE { rect.right - rect.left, rect.bottom - rect.top }, m_toolbar->IsVertical(), GetEndPx());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetToolbarRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarWindow::GetToolbarRect (const RECT & clientPx, bool vertical, int endPx)
{
    RECT  rect = clientPx;



    if (vertical)
    {
        rect.top    += endPx;
        rect.bottom -= endPx;
    }
    else
    {
        rect.left  += endPx;
        rect.right -= endPx;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetResizedRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarWindow::GetResizedRect (const RECT & startPx, LRESULT end, int deltaPx, int minLengthPx)
{
    RECT  rect = startPx;



    switch (end)
    {
    case HTLEFT:   rect.left   = (std::min) (startPx.left   + deltaPx, startPx.right  - minLengthPx); break;
    case HTRIGHT:  rect.right  = (std::max) (startPx.right  + deltaPx, startPx.left   + minLengthPx); break;
    case HTTOP:    rect.top    = (std::min) (startPx.top    + deltaPx, startPx.bottom - minLengthPx); break;
    case HTBOTTOM: rect.bottom = (std::max) (startPx.bottom + deltaPx, startPx.top    + minLengthPx); break;
    default:       break;
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarWindow::GetResizeCursor
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiToolbarWindow::GetResizeCursor (LRESULT end)
{
    switch (end)
    {
    case HTLEFT:
    case HTRIGHT:  return IDC_SIZEWE;
    case HTTOP:
    case HTBOTTOM: return IDC_SIZENS;
    default:       return nullptr;
    }
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
