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
        return pane.empty() || !Contains (pane);
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
    return (!pane.empty() && Contains (pane)) ? pane : std::wstring();
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
    auto  found = m_focus.find (GetKeyOf (m_routingPane));



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

    m_focus[GetKeyOf (m_routingPane)] = control;

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
    auto  found = m_windows.find (GetKeyOf (pane));



    return (found != m_windows.end()) ? found->second.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetWindowOf
//
//  The key of the window a pane floats in, empty while it is in the
//  debugger window.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FloatingPanes::GetWindowOf (const std::wstring & pane) const
{
    for (const auto & [window, panes] : m_windowPanes)
    {
        if (std::ranges::find (panes, pane) != panes.end())
        {
            return window;
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetKeyOf
//
//  What a window's tip and focus are kept under: its key for a floating
//  pane, and anything else as given, a window's own key, a pane slid out
//  from an edge and the floating command bar's among them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FloatingPanes::GetKeyOf (const std::wstring & pane) const
{
    std::wstring  window = GetWindowOf (pane);



    return window.empty() ? pane : window;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetActivePane
//
//  The tab a window shows, as its site has it now.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FloatingPanes::GetActivePane (const std::wstring & window) const
{
    DxuiDockedWindow          * found = nullptr;
    const DxuiPaneLayoutNode  * root  = nullptr;
    auto                        panes = m_windowPanes.find (window);



    if (panes == m_windowPanes.end() || panes->second.empty())
    {
        return std::wstring();
    }

    found = m_windows.contains (window) ? m_windows.at (window).get() : nullptr;
    root  = (found != nullptr) ? found->GetSite().GetPaneLayout().GetRoot() : nullptr;

    if (root != nullptr && root->kind == DxuiPaneLayoutNode::Kind::Tabs &&
        root->active >= 0 && root->active < (int) root->panes.size())
    {
        return root->panes[(size_t) root->active];
    }

    return panes->second.front();
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
//  window the layout has, holding the panes it lists, and none for the rest,
//  each visible while the debugger window and any of its panes are. A window
//  whose panes changed, one torn out of it or a group dropped together, is
//  made again. Run from the frame, never from inside a floating window's own
//  message, which may be for the window about to be destroyed.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Sync()
{
    const DxuiPaneLayout &                             layout  = m_host.GetFloatDockSite()->GetPaneLayout();
    bool                                               visible = IsWindowVisible (m_host.GetFloatOwner()) != FALSE;
    std::map<std::wstring, std::vector<std::wstring>>  wanted;
    std::vector<std::wstring>                          stale;



    if (m_isSyncNeeded)
    {
        m_isSyncNeeded = false;

        for (const DxuiPaneLayout::Floating & floating : layout.GetFloating())
        {
            wanted[floating.window].push_back (floating.pane);
        }

        for (const auto & [window, panes] : m_windowPanes)
        {
            auto  found = wanted.find (window);

            if (found == wanted.end() || !std::ranges::is_permutation (found->second, panes))
            {
                stale.push_back (window);
            }
        }

        for (const std::wstring & window : stale)
        {
            Dock (window);
        }

        for (const auto & [window, panes] : wanted)
        {
            if (!m_windows.contains (window))
            {
                Float (window);
            }
        }

        m_host.OnFloatsSynced();
    }

    for (const auto & entry : m_windows)
    {
        const std::vector<std::wstring> &  panes = m_windowPanes[entry.first];
        bool                               shown = visible && std::ranges::any_of (panes, [this] (const std::wstring & pane) { return m_host.IsPaneShown (pane); });

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
    DxuiDockSite         & site    = *m_host.GetFloatDockSite();
    DxuiDockedWindow     * window  = Find (pane);
    RECT                   rect    = {};



    //  A pane alone in its window stays where the window is; one of several
    //  leaves them for a window of its own, the size theirs is, under the
    //  cursor.
    if (window != nullptr && site.GetPaneLayout().GetFloatingPanes (GetWindowOf (pane)).size() == 1)
    {
        rect = window->GetScreenRect();
    }
    else
    {
        rect = GetTearOffRect (pane, clientPx);
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
    m_tearGrab    = m_host.GetFloatDockSite()->GetTearOffGrab();
    m_tearFromTab = m_host.GetFloatDockSite()->WasTearOffFromTab();

    RequestFloat (pane, clientPx);

    if (m_host.GetFloatDockSite()->GetPaneLayout().IsFloating (pane))
    {
        m_tornOffPane = pane;
        ReleaseCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::TearOffGroup
//
//  A group's title bar dragged (FR-205): every pane of the group floats in
//  one window, at the size the group had docked, carried by the pointer as
//  a lone pane torn off is.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::TearOffGroup (const std::vector<std::wstring> & panes, const std::wstring & active, POINT clientPx)
{
    DxuiDockSite  & site = *m_host.GetFloatDockSite();
    RECT            rect = GetTearOffRect (active, clientPx);



    m_tearGrab    = site.GetTearOffGrab();
    m_tearFromTab = false;

    if (!site.EditPaneLayout().FloatGroup (panes, active, GetMonitorKey (rect), rect))
    {
        return;
    }

    site.Relayout();
    m_isSyncNeeded = true;
    m_host.SaveLayout();

    m_tornOffPane = active;
    ReleaseCapture();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::TearOut
//
//  A tab dragged off the strip of a floating window of several: that pane
//  floats alone, the size of the window it left, carried by the pointer.
//  The window it left is made again without it at the next Sync, from the
//  frame, since this runs inside that window's own mouse message.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::TearOut (const std::wstring & window, const std::wstring & pane, POINT windowPx)
{
    DxuiDockedWindow  * from   = m_windows.contains (window) ? m_windows.at (window).get() : nullptr;
    POINT               client = windowPx;



    if (from == nullptr || from->GetHwnd() == nullptr)
    {
        return;
    }

    m_tearGrab    = from->GetSite().GetTearOffGrab();
    m_tearFromTab = from->GetSite().WasTearOffFromTab();

    ClientToScreen (from->GetHwnd(), &client);
    ScreenToClient (m_host.GetFloatOwner(), &client);

    RequestFloat (pane, client);

    if (m_host.GetFloatDockSite()->GetPaneLayout().GetFloatingPanes (m_host.GetFloatDockSite()->GetPaneLayout().GetFloatingWindow (pane)).size() == 1)
    {
        m_tornOffPane = pane;
        ReleaseCapture();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::GetTearOffRect
//
//  A pane floats at the size its group had docked, title bar and tabs
//  included, since its window draws the same group (FR-205); a pane that
//  is not docked, or a group too small to work in, at least 320 by 220.
//  It is placed under the cursor here, and exactly under the spot pressed
//  once the window exists (PlaceUnderGrab).
//
////////////////////////////////////////////////////////////////////////////////

RECT FloatingPanes::GetTearOffRect (const std::wstring & pane, POINT clientPx) const
{
    const DxuiDpiScaler  & scaler  = m_host.GetFloatScaler();
    DxuiDockSite         & site    = *m_host.GetFloatDockSite();
    DxuiDockedWindow     * window  = Find (pane);
    IDxuiControl         * content = m_host.GetPaneContent (pane);
    RECT                   bounds  = {};
    POINT                  screen  = clientPx;
    int                    width   = 0;
    int                    height  = 0;



    if (window != nullptr)
    {
        bounds = window->GetScreenRect();
    }
    else if (site.TryGetPaneGroupRect (pane, bounds))
    {
        //  As the group is drawn in the site.
    }
    else if (content != nullptr)
    {
        bounds = content->GetBounds();
        bounds.bottom += scaler.ToPx (60);
    }

    width  = std::max ((int) (bounds.right  - bounds.left), scaler.ToPx (320));
    height = std::max ((int) (bounds.bottom - bounds.top),  scaler.ToPx (220));

    ClientToScreen (m_host.GetFloatOwner(), &screen);

    return RECT { screen.x - width / 2, screen.y - scaler.ToPx (16), screen.x + width / 2, screen.y - scaler.ToPx (16) + height };
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

    if (pane.empty() || !buttonDown || Find (pane) == nullptr)
    {
        return;
    }

    hwnd = Find (pane)->GetHwnd();

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
//  FloatingPanes::DockBackWindow
//
//  A window's pin docks every pane in it back, together: the first where its
//  group came from, the rest tabbed with it in the window's order, the one
//  it showed shown.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::DockBackWindow (const std::wstring & window)
{
    DxuiPaneLayout             & layout = m_host.GetFloatDockSite()->EditPaneLayout();
    std::vector<std::wstring>    panes  = layout.GetFloatingPanes (window);
    std::wstring                 active = GetActivePane (window);
    bool                         moved  = false;



    if (panes.empty())
    {
        return;
    }

    moved = layout.DockBack (panes.front());

    for (size_t i = 1; moved && i < panes.size(); i++)
    {
        (void) layout.TabWith (panes[i], panes.front());
    }

    if (moved)
    {
        (void) layout.Activate (active);
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
        //  Its tab in front, in a window of several.
        window->GetSite().ActivatePane (pane);
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
    for (const auto & [window, floating] : m_windows)
    {
        RECT                        rect  = floating->GetScreenRect();
        const DxuiPaneLayoutNode  * root  = floating->GetSite().GetPaneLayout().GetRoot();

        //  Setting a window's place adds nothing, so a window whose panes
        //  just docked, here until the next frame, cannot undo the dock.
        (void) layout.SetFloatingRect (window, GetMonitorKey (rect), rect);

        //  The tabs as the window has them: reordered along its strip, or
        //  another shown.
        if (root != nullptr && root->kind == DxuiPaneLayoutNode::Kind::Tabs && !root->panes.empty())
        {
            (void) layout.SetFloatingTabs (window, root->panes, GetActivePane (window));
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
    auto  found = m_tips.find (GetKeyOf (key));



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



    for (const auto & [key, window] : m_windows)
    {
        DxuiHwndSource  * host = window->GetPopupHost();
        DxuiTooltip     * tip  = FindTip (key);

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
        entry.second->SetFocusedLook (std::ranges::find (m_windowPanes.at (entry.first), focusedPane) != m_windowPanes.at (entry.first).end(), theme);
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



    for (const auto & [key, window] : m_windows)
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
//  A window for a floating window the layout has, at the place it has for
//  it, with the controls of every pane in it moved there and the panes as
//  tabs, the one it shows in front.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Float (const std::wstring & key)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::make_unique<DxuiDockedWindow>();
    DxuiWindow::CreateParams           params;
    RECT                               rect   = {};
    HRESULT                            hr     = S_OK;
    DxuiDockSite                     & site   = *m_host.GetFloatDockSite();
    const DxuiTheme                  * theme  = m_host.GetTheme();
    std::vector<std::wstring>          panes  = site.GetPaneLayout().GetFloatingPanes (key);
    std::wstring                       pane   = site.GetPaneLayout().GetFloatingActive (key);
    DxuiPaneLayout                     tabs;



    if (panes.empty())
    {
        return;
    }

    for (const DxuiPaneLayout::Floating & floating : site.GetPaneLayout().GetFloating())
    {
        rect = (floating.window == key) ? floating.rectDip : rect;
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
        //  With no window to float in, the panes go back where they were.
        for (const std::wstring & each : panes)
        {
            (void) site.EditPaneLayout().DockBack (each);
        }

        return;
    }

    window->SetTheme  (theme);
    window->SetKeyMap (m_host.GetFloatKeyMap());

    tabs = DxuiPaneLayout::MakeSingle (panes.front());

    for (const std::wstring & each : panes)
    {
        for (IDxuiControl * control : m_host.GetPaneControls (each))
        {
            std::unique_ptr<IDxuiControl>  owned = m_host.DetachFloatControl (control);

            if (owned != nullptr)
            {
                (void) window->AttachChild (std::move (owned));
            }
        }

        window->GetSite().AddPane (each, m_host.GetPaneTitle (each), m_host.GetPaneContent (each));

        if (each != panes.front())
        {
            (void) tabs.TabWithAt (each, panes.front(), (int) tabs.GetGroup (panes.front()).size());
        }
    }

    (void) tabs.Activate (pane);
    window->GetSite().SetPaneLayout (tabs);

    //  The group's title bar is the window's only one: its menu is the shown
    //  pane's Dock To menu, opened in this window; its pin docks every pane
    //  back; and its close button closes the shown pane with its floating
    //  place saved, so the View menu opens it there again. A tab dragged off
    //  the strip of a window of several floats that pane on its own.
    window->GetSite().SetFloating    ([this, key] (const std::wstring &) { DockBackWindow (key); });
    window->GetSite().SetPaneGap     (DxuiDockSite::kPaneGapDip, 0);
    window->GetSite().SetOnPaneMenu  ([this] (const std::wstring & p, POINT clientPx)
    {
        RouteScope  scope (*this, p);

        m_host.ShowDockToMenu (p, clientPx);
    });
    window->GetSite().SetOnClosePane ([this] (const std::wstring & p) { m_host.ClosePane (p); },
                                      [this] (const std::wstring & p) { return m_host.CanClosePane (p); });
    window->GetSite().SetOnTearOff   ([this, key] (const std::wstring & p, POINT windowPx) { TearOut (key, p, windowPx); });

    window->SetOnContentMouse      ([this, key] (const DxuiMouseEvent & ev) { return RouteMouse (GetActivePane (key), ev); });
    window->SetOnContentKey        ([this, key] (const DxuiKeyEvent & ev)   { return RouteKey   (GetActivePane (key), ev); });
    window->SetOnMappedCommand     ([this]      (int commandId)             { return m_host.RunFloatedCommand (commandId); });
    window->SetOnCaptionDrag       ([this, key] (POINT screen)              { OnDrag (key, screen, false); });
    window->SetOnCaptionDragEnd    ([this, key] (POINT screen)              { OnDrag (key, screen, true);  });
    window->SetOnCaptionDragCancel ([this, key]
    {
        DropCarriedTab (key);
        SetFade (key, false);
        m_host.GetFloatDockSite()->CancelDrag();
        HideDragMarks();
        m_host.Repaint();
    });
    window->SetOnClosed            ([this, key]
    {
        for (const std::wstring & each : m_windowPanes[key])
        {
            m_host.ClosePane (each);
        }
    });
    window->SetOnFilesDropped      ([this] (const std::vector<std::wstring> & paths) { return m_host.DropFloatedFiles (paths); });
    window->SetAcceptsDroppedFiles (true);
    window->SetOnFocusChanged      ([this, key] (bool focused)              { m_host.OnFloatFocusChanged (GetActivePane (key), focused); });

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
        m_tips[key] = std::move (tip);
    }

    m_windows[key]     = std::move (window);
    m_windowPanes[key] = std::move (panes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes::Dock
//
//  The controls of every pane in a floating window come back to the
//  debugger window and the window goes. A pane that is still floating, in
//  another window now, floats again at the next Sync.
//
////////////////////////////////////////////////////////////////////////////////

void FloatingPanes::Dock (const std::wstring & key)
{
    std::unique_ptr<DxuiDockedWindow>  window = std::move (m_windows[key]);
    std::vector<std::wstring>          panes  = std::move (m_windowPanes[key]);



    m_windows.erase     (key);
    m_windowPanes.erase (key);
    m_focus.erase       (key);

    if (m_tips.contains (key))
    {
        m_tips[key]->HideImmediate();
        EraseTip (key);
    }

    for (const std::wstring & pane : panes)
    {
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
    DxuiDockedWindow  & window   = *Find (pane);
    DxuiDockSite      & site     = window.GetSite();
    POINT               grab     = m_tearGrab;
    RECT                grabbed  = {};
    RECT                rect     = window.GetScreenRect();
    POINT               anchor   = {};
    POINT               cursor   = {};



    if (m_tearFromTab)
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

void FloatingPanes::OnDrag (const std::wstring & window, POINT screenPx, bool ended)
{
    const std::wstring  & pane   = window;
    DxuiDockSite        & site   = *m_host.GetFloatDockSite();
    POINT                 client = screenPx;
    RECT                  area   = site.GetBounds();
    bool                  inside = false;
    DxuiMouseEvent        ev;



    ScreenToClient (m_host.GetFloatOwner(), &client);

    if (ended)
    {
        DropCarriedTab (pane);
        SetFade        (pane, false);
    }

    inside = client.x >= area.left && client.x < area.right && client.y >= area.top && client.y < area.bottom;

    if (!ended)
    {
        //  Every pane in the window goes where it is dropped, together.
        if (!site.IsDragging())
        {
            site.BeginGroupDrag (m_windowPanes[window], GetActivePane (window));
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
