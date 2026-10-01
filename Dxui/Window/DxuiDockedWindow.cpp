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
//  DxuiDockedWindow::OnFilesDropped
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::OnFilesDropped (const std::vector<std::wstring> & paths)
{
    return m_onFiles ? m_onFiles (paths) : false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnMoveLoopTick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnMoveLoopTick()
{
    bool  buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    Report (m_drag.OnTick (buttonDown, GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnWindowPlaced
//
//  The move loop ended, which is where a caption drag ends.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnWindowPlaced()
{
    Report (m_drag.OnLoopEnd (GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PollCaptionDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PollCaptionDrag()
{
    bool  buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;



    Report (m_drag.OnPoll (buttonDown, GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::Report
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::Report (DxuiCaptionDragTracker::Event ev)
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
//  DxuiDockedWindow::GetScreenSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiDockedWindow::GetScreenSize() const
{
    RECT  rect = GetScreenRect();



    return SIZE { rect.right - rect.left, rect.bottom - rect.top };
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
