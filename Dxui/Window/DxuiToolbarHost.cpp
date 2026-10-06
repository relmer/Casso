#include "Pch.h"

#include "Window/DxuiToolbarHost.h"
#include "Core/DxuiSystemSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::Attach
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Attach (DxuiWindow * owner, DxuiToolbar * toolbar, DxuiDockSite * dockSite, HINSTANCE hInstance)
{
    if (m_owner != nullptr)
    {
        m_owner->RemoveMouseFilter (this);
    }

    m_owner     = owner;
    m_toolbar   = toolbar;
    m_dockSite  = dockSite;
    m_hInstance = hInstance;

    //  A drag of the handle takes the owner's mouse input ahead of anything
    //  the owner routes itself, so whatever the pointer crosses -- a menu
    //  bar, a status bar, another toolbar -- the drag sees every move and
    //  the release that ends it.
    if (m_owner != nullptr)
    {
        m_owner->AddMouseFilter (this, [this] (const DxuiMouseEvent & ev) { return RouteDrag (ev); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::DxuiToolbarHost
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarHost::DxuiToolbarHost()
{
    m_ownGroup->Add (this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::~DxuiToolbarHost
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarHost::~DxuiToolbarHost()
{
    if (m_owner != nullptr)
    {
        m_owner->RemoveMouseFilter (this);
    }

    m_group->Remove (this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::JoinGroup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::JoinGroup (DxuiToolbarDockGroup & group)
{
    m_group->Remove (this);

    m_group = &group;
    m_group->Add (this);
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
//  DxuiToolbarHost::GetTearOffLengthDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarHost::GetTearOffLengthDip (const RECT & bar, bool vertical, int dpi)
{
    int  lengthPx = vertical ? bar.bottom - bar.top : bar.right - bar.left;



    return MulDiv (lengthPx, USER_DEFAULT_SCREEN_DPI, (std::max) (dpi, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::GetFloatLengthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarHost::GetFloatLengthPx (const DxuiToolbarDock & dock, bool fillsEdge, int dpi)
{
    if (!fillsEdge || dock.floatLengthDip <= 0)
    {
        return 0;
    }

    return MulDiv (dock.floatLengthDip, dpi, USER_DEFAULT_SCREEN_DPI);
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
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler)
{
    (void) m_group->Layout (area, hostClient, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::BeginLayout
//
//  The group's layout starting: the toolbar readied to be measured, and
//  true when it is docked and so takes a place in a band. Floating, the
//  toolbar is in its own window and the dock site has every edge.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarHost::BeginLayout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler)
{
    m_scaler = scaler;
    m_area   = area;

    if (m_toolbar == nullptr)
    {
        return false;
    }

    m_toolbar->SetEndEdges (m_float == nullptr);

    if (m_float != nullptr)
    {
        if (m_dockSite != nullptr)
        {
            m_dockSite->ClearEdgeShare();
        }

        return false;
    }

    m_toolbar->SetTextRenderer   ((m_owner != nullptr) ? m_owner->GetTextRenderer() : nullptr);
    m_toolbar->SetHostClientRect (hostClient);
    m_toolbar->SetVertical       (m_dock.IsVertical());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::PlaceDocked
//
//  The docked toolbar at its place in its band. While it is carried, its
//  band is highlighted, so it is plain which band a drop puts it in and
//  whether that band is one of its own.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::PlaceDocked (const RECT & bar, const RECT & band)
{
    m_toolbar->Layout      (bar, m_scaler);
    m_toolbar->SetDropSlot (m_dragging ? band : RECT {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::RouteDrag
//
//  A press on the grab handle carries the docked toolbar: it slides along
//  the band it is in, moves into another band of its edge or into a new one
//  between two, however near another edge the pointer comes, and only a
//  pull well away from the edge's bands tears it off to float under the
//  pointer. The release saves its place, and its neighbors'.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarHost::RouteDrag (const DxuiMouseEvent & ev)
{
    POINT            at    = ev.positionDip;
    RECT             bar   = {};
    int              along = 0;
    int              reach = 0;
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

        m_toolbar->SetDropSlot (m_group->GetBandRect (m_dock.edge, m_dock.band));
        UpdateLift();

        if (m_onDragStart)
        {
            m_onDragStart();
        }

        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        reach = m_group->GetDepthPx (m_dock.edge) + m_scaler.ToPx (kDockReachDp);

        if (DxuiToolbarDock::IsPulledOut (at, m_dock.edge, m_area, reach, m_scaler.ToPx (kPullDp)))
        {
            TearOff (at);
            break;
        }

        dock = DxuiToolbarDock::SlideAlong (at, m_grab, m_dock, m_group->GetEdgeArea (m_dock.edge), (int) m_scaler.GetDpi());

        MoveAcrossBands (at, dock);
        break;

    case DxuiMouseEventKind::Up:
        m_dragging = false;
        m_toolbar->SetDropSlot (RECT {});
        UpdateLift();
        m_group->Commit (this);
        break;

    default:
        break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::MoveAcrossBands
//
//  The docked toolbar under a drag, at `dock` along its band: over another
//  band of its edge it joins that band, and near a boundary between two, or
//  either side of them all, it makes a band of its own there. Alone in its
//  band, it stays there over the boundaries either side of it, so a band of
//  its own does not jump under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::MoveAcrossBands (POINT clientPx, DxuiToolbarDock & dock)
{
    int                       across = DxuiToolbarBands::GetAcrossPx (clientPx, m_dock.edge, m_area);
    int                       split  = m_scaler.ToPx (DxuiToolbarDockGroup::kSplitDp);
    DxuiToolbarBands::Target  target;
    bool                      moved  = false;



    target = DxuiToolbarBands::PickTarget (across, m_group->GetBandThicknesses (m_dock.edge), m_dock.band, m_group->IsAloneInBand (this), split);

    dock.band = target.band;
    moved     = target.isNewBand || !(dock == m_dock);

    if (!moved)
    {
        return;
    }

    if (target.isNewBand)
    {
        m_group->InsertBand (m_dock.edge, target.band, this);
    }

    m_dock = dock;

    if (m_onLayout)
    {
        m_onLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::TryPickDrop
//
//  Where a floating toolbar dropped at `clientPx` docks, with `grab` where
//  the pointer holds it: on the edge whose bands the pointer is in or near,
//  in the band under it or a new band at the boundary it is near. False
//  away from every edge.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarHost::TryPickDrop (POINT clientPx, POINT grab, DxuiToolbarDock & outDock, bool & outNewBand)
{
    int                       reach  = m_scaler.ToPx (GetBandDipOfBar() + kDockReachDp);
    int                       split  = m_scaler.ToPx (DxuiToolbarDockGroup::kSplitDp);
    DxuiToolbarDock::Edge     edge   = DxuiToolbarDock::Edge::Top;
    DxuiToolbarBands::Target  target;
    bool                      isNear = false;



    isNear = DxuiToolbarBands::TryPickEdge (clientPx, m_area, m_group->GetPlacement().depthPx, reach, edge);

    if (!isNear)
    {
        return false;
    }

    target = DxuiToolbarBands::PickTarget (DxuiToolbarBands::GetAcrossPx (clientPx, edge, m_area), m_group->GetBandThicknesses (edge), DxuiToolbarDock::kNoBand, false, split);

    outDock      = DxuiToolbarDock::PickForDropOn (edge, clientPx, grab, m_group->GetEdgeArea (edge), (int) m_scaler.GetDpi());
    outDock.band = target.band;
    outNewBand   = target.isNewBand;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::TakeDrop
//
//  The toolbar takes a docked place, a new band moving the bands from it on
//  one further in, and every place is saved.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::TakeDrop (const DxuiToolbarDock & dock, bool isNewBand)
{
    if (isNewBand)
    {
        m_group->InsertBand (dock.edge, dock.band, this);
    }

    m_dock = dock;

    m_group->SaveAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::TearOff
//
//  A drag of the docked toolbar that leaves the edges floats it at once, its
//  grab handle under the pointer, and the floating window carries on with
//  the drag while the button is still down. A toolbar that fills its edge
//  floats as long as it was docked.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::TearOff (POINT clientPx)
{
    POINT  screen   = clientPx;
    int    grip     = m_scaler.ToPx (DxuiToolbar::kGripDp);
    int    band     = m_scaler.ToPx (GetBandDipOfBar());
    bool   vertical = m_dock.IsVertical();



    ClientToScreen (GetOwnerHwnd(), &screen);

    m_dragging            = false;
    m_dock.floatVertical  = vertical;
    m_dock.floating       = true;
    m_dock.floatPx        = GetTearOffTopLeft (screen, m_dock.floatVertical, grip, band);
    m_dock.floatLengthDip = m_fillsEdge ? GetTearOffLengthDip (m_toolbar->GetBounds(), vertical, (int) m_scaler.GetDpi()) : 0;

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
//  The floating window: `lengthPx` long, or as long as the toolbar's icons
//  need for 0, and one band high, with its top left at `topLeftPx`, or by
//  the owner's when that is on no monitor, as a place saved on a monitor
//  since unplugged would be.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiToolbarHost::GetFloatingRect (POINT topLeftPx, int lengthPx)
{
    int   length = lengthPx;
    int   band   = m_scaler.ToPx (GetBandDipOfBar());
    RECT  rect   = {};
    RECT  owner  = {};



    m_toolbar->SetVertical (m_dock.floatVertical);
    m_toolbar->SetLabels   (false);

    if (length <= 0)
    {
        length = m_toolbar->GetNaturalLengthPx (m_scaler);
    }

    rect = m_dock.floatVertical ? RECT { topLeftPx.x, topLeftPx.y, topLeftPx.x + band, topLeftPx.y + length }
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
//  and the toolbar moved into it, showing icons alone. A toolbar that fills
//  its edge keeps its length, and its window resizes along it, never
//  shorter than the band is thick. The window is made where it goes, so it
//  has that monitor's scale from the start and no scale change resizes it
//  under a drag that has just torn the toolbar off.
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

    rect = GetFloatingRect (m_dock.floatPx, GetFloatLengthPx (m_dock, m_fillsEdge, dpi));

    params.hInstance              = m_hInstance;
    params.ownerHwnd              = GetOwnerHwnd();
    params.initialSizeDip         = { MulDiv (rect.right - rect.left, USER_DEFAULT_SCREEN_DPI, dpi), MulDiv (rect.bottom - rect.top, USER_DEFAULT_SCREEN_DPI, dpi) };
    params.minSizeDip             = m_fillsEdge ? SIZE { GetBandDipOfBar(), GetBandDipOfBar() } : SIZE {};
    params.resizable              = false;
    params.frameless              = true;
    params.captionStyle           = DxuiCaptionStyle::None;
    params.createNoActivate       = true;
    params.toolWindow             = true;
    params.useInitialWindowRectPx = true;
    params.initialWindowRectPx    = rect;

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
    window->SetLengthResizable     (m_fillsEdge);
    window->SetOnContentMouse     ([this] (const DxuiMouseEvent & ev) { return m_onFloatMouse ? m_onFloatMouse (ev) : false; });
    window->SetOnCaptionDrag       ([this] (POINT screen)              { OnFloatDrag (screen); });
    window->SetOnCaptionDragEnd    ([this] (POINT screen)              { OnFloatDragEnd (screen); });
    window->SetOnCaptionDragCancel ([this]                             { FinishSnap(); });
    window->SetOnMoveLoopFrame     ([this]                             { RunMoveLoopFrame(); });
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

    UpdateLift();

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
//  DxuiToolbarHost::UpdateLift
//
//  The toolbar is lifted while it is carried, docked by its handle or
//  floating in its window's move loop, and painted above everything else in
//  the window it is in; put down, it settles back among the rest. While it
//  rises, the window it is in draws every frame.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::UpdateLift()
{
    bool          lifted   = m_dragging || (m_float != nullptr && m_float->IsMoving());
    int64_t       now      = (int64_t) GetTickCount64();
    DxuiWindow  * window   = (m_float != nullptr) ? m_float.get() : m_owner;
    bool          changed  = false;
    bool          animated = false;



    if (m_toolbar == nullptr)
    {
        return;
    }

    changed = lifted != m_toolbar->IsLifted();

    if (changed)
    {
        animated = m_animations.has_value() ? *m_animations : DxuiSystemSettings::Instance().AreAnimationsEnabled();
        m_toolbar->SetLifted (lifted, now, animated);
    }

    //  A toolbar carried from one window to the other is raised again in
    //  the window it is now in.
    if (window != nullptr && (lifted || changed))
    {
        window->SetRaisedChild (lifted ? m_toolbar : nullptr);
    }

    if (window != nullptr && (changed || m_toolbar->IsLiftSettling (now)))
    {
        window->Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::FitFloatWindow
//
//  The window stays as long as the toolbar's entries need, so the toolbar
//  never falls back on See more while it floats: a length measured before
//  the window had its DPI, or before a theme or entry change, comes right
//  here. The window's frame comes on top of the toolbar's length. A toolbar
//  that fills its edge is as long as its window has been made instead, and
//  only its thickness comes right.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::FitFloatWindow()
{
    HWND  hwnd    = m_float->GetHwnd();
    RECT  client  = {};
    RECT  current = m_float->GetScreenRect();
    RECT  wanted  = {};
    int   frameX  = 0;
    int   frameY  = 0;
    int   length  = 0;



    if (!GetClientRect (hwnd, &client))
    {
        return;
    }

    frameX = (current.right - current.left) - (client.right - client.left);
    frameY = (current.bottom - current.top) - (client.bottom - client.top);

    if (m_fillsEdge)
    {
        length = AdoptFloatLength (current) - (m_dock.floatVertical ? frameY : frameX);
    }

    wanted = GetFloatingRect (POINT { current.left, current.top }, length);

    if (wanted.right - wanted.left + frameX != current.right - current.left || wanted.bottom - wanted.top + frameY != current.bottom - current.top)
    {
        m_float->SetScreenRect (RECT { current.left, current.top, current.left + wanted.right - wanted.left + frameX, current.top + wanted.bottom - wanted.top + frameY });
        GetClientRect (hwnd, &client);
    }

    //  Measuring planned the toolbar for any length, so it is laid out
    //  again for the window it is in, between the ends a resizable window
    //  keeps for itself.
    m_toolbar->Layout (m_float->GetToolbarBounds (client), m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::AdoptFloatLength
//
//  The floating window's length in pixels, frame and all, as its ends were
//  last dragged to, and never shorter than the band is thick. The window
//  is created at the length kept, so the length holds from one run to the
//  next. A new length goes into the place and is saved, with the window's
//  top left, which a drag of its leading end moves.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarHost::AdoptFloatLength (const RECT & window)
{
    int  dpi       = (int) m_scaler.GetDpi();
    int  lengthPx  = m_dock.floatVertical ? window.bottom - window.top : window.right - window.left;
    int  lengthDip = 0;



    lengthPx  = (std::max) (lengthPx, m_scaler.ToPx (GetBandDipOfBar()));
    lengthDip = MulDiv (lengthPx, USER_DEFAULT_SCREEN_DPI, (std::max) (dpi, 1));

    if (lengthDip != m_dock.floatLengthDip)
    {
        m_dock.floatLengthDip = lengthDip;
        m_dock.floatPx        = POINT { window.left, window.top };

        Save();
    }

    return lengthPx;
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
    POINT            client    = screenPx;
    RECT             rect      = (m_float != nullptr) ? m_float->GetScreenRect() : RECT {};
    POINT            grab      = POINT { screenPx.x - rect.left, screenPx.y - rect.top };
    DxuiToolbarDock  dock;
    bool             isNewBand = false;
    bool             isDocked  = false;



    ScreenToClient (GetOwnerHwnd(), &client);

    m_hasLastFloat = false;

    if (m_snapping)
    {
        FinishSnap();
        return;
    }

    //  Across the top or bottom the toolbar starts a margin in from the
    //  window's edge, so the grab point counts it.
    isDocked = IsWindowVisible (GetOwnerHwnd()) && TryPickDrop (client, POINT { grab.x + m_scaler.ToPx (kMarginDp), grab.x }, dock, isNewBand);

    if (isDocked)
    {
        TakeDrop (dock, isNewBand);
        return;
    }

    m_dock.floating = true;
    m_dock.floatPx  = POINT { rect.left, rect.top };

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
    m_hasLastFloat = false;

    if (!m_snapping)
    {
        return;
    }

    m_snapping   = false;
    m_snapDragOn = m_snapResumes;

    TakeDrop (m_snapDock, m_snapNewBand);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::RunMoveLoopFrame
//
//  The OS owns the thread while the floating window is dragged, so the
//  owner's frames run from here, as every owner's do: its own tick unless
//  it set another.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarHost::RunMoveLoopFrame()
{
    if (m_onMoveFrame)
    {
        m_onMoveFrame();
    }
    else if (m_owner != nullptr)
    {
        m_owner->RunModalLoopTick();
    }
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
    bool   isNear   = false;
    POINT  grab     = {};



    if (m_float == nullptr || !IsWindowVisible (GetOwnerHwnd()))
    {
        return;
    }

    ScreenToClient (GetOwnerHwnd(), &client);

    if (m_snapping)
    {
        return;
    }

    rect   = m_float->GetScreenRect();
    grab   = DxuiToolbarDock::GrabForDocking (POINT { screenPx.x - rect.left, screenPx.y - rect.top }, m_dock.floatVertical, m_scaler.ToPx (kMarginDp));
    isNear = TryPickDrop (client, grab, m_snapDock, m_snapNewBand);

    if (isNear)
    {
        m_grab        = grab;
        m_snapResumes = true;
        m_snapping    = true;

        ReleaseCapture();
        return;
    }

    if (TrySnapFarEnd (rect))
    {
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

    m_float->SetScreenRect (GetFloatingRect (m_dock.floatPx, GetFloatLengthPx (m_dock, m_fillsEdge, (int) m_scaler.GetDpi())));
    m_float->Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost::TrySnapFarEnd
//
//  A floating toolbar whose far end, the end away from its grab handle, is
//  dragged up to the edge across from it snaps into that edge, as one held
//  near an edge by its handle does. A long toolbar such as a strip of
//  pictures reaches the right edge, or standing up the bottom, with that
//  end while the pointer is still far away at the handle, so the band
//  there must answer to the toolbar, not only to the pointer. The pointer
//  is not in that band, so the drag ends with the snap rather than going on
//  along the band, which would pull it straight out again.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarHost::TrySnapFarEnd (const RECT & screenRect)
{
    POINT                  origin   = {};
    RECT                   rect     = screenRect;
    RECT                   previous = {};
    RECT                   inner    = {};
    int                    reach    = m_scaler.ToPx (GetBandDipOfBar() + kDockReachDp);
    int                    offset   = 0;
    bool                   isNear   = false;
    DxuiToolbarDock::Edge  edge     = DxuiToolbarDock::Edge::Right;



    ScreenToClient (GetOwnerHwnd(), &origin);
    OffsetRect     (&rect, origin.x, origin.y);

    previous       = m_hasLastFloat ? m_lastFloat : rect;
    m_lastFloat    = rect;
    m_hasLastFloat = true;

    //  The far end meets the inner side of the bands already along that
    //  edge, and the toolbar takes a new band there.
    inner         = m_area;
    inner.right  -= m_group->GetDepthPx (DxuiToolbarDock::Edge::Right);
    inner.bottom -= m_group->GetDepthPx (DxuiToolbarDock::Edge::Bottom);

    isNear = DxuiToolbarDock::TryGetFarEndEdge (rect, previous, m_dock.floatVertical, inner, reach, edge);

    if (!isNear)
    {
        return false;
    }

    offset          = (edge == DxuiToolbarDock::Edge::Right) ? rect.top - m_group->GetEdgeArea (edge).top : rect.left - m_area.left - m_scaler.ToPx (kMarginDp);
    m_snapDock      = DxuiToolbarDock::MakeDocked (edge, offset, (int) m_scaler.GetDpi());
    m_snapDock.band = (int) m_group->GetBandThicknesses (edge).size();
    m_snapNewBand   = true;
    m_snapResumes   = false;
    m_snapping      = true;

    return true;
}





