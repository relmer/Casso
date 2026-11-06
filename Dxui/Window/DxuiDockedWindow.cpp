#include "Pch.h"

#include "Window/DxuiDockedWindow.h"
#include "Core/DxuiPaneMetrics.h"
#include "Theme/DxuiDwm.h"
#include "Core/WindowTrace.h"





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

    CBRA (m_site != nullptr);

    FinishCreate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::CreateHeadless
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiDockedWindow::CreateHeadless (const CreateParams & params)
{
    HRESULT  hr = S_OK;



    hr = DxuiWindow::CreateHeadless (params);
    CHR (hr);

    CBRA (m_site != nullptr);

    FinishCreate();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::FinishCreate
//
//  Windows 11 rounds the window's corners, and the site, which fills the
//  window, draws the panes on them to the same radius.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::FinishCreate()
{
    bool  isRounded = DxuiDwm::IsWindows11OrGreater();



    m_site->SetWindowCornerDip (isRounded ? DxuiPaneMetrics::kWindowCornerDip : 0);

    SetOnModalLoopTick ([this] { OnMoveLoopTick(); });
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
//  DxuiDockedWindow::TryGetHeaderFadeRow
//
//  The site lays its groups out in pixels at its DPI, so the row is used as
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::TryGetHeaderFadeRow (const DxuiDockSite & site, RECT & outRowPx)
{
    const DxuiTabGroup  * group = nullptr;



    outRowPx = RECT {};

    if (site.GetGroupCount() == 0)
    {
        return false;
    }

    group    = site.GetGroup (0);
    outRowPx = site.GetCarriedPane().empty() ? group->GetTitleRect() : group->GetStripRect();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PaintDragLayer
//
//  The marks of a drag in the site, over the panes and their pictures, then
//  the header fade, drawn after the page's text so it fades that too. In a
//  composited window the fade's bands, drawn in erase mode, lower the alpha
//  of what is under them, and the desktop is visible through them.
//
//  The fade is not a modal overlay: an overlay's input routing sends the
//  mouse and keys of a move to OnOverlayMouse and OnOverlayKey, and the
//  window stops following the pointer once the fade is on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PaintDragLayer (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme)
{
    RECT  row = {};



    if (m_site == nullptr)
    {
        return;
    }

    m_site->PaintDragLayer (painter, text, theme);

    if (m_headerFade && TryGetHeaderFadeRow (*m_site, row))
    {
        PaintHeaderFade (painter, row);
    }
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

    if (GetHwnd() != nullptr)
    {
        DxuiDwm::ApplyBorderColor (GetHwnd(), argb);
    }

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
//  A drag in the site is canceled when the window loses the mouse with the
//  button still down. The window releases the capture itself as the button
//  comes up, with the button already up, which is no loss.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnCreate()
{
    InputFilter  filter;



    m_site = CreateChild<DxuiDockSite>();

    filter.onMouseLost = [this] (UINT)
    {
        m_site->OnDragMouseLost (DxuiInputState::IsKeyDown (VK_LBUTTON));
    };

    AddInputFilter (m_site, std::move (filter));
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
    if (m_isMoving)
    {
        if (ev.kind == DxuiMouseEventKind::Up && ev.button == DxuiMouseButton::Left)
        {
            EndMove (false);
        }
        else if (ev.kind == DxuiMouseEventKind::Move)
        {
            MoveWithPointer();
        }

        return true;
    }

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
//  Escape cancels a drag in the site before m_onKey is called.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiDockedWindow::OnKey (const DxuiKeyEvent & ev)
{
    if (m_isMoving && ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE)
    {
        EndMove (true);
        return true;
    }

    if (m_site != nullptr && m_site->OnDragKey (ev))
    {
        return true;
    }

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
    POINT          cursor     = {};
    bool           buttonDown = m_readPointer (cursor);
    LARGE_INTEGER  start      = {};
    LARGE_INTEGER  end        = {};
    LARGE_INTEGER  frequency  = {};



    Report (m_drag.OnTick (buttonDown, GetScreenSize()));

    if (m_onMoveFrame)
    {
        QueryPerformanceCounter (&start);
        m_onMoveFrame();
        QueryPerformanceCounter (&end);
        QueryPerformanceFrequency (&frequency);

        WindowTrace::Log ("move.tick", "floating", std::format ("frame {:.1f} ms", (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) frequency.QuadPart));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnWindowPlaced
//
//  The move loop ended, which is where a caption drag ends. Each step of a
//  move by BeginMove places the window too, and ends nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnWindowPlaced()
{
    if (m_isMoving)
    {
        return;
    }

    Report (m_drag.OnLoopEnd (GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnDpiChanged
//
//  Dragged onto a monitor at another scale, the window changes size to keep
//  its size in DIPs. The drag is rebased at the new size rather than taken
//  for a resize, so it goes on. During a move by BeginMove the grab point
//  is scaled with the window, so the pointer is over the same place in it
//  in proportion and a window that shrank is not left beside the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::OnDpiChanged (UINT newDpi)
{
    SIZE  size = GetScreenSize();



    UNREFERENCED_PARAMETER (newDpi);

    m_drag.Rebase (size);

    if (m_isMoving)
    {
        m_moveGrab = ScaleGrab (m_moveGrab, m_moveSize, size);
        m_moveSize = size;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::ScaleGrab
//
//  A point in a window of size `from`, at the same place in proportion in a
//  window of size `to`, rounded to the nearest pixel; `grab` itself when
//  `from` is empty.
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiDockedWindow::ScaleGrab (POINT grab, SIZE from, SIZE to)
{
    POINT  scaled = grab;



    if (from.cx > 0 && from.cy > 0)
    {
        scaled.x = (LONG) std::lround ((double) grab.x * (double) to.cx / (double) from.cx);
        scaled.y = (LONG) std::lround ((double) grab.y * (double) to.cy / (double) from.cy);
    }

    return scaled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::PollCaptionDrag
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::PollCaptionDrag()
{
    POINT  cursor     = {};
    bool   buttonDown = m_readPointer (cursor);



    //  A move whose capture went elsewhere, as to a menu, ends there.
    if (m_isMoving && !GetSource()->HasMouseCapture())
    {
        EndMove (false);
        return;
    }

    if (!m_isMoving)
    {
        Report (m_drag.OnPoll (buttonDown, GetScreenSize()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::OnNcLButtonDown
//
//  A press on the title bar calls BeginMove.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult DxuiDockedWindow::OnNcLButtonDown (LRESULT hitTest, int xScreen, int yScreen)
{
    UNREFERENCED_PARAMETER (xScreen);
    UNREFERENCED_PARAMETER (yScreen);

    if (hitTest != HTCAPTION)
    {
        return DxuiMessageResult::NotHandled;
    }

    BeginMove();
    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::BeginMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::BeginMove()
{
    POINT  cursor = {};
    RECT   rect   = GetScreenRect();



    if (!IsCreated() || m_isMoving || !m_readPointer (cursor))
    {
        return;
    }

    m_isMoving  = true;
    m_moveGrab  = POINT { cursor.x - rect.left, cursor.y - rect.top };
    m_moveStart = rect;
    m_moveSize  = SIZE { rect.right - rect.left, rect.bottom - rect.top };

    GetSource()->CaptureMouse();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::MoveWithPointer
//
//  The window follows the pointer, and each step reports a move, as a tick
//  of the system's move loop did.
//
//  THE TICK HAS THE SIZE AFTER THE WINDOW IS PLACED. A step that moves the
//  window onto a monitor at another scale changes its size inside
//  SetWindowRectPx, which rebases the drag at the new size; a tick at the
//  size from before would read as a resize and cancel the drag, and no drop
//  targets would be drawn for the rest of it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::MoveWithPointer()
{
    POINT  cursor     = {};
    bool   buttonDown = m_readPointer (cursor);
    SIZE   size       = GetScreenSize();
    RECT   rect       = {};



    rect.left   = cursor.x - m_moveGrab.x;
    rect.top    = cursor.y - m_moveGrab.y;
    rect.right  = rect.left + size.cx;
    rect.bottom = rect.top  + size.cy;

    GetSource()->SetWindowRectPx (rect);

    Report (m_drag.OnTick (buttonDown, GetScreenSize()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::EndMove
//
//  A canceled move puts the window back where it began.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::EndMove (bool isCanceled)
{
    m_isMoving = false;

    GetSource()->ReleaseMouse();

    if (isCanceled)
    {
        SetScreenRect (m_moveStart);
        Report (m_drag.Cancel());
        return;
    }

    OnWindowPlaced();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::Report
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::Report (DxuiCaptionDragTracker::Event ev)
{
    POINT  cursor = {};



    (void) m_readPointer (cursor);

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

    case DxuiCaptionDragTracker::Event::Resized:
        if (m_onResized)
        {
            m_onResized();
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
    if (!IsCreated())
    {
        return;
    }

    GetSource()->SetWindowRectPx (rectPx);

    //  A rect saved on a larger screen can exceed this one. Size only: a pane
    //  torn off is put under the pointer, wherever on the screen that is.
    GetSource()->FitToWorkArea (DxuiWorkAreaFit::SizeOnly);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::GetScreenRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiDockedWindow::GetScreenRect() const
{
    return IsCreated() ? GetSource()->GetWindowRectPx() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockedWindow::FitToWorkArea
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDockedWindow::FitToWorkArea (DxuiWorkAreaFit fit)
{
    if (IsCreated())
    {
        GetSource()->FitToWorkArea (fit);
    }
}
