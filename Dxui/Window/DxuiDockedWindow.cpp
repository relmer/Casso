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
//  DxuiDockedWindow::SetHeaderFade
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::SetHeaderFade (bool on)
{
    if (m_headerFade == on)
    {
        return;
    }

    m_headerFade = on;
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::GetHeaderFade
//
//  Equal bands across the left half of the row, each taking away less than
//  the one before, the last nearly nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockDragMark> DxuiDockedWindow::GetHeaderFade (const RECT & rowPx)
{
    std::vector<DxuiDockDragMark>  bands;
    long                           reach = (rowPx.right - rowPx.left) / 2;
    DxuiDockDragMark               band;



    if (reach <= 0 || rowPx.bottom <= rowPx.top)
    {
        return bands;
    }

    for (int i = 0; i < kFadeBands; i++)
    {
        float  erase = kFadeMaxErase * (float) (kFadeBands - i) / (float) kFadeBands;

        band.rect   = RECT { rowPx.left + reach * i / kFadeBands, rowPx.top, rowPx.left + reach * (i + 1) / kFadeBands, rowPx.bottom };
        band.argb   = (uint32_t) std::lround (erase * 255.0f) << 24;

        if (band.rect.right > band.rect.left)
        {
            bands.push_back (band);
        }
    }

    return bands;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PaintModalOverlay
//
//  The header fade, drawn after the page's text so it fades that too. In a
//  composited window the bands, drawn in erase mode, lower the alpha of what
//  is under them, so the desktop shows through.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiTabGroup  * group = nullptr;
    RECT            row   = {};
    UINT            dpi   = GetDpi();



    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);

    if (m_site == nullptr || m_site->GetGroupCount() == 0)
    {
        return;
    }

    group = m_site->GetGroup (0);
    row   = m_site->GetCarriedPane().empty() ? group->GetTitleRect() : group->GetStripRect();
    row   = RECT { MulDiv (row.left,  (int) dpi, USER_DEFAULT_SCREEN_DPI), MulDiv (row.top,    (int) dpi, USER_DEFAULT_SCREEN_DPI),
                   MulDiv (row.right, (int) dpi, USER_DEFAULT_SCREEN_DPI), MulDiv (row.bottom, (int) dpi, USER_DEFAULT_SCREEN_DPI) };

    PaintHeaderFade (painter, row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PaintHeaderFade
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PaintHeaderFade (IDxuiPainter & painter, const RECT & rowPx)
{
    painter.SetErase (true);

    for (const DxuiDockDragMark & band : GetHeaderFade (rowPx))
    {
        painter.FillRect ((float) band.rect.left, (float) band.rect.top,
                          (float) (band.rect.right - band.rect.left), (float) (band.rect.bottom - band.rect.top), band.argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::SetFocusedLook
//
//  The first group takes the focused look through its active pane, and
//  Windows 11's border follows it. DWM is called, and the window repainted,
//  only when the look or its color changed, so an owner can call this once a
//  frame.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::SetFocusedLook (bool focused, const IDxuiTheme & theme)
{
    uint32_t      argb = focused ? theme.FocusAccent() : theme.Border();
    std::wstring  pane;



    if (m_site == nullptr)
    {
        return;
    }

    if (focused && m_site->GetGroupCount() > 0)
    {
        pane = m_site->GetPaneOf (m_site->GetGroup (0)->GetActiveContent());
    }

    m_site->SetFocusedPane (pane);

    if (focused == m_focusedLook && argb == m_borderArgb)
    {
        return;
    }

    m_focusedLook = focused;
    m_borderArgb  = argb;

    DxuiDwm::ApplyBorderColor (GetHwnd(), argb);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnWindowFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnWindowFocusChanged (bool focused)
{
    if (m_onFocusChanged)
    {
        m_onFocusChanged (focused);
    }
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
//  DxuiDockedWindow::OnDpiChanged
//
//  Dragged onto a monitor at another scale, the window takes the size the
//  system gives it there. That is not a resize, so the drag goes on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnDpiChanged (UINT newDpi)
{
    UNREFERENCED_PARAMETER (newDpi);

    m_drag.Rebase (GetScreenSize());
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
