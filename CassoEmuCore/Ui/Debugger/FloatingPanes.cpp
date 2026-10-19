#include "Pch.h"

#include "Ui/Debugger/FloatingPanes.h"

#include "Ui/Debugger/DebuggerLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::IsRoutable
//
//  An event from the debugger window is routed to the controls in that
//  window, one from a floating window only to the controls of its pane.
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingPanes::IsRoutable (const std::wstring & pane) const
{
    if (m_routingPane.empty())
    {
        return pane.empty() || !m_windows.contains (pane);
    }

    return pane == m_routingPane;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetBarRoutingPane
//
//  The pane's own while it floats, and none while it is in the debugger
//  window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FloatingPanes::GetBarRoutingPane (const std::wstring & pane) const
{
    return (!pane.empty() && m_windows.contains (pane)) ? pane : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetRoutedPopupHost
//
////////////////////////////////////////////////////////////////////////////////

DxuiHwndSource * FloatingPanes::GetRoutedPopupHost() const
{
    DxuiDockedWindow  * window = Find (m_routingPane);



    return (window != nullptr) ? window->GetPopupHost() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetRoutedTip
//
//  A tooltip is drawn only over its own window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltip * FloatingPanes::GetRoutedTip() const
{
    return m_routingPane.empty() ? nullptr : FindTip (m_routingPane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetRoutedHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND FloatingPanes::GetRoutedHwnd() const
{
    DxuiDockedWindow  * window = Find (m_routingPane);



    return (window != nullptr) ? window->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetRoutedFocus
//
//  A floating window has a focus of its own: the control last pressed in it.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * FloatingPanes::GetRoutedFocus() const
{
    auto  found = m_focus.find (m_routingPane);



    return (found != m_focus.end()) ? found->second : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::SetRoutedFocus
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::SetRoutedFocus (IDxuiControl * control)
{
    IDxuiControl  * old = GetRoutedFocus();



    if (old == control)
    {
        return;
    }

    if (old != nullptr)
    {
        old->OnFocusChanged (false);
    }

    m_focus[m_routingPane] = control;

    if (control != nullptr)
    {
        control->OnFocusChanged (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::ForgetFocus
//
//  Clear the focus of each floating window whose focused control is among
//  `controls`.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::ForgetFocus (const std::vector<IDxuiControl *> & controls)
{
    for (auto it = m_focus.begin(); it != m_focus.end(); )
    {
        if (std::ranges::count (controls, it->second) > 0)
        {
            it->second->OnFocusChanged (false);
            it = m_focus.erase (it);
            continue;
        }

        ++it;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Find
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockedWindow * FloatingPanes::Find (const std::wstring & pane) const
{
    auto  found = m_windows.find (pane);



    return (found != m_windows.end()) ? found->second.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::FindSite
//
////////////////////////////////////////////////////////////////////////////////

DxuiDockSite * FloatingPanes::FindSite (const std::wstring & pane) const
{
    DxuiDockedWindow  * window = Find (pane);



    return (window != nullptr) ? &window->GetSite() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Sync
//
//  Makes the floating windows match the layout: a window for each floating
//  pane and none for the rest, each visible while the debugger window and
//  its pane are. Run from the frame, never from inside a floating window's
//  own message, which may be for the window about to be destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Sync()
{
    const DxuiPaneLayout &  layout  = m_host.GetFloatDockSite()->GetPaneLayout();
    bool                    visible = IsWindowVisible (m_host.GetFloatOwner()) != FALSE;



    if (m_isSyncNeeded)
    {
        m_isSyncNeeded = false;

        for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
        {
            if (layout.IsFloating (pane) && !m_windows.contains (pane))
            {
                Float (pane);
            }
            else if (!layout.IsFloating (pane) && m_windows.contains (pane))
            {
                Dock (pane);
            }
        }

        m_host.OnFloatsSynced();
    }

    for (const auto & entry : m_windows)
    {
        bool  shown = visible && m_host.IsPaneShown (entry.first);

        if ((IsWindowVisible (entry.second->GetHwnd()) != FALSE) != shown)
        {
            if (shown)
            {
                entry.second->Show (false);
            }
            else
            {
                entry.second->Hide();
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::RequestFloat
//
//  A pane dropped outside the dock site, or floated from its menu, floats in
//  a window of the size it had, under the cursor. A floating pane dragged
//  somewhere with no zone stays where it was put.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::RequestFloat (const std::wstring & pane, POINT clientPx)
{
    const DxuiDpiScaler  & scaler  = m_host.GetFloatScaler();
    DxuiDockSite         & site    = *m_host.GetFloatDockSite();
    DxuiDockedWindow     * window  = Find (pane);
    IDxuiControl         * content = m_host.GetPaneContent (pane);
    RECT                   bounds  = (content != nullptr) ? content->GetBounds() : RECT {};
    int                    width   = std::max ((int) (bounds.right - bounds.left), scaler.ToPx (320));
    int                    height  = std::max ((int) (bounds.bottom - bounds.top), scaler.ToPx (220)) + scaler.ToPx (60);
    POINT                  screen  = clientPx;
    RECT                   rect    = {};



    if (window != nullptr)
    {
        rect = window->GetScreenRect();
    }
    else
    {
        ClientToScreen (m_host.GetFloatOwner(), &screen);
        rect = RECT { screen.x - width / 2, screen.y - scaler.ToPx (16), screen.x + width / 2, screen.y - scaler.ToPx (16) + height };
    }

    if (site.EditPaneLayout().Float (pane, GetMonitorKey (rect), rect))
    {
        site.Relayout();
        m_isSyncNeeded = true;
        m_host.SaveLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::TearOff
//
//  A pane's tab dragged off its strip, or a lone pane's title bar dragged,
//  floats the pane at once under the cursor. The mouse capture is released,
//  so that the rest of the drag moves the floating window once the frame
//  creates it.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::TearOff (const std::wstring & pane, POINT clientPx)
{
    RequestFloat (pane, clientPx);

    if (m_host.GetFloatDockSite()->GetPaneLayout().IsFloating (pane))
    {
        m_tornOffPane = pane;
        ReleaseCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::CarryTornOff
//
//  Start a title-bar drag of the floating window a pane was just torn off
//  into, while the mouse button that started the tear-off is down. The
//  system's move loop then draws the debugger window's drop zones, and the
//  pane is docked where the button is released, as for a drag of any
//  floating window.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::CarryTornOff()
{
    std::wstring  pane       = std::move (m_tornOffPane);
    bool          buttonDown = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    HWND          hwnd       = nullptr;



    m_tornOffPane.clear();

    if (pane.empty() || !buttonDown || !m_windows.contains (pane))
    {
        return;
    }

    hwnd = m_windows[pane]->GetHwnd();

    if (hwnd != nullptr && IsWindowVisible (hwnd))
    {
        PlaceUnderGrab (pane);
        PostMessage (hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::DockBack
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::DockBack (const std::wstring & pane)
{
    if (m_host.GetFloatDockSite()->EditPaneLayout().DockBack (pane))
    {
        m_isSyncNeeded = true;
        m_host.SaveLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Show
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Show (const std::wstring & pane)
{
    DxuiDockedWindow  * window = Find (pane);



    if (window != nullptr && window->GetHwnd() != nullptr)
    {
        window->Show (true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::SaveRects
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::SaveRects (DxuiPaneLayout & layout) const
{
    for (const auto & entry : m_windows)
    {
        RECT  rect = entry.second->GetScreenRect();

        //  A window whose pane just docked is here until the next frame;
        //  floating it again would undo the dock.
        if (layout.IsFloating (entry.first))
        {
            (void) layout.Float (entry.first, GetMonitorKey (rect), rect);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::EraseTip
//
//  A floating window's tooltip goes with the window.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::EraseTip (const std::wstring & key)
{
    auto  found = m_tips.find (key);



    if (found != m_tips.end())
    {
        m_host.OnFloatTipErased (found->second.get());
    }

    m_tips.erase (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::FindTip
//
////////////////////////////////////////////////////////////////////////////////

DxuiTooltip * FloatingPanes::FindTip (const std::wstring & key) const
{
    auto  found = m_tips.find (key);



    return (found != m_tips.end()) ? found->second.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::ApplyTheme (const DxuiTheme * theme)
{
    if (theme != nullptr)
    {
        for (const auto & entry : m_tips)
        {
            entry.second->SetTheme (*theme);
        }
    }

    for (const auto & entry : m_windows)
    {
        entry.second->SetTheme (theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::ApplyKeyMap
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::ApplyKeyMap (const DxuiKeyMap * map)
{
    for (const auto & entry : m_windows)
    {
        entry.second->SetKeyMap (map);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::HideAll
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::HideAll()
{
    for (const auto & entry : m_windows)
    {
        entry.second->Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::PollCaptionDrags
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::PollCaptionDrags()
{
    for (const auto & entry : m_windows)
    {
        entry.second->PollCaptionDrag();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::InvalidateAll
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::InvalidateAll()
{
    for (const auto & entry : m_windows)
    {
        entry.second->Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Tick
//
//  Each floating window's context menu and tooltip run on the ticks the
//  debugger window supplies, as its own do; without them a menu stays at
//  the first frame of its reveal, one line high.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Tick (int64_t now)
{
    RECT  client     = {};
    bool  isMenuOpen = IsAnyMenuOpen();



    for (const auto & [pane, window] : m_windows)
    {
        DxuiHwndSource  * host = window->GetPopupHost();
        DxuiTooltip     * tip  = FindTip (pane);

        if (host != nullptr && host->GetContextMenu().WantsTick())
        {
            host->GetContextMenu().Tick (now);
        }

        if (tip == nullptr)
        {
            continue;
        }

        if (window->GetHwnd() != nullptr && GetClientRect (window->GetHwnd(), &client))
        {
            tip->SetDpi          (GetDpiForWindow (window->GetHwnd()));
            tip->SetViewportSize (client.right - client.left, client.bottom - client.top);
        }

        if (isMenuOpen)
        {
            tip->HideImmediate();
        }
        else if (tip->WantsTick())
        {
            tip->Tick (now);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::SetFocusedLooks
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::SetFocusedLooks (const std::wstring & focusedPane, const DxuiTheme & theme)
{
    for (const auto & entry : m_windows)
    {
        entry.second->SetFocusedLook (entry.first == focusedPane, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::IsAnyMenuOpen
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingPanes::IsAnyMenuOpen() const
{
    bool  open = false;



    for (const auto & [pane, window] : m_windows)
    {
        DxuiHwndSource  * popups = (window != nullptr) ? window->GetPopupHost() : nullptr;

        open = open || (popups != nullptr && popups->GetContextMenu().IsVisible());
    }

    return open;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetMonitorKey
//
//  A monitor by its device name, which stays the same while it is attached
//  where it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FloatingPanes::GetMonitorKey (const RECT & rectPx)
{
    HMONITOR        monitor = MonitorFromRect (&rectPx, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW  info    = {};



    info.cbSize = sizeof (info);

    if (monitor == nullptr || !GetMonitorInfoW (monitor, &info))
    {
        return L"";
    }

    return info.szDevice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Float
//
//  A window for one floating pane, at the place the layout has for it,
//  with the pane's controls moved into it.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Float (const std::wstring & pane)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::make_unique<DxuiDockedWindow>();
    DxuiWindow::CreateParams           params;
    RECT                               rect   = {};
    HRESULT                            hr     = S_OK;
    DxuiDockSite                     & site   = *m_host.GetFloatDockSite();
    const DxuiTheme                  * theme  = m_host.GetTheme();



    for (const DxuiPaneLayout::Floating & floating : site.GetPaneLayout().GetFloating())
    {
        rect = (floating.pane == pane) ? floating.rectDip : rect;
    }

    params.title            = m_host.GetPaneTitle (pane);
    params.hInstance        = m_host.GetFloatInstance();
    params.ownerHwnd        = m_host.GetFloatOwner();
    params.initialSizeDip   = { std::max (160L, rect.right - rect.left), std::max (120L, rect.bottom - rect.top) };
    params.minSizeDip       = { 160, 120 };
    params.resizable        = true;
    params.captionStyle     = DxuiCaptionStyle::None;
    params.createNoActivate = true;
    params.toolWindow       = true;

    //  Composited, so the row of a dragged tab can fade over a tab strip.
    params.composited       = true;

    hr = window->Create (params);

    if (FAILED (hr))
    {
        //  With no window to float in, the pane goes back where it was.
        (void) site.EditPaneLayout().DockBack (pane);
        return;
    }

    window->SetTheme  (theme);
    window->SetKeyMap (m_host.GetFloatKeyMap());

    for (IDxuiControl * control : m_host.GetPaneControls (pane))
    {
        std::unique_ptr<IDxuiControl>  owned = m_host.DetachFloatControl (control);

        if (owned != nullptr)
        {
            (void) window->AttachChild (std::move (owned));
        }
    }

    window->GetSite().AddPane       (pane, m_host.GetPaneTitle (pane), m_host.GetPaneContent (pane));
    window->GetSite().SetPaneLayout (DxuiPaneLayout::MakeSingle (pane));

    //  The pane's title bar is the window's only one: its menu is the pane's
    //  Dock To menu, opened in this window; its pin docks the pane back; and
    //  its close button closes the pane with its floating place saved, so the
    //  View menu opens it there again.
    window->GetSite().SetFloating    ([this] (const std::wstring & p) { DockBack (p); });
    window->GetSite().SetPaneGap     (DxuiDockSite::kPaneGapDip, 0);
    window->GetSite().SetOnPaneMenu  ([this, pane] (const std::wstring & p, POINT clientPx)
    {
        RouteScope  scope (*this, pane);

        m_host.ShowDockToMenu (p, clientPx);
    });
    window->GetSite().SetOnClosePane ([this] (const std::wstring & p) { m_host.ClosePane (p); },
                                      [this] (const std::wstring & p) { return m_host.CanClosePane (p); });

    window->SetOnContentMouse      ([this, pane] (const DxuiMouseEvent & ev) { return RouteMouse (pane, ev); });
    window->SetOnContentKey        ([this, pane] (const DxuiKeyEvent & ev)   { return RouteKey   (pane, ev); });
    window->SetOnMappedCommand     ([this]       (int commandId)             { return m_host.RunFloatedCommand (commandId); });
    window->SetOnCaptionDrag       ([this, pane] (POINT screen)              { OnDrag (pane, screen, false); });
    window->SetOnCaptionDragEnd    ([this, pane] (POINT screen)              { OnDrag (pane, screen, true);  });
    window->SetOnCaptionDragCancel ([this, pane]
    {
        DropCarriedTab (pane);
        SetFade (pane, false);
        m_host.GetFloatDockSite()->CancelDrag();
        HideDragMarks();
        m_host.Repaint();
    });
    window->SetOnClosed            ([this, pane]                             { m_host.ClosePane (pane); });
    window->SetOnFilesDropped      ([this] (const std::vector<std::wstring> & paths) { return m_host.DropFloatedFiles (paths); });
    window->SetAcceptsDroppedFiles (true);
    window->SetOnFocusChanged      ([this, pane] (bool focused)              { m_host.OnFloatFocusChanged (pane, focused); });

    if (rect.right > rect.left && rect.bottom > rect.top)
    {
        window->SetScreenRect (rect);
    }

    //  A tooltip is drawn only over its own window, so the pane's toolbar
    //  tips use a tooltip of this window's.
    {
        std::unique_ptr<DxuiTooltip>  tip = std::make_unique<DxuiTooltip>();

        tip->SetPopupHost (window->GetPopupHost());
        tip->SetTheme     (*theme);
        tip->SetDpi       (GetDpiForWindow (window->GetHwnd()));
        tip->ApplyVisualStudioLook();
        m_tips[pane] = std::move (tip);
    }

    m_windows[pane] = std::move (window);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Dock
//
//  The pane's controls come back to the debugger window and its floating
//  window goes.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Dock (const std::wstring & pane)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::move (m_windows[pane]);



    m_windows.erase (pane);
    m_focus.erase   (pane);

    if (m_tips.contains (pane))
    {
        m_tips[pane]->HideImmediate();
        EraseTip (pane);
    }

    for (IDxuiControl * control : m_host.GetPaneControls (pane))
    {
        std::unique_ptr<IDxuiControl>  owned = window->DetachChild (control);

        if (owned != nullptr)
        {
            m_host.AttachFloatControl (std::move (owned));
        }
    }

    m_host.OnFloatDocked (pane, window.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::PlaceUnderGrab
//
//  A pane torn off by its tab floats with that tab in a strip along the
//  window's bottom, and a pane torn off by its title bar with that title
//  bar; either way the window goes where the cursor is over the spot it
//  pressed, rather than centered on the cursor.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::PlaceUnderGrab (const std::wstring & pane)
{
    DxuiDockSite      & mainSite = *m_host.GetFloatDockSite();
    DxuiDockedWindow  & window   = *m_windows[pane];
    DxuiDockSite      & site     = window.GetSite();
    POINT               grab     = mainSite.GetTearOffGrab();
    RECT                grabbed  = {};
    RECT                rect     = window.GetScreenRect();
    POINT               anchor   = {};
    POINT               cursor   = {};



    if (mainSite.WasTearOffFromTab())
    {
        site.SetCarriedPane (pane);
        grabbed = site.GetCarriedTabRect();
    }
    else if (site.GetGroupCount() > 0)
    {
        grabbed = site.GetGroup (0)->GetTitleRect();
    }

    if (grabbed.right <= grabbed.left || !GetCursorPos (&cursor))
    {
        return;
    }

    anchor = POINT { grabbed.left + grab.x, grabbed.top + grab.y };
    ClientToScreen (window.GetHwnd(), &anchor);
    OffsetRect (&rect, cursor.x - anchor.x, cursor.y - anchor.y);
    window.SetScreenRect (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::RouteMouse
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingPanes::RouteMouse (const std::wstring & pane, const DxuiMouseEvent & ev)
{
    RouteScope  scope (*this, pane);



    return m_host.RouteFloatedMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::RouteKey
//
////////////////////////////////////////////////////////////////////////////////

bool FloatingPanes::RouteKey (const std::wstring & pane, const DxuiKeyEvent & ev)
{
    RouteScope  scope (*this, pane);



    return m_host.RouteFloatedKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::OnDrag
//
//  While a floating window is moved by its title bar, draw the debugger
//  window's drop zones under the cursor, and dock the pane on the zone where
//  it is released.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::OnDrag (const std::wstring & pane, POINT screenPx, bool ended)
{
    DxuiDockSite    & site   = *m_host.GetFloatDockSite();
    POINT             client = screenPx;
    RECT              area   = site.GetBounds();
    bool              inside = false;
    DxuiMouseEvent    ev;



    ScreenToClient (m_host.GetFloatOwner(), &client);

    if (ended)
    {
        DropCarriedTab (pane);
        SetFade        (pane, false);
    }

    inside = client.x >= area.left && client.x < area.right && client.y >= area.top && client.y < area.bottom;

    if (!ended)
    {
        if (!site.IsDragging())
        {
            site.BeginDrag (pane);
        }

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = client;
        (void) site.OnMouse (ev);
        SetFade (pane, site.GetStripTargetGroup() >= 0);
        ShowDragMarks();
        m_host.Repaint();
        return;
    }

    if (!site.IsDragging())
    {
        HideDragMarks();
        return;
    }

    //  Released outside the site, the pane stays floating where it was put;
    //  released over a zone it docks there, which the site reports.
    (void) site.EndDrag (inside ? client : POINT { area.right + 1, area.bottom + 1 });
    HideDragMarks();
    m_host.Repaint();

    if (!inside)
    {
        m_host.SaveLayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::ShowDragMarks
//
//  The drop targets of a floating window's drag, drawn in an overlay above
//  the floating windows, which would cover them in the debugger window's
//  own paint.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::ShowDragMarks()
{
    HRESULT          hr     = S_OK;
    RECT             client = {};
    POINT            origin = {};
    HWND             owner  = m_host.GetFloatOwner();
    DxuiDockSite   & site   = *m_host.GetFloatDockSite();



    BAIL_OUT_IF (m_host.GetTheme() == nullptr, S_OK);

    site.SetDragMarksDrawnElsewhere (true);

    GetClientRect  (owner, &client);
    ClientToScreen (owner, &origin);
    OffsetRect     (&client, origin.x, origin.y);

    hr = m_dragOverlay.Show (owner, client, site.GetDragMarks (*m_host.GetTheme()));
    CHRA (hr);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::HideDragMarks
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::HideDragMarks()
{
    m_dragOverlay.Hide();
    m_host.GetFloatDockSite()->SetDragMarksDrawnElsewhere (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::DropCarriedTab
//
//  Remove the tab strip from a torn-off pane that is floating once its drag
//  ends.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::DropCarriedTab (const std::wstring & pane)
{
    DxuiDockSite  * site = FindSite (pane);



    if (site != nullptr)
    {
        site->SetCarriedPane (L"");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::SetFade
//
//  Fade the row of a floating pane's tab or title while it is dragged over a
//  tab strip, so that the strip under it is visible.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::SetFade (const std::wstring & pane, bool on)
{
    DxuiDockedWindow  * window = Find (pane);



    if (window != nullptr)
    {
        window->SetHeaderFade (on);
    }
}
