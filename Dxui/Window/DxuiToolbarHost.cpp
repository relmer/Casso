#include "Pch.h"

#include "Window/DxuiToolbarHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Attach
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Attach (DxuiWindow * owner, DxuiToolbar * toolbar, DxuiDockSite * dockSite, HINSTANCE hInstance)
{
    m_owner     = owner;
    m_toolbar   = toolbar;
    m_dockSite  = dockSite;
    m_hInstance = hInstance;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::ResetDock
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::ResetDock()
{
    m_dock = DxuiToolbarDock {};
    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetOwnerHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DxuiToolbarHost::GetOwnerHwnd() const
{
    return (m_owner != nullptr) ? m_owner->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Save
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Save()
{
    if (m_onSave)
    {
        m_onSave (m_dock.ToText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::EdgeToDockSide
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockSide DxuiToolbarHost::EdgeToDockSide (DxuiToolbarDock::Edge edge)
{
    switch (edge)
    {
    case DxuiToolbarDock::Edge::Bottom: return DxuiDockSide::Bottom;
    case DxuiToolbarDock::Edge::Left:   return DxuiDockSide::Left;
    case DxuiToolbarDock::Edge::Right:  return DxuiDockSide::Right;
    default:                            return DxuiDockSide::Top;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetDockedRect
//
//  The toolbar takes a band along its edge, at its place along that edge,
//  as long as its entries need or the edge allows.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarHost::GetDockedRect (const DxuiToolbarDock & dock, const RECT & area, int lengthPx, int bandPx, int marginPx, int dpi)
{
    bool  vertical = dock.IsVertical();
    int   edgeLen  = vertical ? area.bottom - area.top : area.right - area.left - marginPx * 2;
    int   length   = (std::min) (lengthPx, edgeLen);
    int   offset   = DxuiToolbarDock::ClampOffset (MulDiv (dock.offsetDip, dpi, USER_DEFAULT_SCREEN_DPI), edgeLen, length);
    int   start    = area.left + marginPx + offset;
    int   down     = area.top + offset;



    switch (dock.edge)
    {
    case DxuiToolbarDock::Edge::Bottom: return RECT { start,                area.bottom - bandPx, start + length,     area.bottom       };
    case DxuiToolbarDock::Edge::Left:   return RECT { area.left,            down,                 area.left + bandPx, down + length     };
    case DxuiToolbarDock::Edge::Right:  return RECT { area.right - bandPx,  down,                 area.right,         down + length     };
    default:                            return RECT { start,                area.top,             start + length,     area.top + bandPx };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetEdgeShareThickness
//
////////////////////////////////////////////////////////////////////////////////

long DxuiToolbarHost::GetEdgeShareThickness (DxuiToolbarDock::Edge edge, int bandPx, int marginPx)
{
    return (edge == DxuiToolbarDock::Edge::Top) ? bandPx : bandPx - marginPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetTearOffTopLeft
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiToolbarHost::GetTearOffTopLeft (POINT screenPx, bool vertical, int gripPx, int bandPx)
{
    return vertical ? POINT { screenPx.x - bandPx / 2, screenPx.y - gripPx / 2 }
                    : POINT { screenPx.x - gripPx / 2, screenPx.y - bandPx / 2 };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Layout
//
//  Docked, the band is the dock site's edge strip too, so auto-hidden panes'
//  tabs on that edge run beside the toolbar rather than in a strip of their
//  own, and the panes beside it draw its long sides. Floating, the toolbar
//  is in its own window and the dock site has every edge.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler)
{
    int   band   = scaler.ToPx (GetBandDipOfBar());
    int   margin = scaler.ToPx (kMarginDp);
    RECT  bar    = {};



    m_scaler = scaler;
    m_area   = area;

    if (m_toolbar == nullptr)
    {
        return;
    }

    m_toolbar->SetEndEdges (m_float == nullptr);

    if (m_float != nullptr)
    {
        if (m_dockSite != nullptr)
        {
            m_dockSite->ClearEdgeShare();
        }

        return;
    }

    m_toolbar->SetTextRenderer   ((m_owner != nullptr) ? m_owner->GetTextRenderer() : nullptr);
    m_toolbar->SetHostClientRect (hostClient);
    m_toolbar->SetVertical       (m_dock.IsVertical());

    bar = GetDockedRect (m_dock, area, m_toolbar->GetNaturalLengthPx (scaler), band, margin, (int) scaler.GetDpi());

    if (m_dockSite != nullptr)
    {
        m_dockSite->SetEdgeShare (EdgeToDockSide (m_dock.edge),
                                  GetEdgeShareThickness (m_dock.edge, band, margin),
                                  m_dock.IsVertical() ? bar.top    : bar.left,
                                  m_dock.IsVertical() ? bar.bottom : bar.right);
    }

    m_toolbar->Layout (bar, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::RouteDrag
//
//  A press on the grab handle carries the docked toolbar: it slides along
//  the band it is in, however near another edge the pointer comes, and only
//  a pull well away from that band tears it off to float under the pointer.
//  The release saves its place.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarHost::RouteDrag (const DxuiMouseEvent & ev)
{
    POINT            at    = ev.positionDip;
    RECT             bar   = {};
    int              along = 0;
    DxuiToolbarDock  dock;



    //  A floating toolbar's own window moves it.
    if (m_toolbar == nullptr || m_float != nullptr)
    {
        return false;
    }

    bar = m_toolbar->GetBounds();

    if (!m_dragging)
    {
        if (ev.kind != DxuiMouseEventKind::Down || m_toolbar->IsMenuOpen() || !m_toolbar->IsOnGrip (at.x, at.y))
        {
            return false;
        }

        along      = m_dock.IsVertical() ? at.y - bar.top : at.x - bar.left;
        //  Across the top or bottom the toolbar starts a margin in from the
        //  window's edge, so the grab point counts it.
        m_grab     = POINT { along + m_scaler.ToPx (kMarginDp), along };
        m_dragging = true;

        if (m_onDragStart)
        {
            m_onDragStart();
        }

        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (DxuiToolbarDock::IsPulledOut (at, m_dock.edge, m_area, m_scaler.ToPx (GetBandDipOfBar() + kDockReachDp), m_scaler.ToPx (kPullDp)))
        {
            TearOff (at);
            break;
        }

        dock = DxuiToolbarDock::SlideAlong (at, m_grab, m_dock, m_area, (int) m_scaler.GetDpi());

        if (!(dock == m_dock))
        {
            m_dock = dock;

            if (m_onLayout)
            {
                m_onLayout();
            }
        }

        break;

    case DxuiMouseEventKind::Up:
        m_dragging = false;
        Save();
        break;

    default:
        break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::TearOff
//
//  A drag of the docked toolbar that leaves the edges floats it at once, its
//  grab handle under the pointer, and the floating window carries on with
//  the drag while the button is still down.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::TearOff (POINT clientPx)
{
    POINT  screen = clientPx;
    int    grip   = m_scaler.ToPx (DxuiToolbar::kGripDp);
    int    band   = m_scaler.ToPx (GetBandDipOfBar());



    ClientToScreen (GetOwnerHwnd(), &screen);

    m_dragging           = false;
    m_dock.floatVertical = m_dock.IsVertical();
    m_dock.floating      = true;
    m_dock.floatPx       = GetTearOffTopLeft (screen, m_dock.floatVertical, grip, band);

    Float();

    if (m_float == nullptr)
    {
        return;
    }

    ReleaseCapture();
    m_float->BeginMove();

    if (m_onLayout)
    {
        m_onLayout();
    }

    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetFloatingRect
//
//  The floating window: as long as the toolbar's icons need and one band
//  high, with its top left at `topLeftPx`, or by the owner's when that is
//  on no monitor, as a place saved on a monitor since unplugged would be.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarHost::GetFloatingRect (POINT topLeftPx)
{
    int   length = 0;
    int   band   = m_scaler.ToPx (GetBandDipOfBar());
    RECT  rect   = {};
    RECT  owner  = {};



    m_toolbar->SetVertical (m_dock.floatVertical);
    m_toolbar->SetLabels   (false);

    length = m_toolbar->GetNaturalLengthPx (m_scaler);
    rect   = m_dock.floatVertical ? RECT { topLeftPx.x, topLeftPx.y, topLeftPx.x + band, topLeftPx.y + length }
                                  : RECT { topLeftPx.x, topLeftPx.y, topLeftPx.x + length, topLeftPx.y + band };

    if (MonitorFromRect (&rect, MONITOR_DEFAULTTONULL) == nullptr && GetWindowRect (GetOwnerHwnd(), &owner))
    {
        OffsetRect (&rect, owner.left + band - rect.left, owner.top + band - rect.top);
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Float
//
//  A window for the floating toolbar, with no title, at its saved place,
//  and the toolbar moved into it, showing icons alone.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Float()
{
    std::unique_ptr<DxuiToolbarWindow>  window = std::make_unique<DxuiToolbarWindow>();
    std::unique_ptr<IDxuiControl>       owned;
    DxuiWindow::CreateParams            params;
    RECT                                rect   = {};
    int                                 dpi    = (std::max) ((int) m_scaler.GetDpi(), 1);
    HRESULT                             hr     = S_OK;



    if (m_owner == nullptr || m_toolbar == nullptr)
    {
        m_dock.floating = false;
        return;
    }

    rect = GetFloatingRect (m_dock.floatPx);

    params.hInstance        = m_hInstance;
    params.ownerHwnd        = GetOwnerHwnd();
    params.initialSizeDip   = { MulDiv (rect.right - rect.left, USER_DEFAULT_SCREEN_DPI, dpi), MulDiv (rect.bottom - rect.top, USER_DEFAULT_SCREEN_DPI, dpi) };
    params.resizable        = false;
    params.captionStyle     = DxuiCaptionStyle::None;
    params.createNoActivate = true;
    params.toolWindow       = true;

    hr = window->Create (params);

    if (FAILED (hr))
    {
        //  With no window to float in, the toolbar docks where it was.
        m_dock.floating = false;
        m_toolbar->SetLabels (true);
        return;
    }

    if (m_onFloatCreated)
    {
        m_onFloatCreated (*window);
    }

    owned = m_owner->DetachChild (m_toolbar);

    if (owned != nullptr)
    {
        (void) window->AttachChild (std::move (owned));
    }

    m_toolbar->SetPopupHost    (window->GetPopupHost());
    m_toolbar->SetTextRenderer (window->GetTextRenderer());
    m_toolbar->OnToolbarMouseLeave();

    window->SetToolbar             (m_toolbar);
    window->SetOnContentMouse      ([this] (const DxuiMouseEvent & ev) { return m_onFloatMouse ? m_onFloatMouse (ev) : false; });
    window->SetOnCaptionDrag       ([this] (POINT screen)              { OnFloatDrag (screen); });
    window->SetOnCaptionDragEnd    ([this] (POINT screen)              { OnFloatDragEnd (screen); });
    window->SetOnCaptionDragCancel ([this]                             { FinishSnap(); });
    window->SetOnMoveLoopFrame     ([this]                             { if (m_onMoveFrame) { m_onMoveFrame(); } });
    window->SetScreenRect          (rect);

    if (IsWindowVisible (GetOwnerHwnd()))
    {
        window->Show (false);
    }

    m_float = std::move (window);

    if (m_onFloatChanged)
    {
        m_onFloatChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::DockBack
//
//  The toolbar back in the owner, labeled again, and its floating window
//  gone. Run from the frame, never from inside that window's own message.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::DockBack()
{
    std::unique_ptr<IDxuiControl>  owned;



    if (m_float == nullptr)
    {
        return;
    }

    owned = m_float->DetachChild (m_toolbar);

    if (owned != nullptr)
    {
        (void) m_owner->AttachChild (std::move (owned));
    }

    m_float->SetToolbar (nullptr);
    m_float->Hide();
    m_float.reset();

    m_toolbar->SetLabels       (true);
    m_toolbar->SetPopupHost    (m_owner->GetPopupHost());
    m_toolbar->SetTextRenderer (m_owner->GetTextRenderer());
    m_toolbar->OnToolbarMouseLeave();

    if (m_onFloatChanged)
    {
        m_onFloatChanged();
    }

    if (m_onLayout)
    {
        m_onLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::ResumeSnapDrag
//
//  A toolbar that snapped into a band while the button is still down goes
//  on sliding along it with the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::ResumeSnapDrag()
{
    constexpr SHORT  kKeyDown = (SHORT) 0x8000;



    if (!m_snapDragOn)
    {
        return;
    }

    m_snapDragOn = false;

    if ((GetKeyState (VK_LBUTTON) & kKeyDown) != 0)
    {
        SetCapture (GetOwnerHwnd());
        m_dragging = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Sync
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Sync()
{
    bool  visible = IsWindowVisible (GetOwnerHwnd()) != FALSE;
    HWND  hwnd    = nullptr;



    if (m_dock.floating && m_float == nullptr)
    {
        Float();

        if (m_onLayout)
        {
            m_onLayout();
        }
    }
    else if (!m_dock.floating && m_float != nullptr)
    {
        DockBack();
        ResumeSnapDrag();
    }

    if (m_float == nullptr)
    {
        return;
    }

    hwnd = m_float->GetHwnd();

    FitFloatWindow();

    if ((IsWindowVisible (hwnd) != FALSE) != visible)
    {
        if (visible)
        {
            m_float->Show (false);
        }
        else
        {
            m_float->Hide();
        }
    }

    m_float->PollCaptionDrag();
    m_float->Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::FitFloatWindow
//
//  The window stays as long as the toolbar's entries need, so the toolbar
//  never falls back on See more while it floats: a length measured before
//  the window had its DPI, or before a theme or entry change, comes right
//  here. The window's frame comes on top of the toolbar's length.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::FitFloatWindow()
{
    HWND  hwnd    = m_float->GetHwnd();
    RECT  client  = {};
    RECT  current = m_float->GetScreenRect();
    RECT  wanted  = GetFloatingRect (POINT { current.left, current.top });
    int   frameX  = 0;
    int   frameY  = 0;



    if (!GetClientRect (hwnd, &client))
    {
        return;
    }

    frameX = (current.right - current.left) - (client.right - client.left);
    frameY = (current.bottom - current.top) - (client.bottom - client.top);

    if (wanted.right - wanted.left + frameX != current.right - current.left || wanted.bottom - wanted.top + frameY != current.bottom - current.top)
    {
        m_float->SetScreenRect (RECT { current.left, current.top, current.left + wanted.right - wanted.left + frameX, current.top + wanted.bottom - wanted.top + frameY });
        GetClientRect (hwnd, &client);
    }

    //  Measuring planned the toolbar for any length, so it is laid out
    //  again for the window it is in.
    m_toolbar->Layout (client, m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Hide()
{
    if (m_float != nullptr)
    {
        m_float->Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::OnFloatDragEnd
//
//  A floating toolbar dropped near an edge of the owner docks there, at the
//  place that keeps its grab handle under the pointer; dropped anywhere else
//  it stays where it was put. The frame docks it, outside the floating
//  window's own message.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::OnFloatDragEnd (POINT screenPx)
{
    POINT  client = screenPx;
    RECT   rect   = (m_float != nullptr) ? m_float->GetScreenRect() : RECT {};
    int    reach  = m_scaler.ToPx (GetBandDipOfBar() + kDockReachDp);
    POINT  grab   = POINT { screenPx.x - rect.left, screenPx.y - rect.top };



    ScreenToClient (GetOwnerHwnd(), &client);

    if (m_snapping)
    {
        FinishSnap();
        return;
    }

    if (IsWindowVisible (GetOwnerHwnd()) && DxuiToolbarDock::IsInDockBand (client, m_area, reach))
    {
        //  Across the top or bottom the toolbar starts a margin in from the
        //  window's edge, so the grab point counts it.
        m_dock = DxuiToolbarDock::PickForDrop (client, POINT { grab.x + m_scaler.ToPx (kMarginDp), grab.x }, m_area, (int) m_scaler.GetDpi());
    }
    else
    {
        m_dock.floating = true;
        m_dock.floatPx  = POINT { rect.left, rect.top };
    }

    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::FinishSnap
//
//  The move loop a snap ended has ended: the toolbar takes the place it
//  snapped to, and the frame docks it and carries the drag on.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::FinishSnap()
{
    if (!m_snapping)
    {
        return;
    }

    m_snapping   = false;
    m_snapDragOn = true;
    m_dock       = m_snapDock;

    Save();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::OnFloatDrag
//
//  A floating toolbar dragged near a side of the owner stands on end, and
//  near the top or bottom lies flat again; away from every edge it keeps
//  the orientation it last took, so it can be left floating either way.
//  Into a band, it snaps in where the pointer holds it: the move loop ends
//  here, and once it has, the frame docks the toolbar and carries the drag
//  on as a drag of the docked toolbar, which slides along the band and
//  leaves it only past the pull.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::OnFloatDrag (POINT screenPx)
{
    POINT  client   = screenPx;
    RECT   rect     = {};
    int    reach    = m_scaler.ToPx (GetBandDipOfBar() + kDockReachDp);
    bool   vertical = false;



    if (m_float == nullptr || !IsWindowVisible (GetOwnerHwnd()))
    {
        return;
    }

    ScreenToClient (GetOwnerHwnd(), &client);

    if (m_snapping)
    {
        return;
    }

    rect = m_float->GetScreenRect();

    if (DxuiToolbarDock::IsInDockBand (client, m_area, reach))
    {
        m_grab     = DxuiToolbarDock::GrabForDocking (POINT { screenPx.x - rect.left, screenPx.y - rect.top }, m_dock.floatVertical, m_scaler.ToPx (kMarginDp));
        m_snapDock = DxuiToolbarDock::PickForDrop (client, m_grab, m_area, (int) m_scaler.GetDpi());
        m_snapping = true;

        ReleaseCapture();
        return;
    }

    vertical = DxuiToolbarDock::PickFloatVertical (client, m_area, reach, m_dock.floatVertical);

    if (vertical == m_dock.floatVertical)
    {
        return;
    }

    m_dock.floatVertical = vertical;
    m_dock.floatPx       = POINT { rect.left, rect.top };

    m_float->SetScreenRect (GetFloatingRect (m_dock.floatPx));
    m_float->Invalidate();
}
