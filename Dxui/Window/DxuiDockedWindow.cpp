#include "Pch.h"

#include "Window/DxuiDockedWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDockedWindow::Create (const CreateParams & params)
{
    HRESULT  hr = S_OK;



    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetOnModalLoopTick ([this] { OnMoveLoopTick(); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnCreate()
{
    m_site = CreateChild<DxuiDockSite>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnWindowClose
//
//  Closing a floating pane is the owner's to decide: it may dock the pane
//  back or hide it. Without an owner the window hides.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnWindowClose()
{
    if (m_onClosed)
    {
        m_onClosed();
        return;
    }

    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  area = boundsDip;



    area.top = std::min (area.bottom, area.top + (long) GetCaptionHeightPx());

    if (m_site != nullptr)
    {
        m_site->Layout (area, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::OnMouse (const DxuiMouseEvent & ev)
{
    if (m_site != nullptr && m_site->OnMouse (ev))
    {
        return true;
    }

    return m_onMouse ? m_onMouse (ev) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::OnKey (const DxuiKeyEvent & ev)
{
    return m_onKey ? m_onKey (ev) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiDockedWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = (m_site != nullptr) ? m_site->GetCursorForPoint (clientPx) : nullptr;



    return (cursor != nullptr) ? cursor : DxuiWindow::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnMappedCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::OnMappedCommand (int commandId)
{
    return m_onCommand ? m_onCommand (commandId) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnMoveLoopTick
//
//  The move loop also runs for a resize, which keeps no size; a tick whose
//  window has the size it had when the loop began is a move.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnMoveLoopTick()
{
    RECT   rect   = GetScreenRect();
    SIZE   size   = { rect.right - rect.left, rect.bottom - rect.top };
    POINT  cursor = {};



    if ((GetKeyState (VK_LBUTTON) & 0x8000) == 0)
    {
        return;
    }

    if (!m_dragging)
    {
        m_dragging = true;
        m_dragSize = size;
    }

    if (size.cx != m_dragSize.cx || size.cy != m_dragSize.cy)
    {
        return;
    }

    GetCursorPos (&cursor);

    if (m_onDrag)
    {
        m_onDrag (cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PollCaptionDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PollCaptionDrag()
{
    RECT   rect   = {};
    POINT  cursor = {};



    if (!m_dragging || (GetKeyState (VK_LBUTTON) & 0x8000) != 0)
    {
        return;
    }

    m_dragging = false;
    rect       = GetScreenRect();

    if (rect.right - rect.left != m_dragSize.cx || rect.bottom - rect.top != m_dragSize.cy)
    {
        return;
    }

    GetCursorPos (&cursor);

    if (m_onDragEnd)
    {
        m_onDragEnd (cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::SetScreenRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::SetScreenRect (const RECT & rectPx)
{
    SetWindowPos (GetHwnd(), nullptr, rectPx.left, rectPx.top, rectPx.right - rectPx.left, rectPx.bottom - rectPx.top,
                  SWP_NOZORDER | SWP_NOACTIVATE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::GetScreenRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockedWindow::GetScreenRect() const
{
    RECT  rect = {};



    GetWindowRect (GetHwnd(), &rect);
    return rect;
}