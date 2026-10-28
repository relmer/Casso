#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::ExplorerTabManager
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTabManager::ExplorerTabManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::Create()
{
    m_tabs = m_context.window.CreateChild<DxuiTabStrip>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::Configure
//
//  The tabs are drawn as File Explorer's are, and the tab close glyph is
//  Segoe Fluent Icons', thinner than MDL2's.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::Configure()
{
    m_tabs->SetStyle         (DxuiTabStripStyle::Explorer);
    m_tabs->SetNewTabGapDip  (CassoExplorerWindow::kNewTabGapDip);
    m_tabs->SetIconFace      (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                              ? DxuiToolbar::kFluentIconFace
                              : DxuiToolbar::kMdl2IconFace);

    m_tabs->SetOnChange ([this] (int index) { SwitchToTab ((size_t) index); });
    m_tabs->SetOnMove   ([this] (int from, int to)
    {
        bool  moved = m_context.browser.MoveTab ((size_t) from, (size_t) to);

        IGNORE_RETURN_VALUE (moved, true);
        m_context.window.SaveSession();
    });
    m_tabs->SetOnDragMove ([this] (int index, POINT point, bool tornOff) { HandleTabDragMove (index, point, tornOff); });
    m_tabs->SetOnDragEnd  ([this] (int index, POINT point, bool tornOff) { HandleTabDragEnd  (index, point, tornOff); });
    m_tabs->SetOnNewTab   ([this]() { m_context.commandManager.Dispatch (CassoExplorerCommands::kNewTab); });
    m_tabs->SetOnClose    ([this] (int index) { CloseTabAt ((size_t) index); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::ApplyTheme
//
//  The tabs are on the caption's color, as Explorer's are, and the selected
//  one joins the row below it, the menu bar's strip.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::ApplyTheme (const DxuiTheme & theme)
{
    if (m_tabs == nullptr)
    {
        return;
    }

    m_tabs->SetStripFill    (theme.titleBarTop);
    m_tabs->SetSelectedFill (theme.navStrip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::Layout (const RECT & rect)
{
    m_tabs->Layout (rect, m_context.scaler);
    FillTabs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::ClearHoverOnLeave
//
//  A pointer leaving the strip by any edge, or the window, ends the tab's
//  hover.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::ClearHoverOnLeave (const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  away = ev;



    if (m_tabs->GetHoverIndex() < 0 || m_tabs->IsInteracting())
    {
        return;
    }

    if ((ev.kind != DxuiMouseEventKind::Move || ExplorerLayout::Contains (m_tabs->GetBounds(), ev.positionDip)) && ev.kind != DxuiMouseEventKind::Leave)
    {
        return;
    }

    away.kind        = DxuiMouseEventKind::Move;
    away.positionDip = { -1, -1 };

    m_tabs->OnMouse (away);
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleCapturedMouse
//
//  A tab being dragged has the pointer until the button comes up, so the
//  drag can run past the strip's ends and scroll it. A move with the button
//  already up means the release went elsewhere, as when another window took
//  the mouse, so the drag is canceled.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTabManager::HandleCapturedMouse (const DxuiMouseEvent & ev)
{
    if (m_tabs->IsDragging() && ev.kind == DxuiMouseEventKind::Move && ev.button != DxuiMouseButton::Left)
    {
        CancelTabDrag();
        return true;
    }

    if (!m_tabs->IsInteracting())
    {
        return false;
    }

    m_tabs->OnMouse (ev);
    m_context.window.Invalidate();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleMouse
//
//  A right-click on a tab opens its menu, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTabManager::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   over  = ExplorerLayout::Contains (m_tabs->GetBounds(), point);



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right && over && m_tabs->HitTest (point.x, point.y) >= 0)
    {
        ShowTabContextMenu (point.x, point.y, m_tabs->HitTest (point.x, point.y));
        return true;
    }

    if (!over || !m_tabs->OnMouse (ev))
    {
        return false;
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleEscapeKey
//
//  Escape cancels a tab drag, as in File Explorer: the card goes, the gap
//  closes and the tab is back where it was.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTabManager::HandleEscapeKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down || ev.vk != VK_ESCAPE || !m_tabs->IsDragging())
    {
        return false;
    }

    CancelTabDrag();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HitTest
//
//  The tab under a point in the window's client area, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerTabManager::HitTest (POINT client) const
{
    return m_tabs->HitTest (client.x, client.y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::OpenInNewWindow
//
//  Another window of the app on the path, stepped down and right from this
//  one, as File Explorer's opens.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::OpenInNewWindow (const std::wstring & path)
{
    CassoExplorerWindowRequest  request;
    CassoExplorerWindow       * opened  = nullptr;



    if (path.empty() || m_context.app.host == nullptr)
    {
        return;
    }

    request.openPath  = path;
    request.placement = m_context.window.GetPlacement();
    request.cascade   = true;

    opened = m_context.app.host->OpenWindow (std::move (request));

    if (opened == nullptr)
    {
        m_context.window.ShowMessage (L"A new window could not be opened.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::OpenPathInNewTab
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::OpenPathInNewTab (const std::wstring & path)
{
    std::wstring  full       (MAX_PATH, L'\0');
    DWORD         length     = GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);
    DWORD         attributes = INVALID_FILE_ATTRIBUTES;



    //  The Recycle Bin, by its name or the shell's, in the tab that already
    //  shows it if there is one.
    if (_wcsicmp (path.c_str(), TreeModel::GetRootLabel (Location::kRecycleBinId).c_str()) == 0 || _wcsicmp (path.c_str(), L"shell:RecycleBinFolder") == 0)
    {
        for (size_t tab = 0; tab < m_context.browser.GetBrowserModel().GetTabCount(); tab++)
        {
            if (m_context.browser.GetBrowserModel().GetTab (tab).location.kind == Location::Kind::RecycleBin)
            {
                SwitchToTab (tab);
                return;
            }
        }

        m_context.browser.OpenInNewTab (Location::MakeRecycleBin());
        return;
    }

    //  Any other folder by the shell's name for it, such as shell:Libraries.
    if (Location::IsShellName (path))
    {
        Location  shellFolder = Location::MakeShellFolder (path, std::wstring());
        HRESULT   hr          = m_context.shellVerbs.GetShellItemName (path, shellFolder.label);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_context.browser.OpenInNewTab (shellFolder);
        return;
    }

    if (length >= full.size())
    {
        full.resize (length);
        length = GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);
    }

    if (length == 0 || length >= full.size())
    {
        return;
    }

    full.resize (length);

    //  A network path is not looked at, since an unreachable share would stop
    //  the window until the network times out: one that is not a disk image
    //  opens as a folder, and its list reports whatever its read finds.
    attributes = !PathResolver::IsNetworkPath (full) ? GetFileAttributesW (full.c_str())
               : TreeModel::IsSupportedImage (full)  ? FILE_ATTRIBUTE_NORMAL
               :                                       FILE_ATTRIBUTE_DIRECTORY;

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        m_context.browser.OpenInNewTab (Location::MakeHostFolder (full));
    }
    else if (TreeModel::IsSupportedImage (full))
    {
        m_context.browser.OpenInNewTab (Location::MakeDiskImage (full));
    }
    else if (m_context.browser.GetTreeModel().IsArchive (full))
    {
        m_context.browser.OpenInNewTab (Location::MakeShellFolder (full, std::filesystem::path (full).filename().wstring()));
    }
    else
    {
        m_context.browser.OpenInNewTab (Location::MakeHostFolder (CassoExplorerBrowser::GetParentFolder (full)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::ShowTabContextMenu
//
//  Explorer's tab menu. With one tab there are no others to close, and the
//  last tab has none to its right, so those rows are disabled rather than
//  doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::ShowTabContextMenu (int x, int y, int index)
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          tab   = (size_t) index;
    size_t                          count = m_context.browser.GetBrowserModel().GetTabCount();



    m_context.menus.GetMenuCommands().clear();

    m_context.menus.AddMenuCommand (items, L"&Close tab", [this, tab]() { CloseTabAt (tab); }, L"Ctrl+W");

    m_context.menus.AddMenuCommand (items, L"Close &other tabs", [this, tab]()
    {
        if (m_context.browser.CloseOtherTabs (tab))
        {
            m_context.listPane.FillList();
        }
    });

    m_context.menus.AddMenuCommand (items, L"Close tabs to the &right", [this, tab]()
    {
        if (m_context.browser.CloseTabsToRight (tab))
        {
            m_context.listPane.FillList();
        }
    });

    m_context.menus.AddMenuCommand (items, L"&Duplicate tab", [this, tab]()
    {
        m_context.browser.DuplicateTab (tab);
        m_context.listPane.FillList();
    });

    m_context.menus.GetMenuCommands()[1]->isEnabled = [count]() { return count > 1; };
    m_context.menus.GetMenuCommands()[2]->isEnabled = [tab, count]() { return tab + 1 < count; };

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::FillTabs
//
//  Tabs from the left of the strip, labeled by where each tab is. They share
//  the strip's width less the + button, no wider than a full tab and no
//  narrower than the minimum; past that the strip scrolls.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::FillTabs()
{
    const BrowserModel              & model = m_context.browser.GetBrowserModel();
    RECT                              strip = m_tabs->GetBounds();
    int                               width = m_context.scaler.ToPx (kTabWidthDip);
    int                               count = (int) model.GetTabCount();
    int                               lead  = (int) std::lround (m_context.scaler.ToPxf (kTabLeadDip));
    int                               plus  = m_context.scaler.ToPx (DxuiTabStrip::kNewTabWidthDip) + (int) std::lround (m_context.scaler.ToPxf (CassoExplorerWindow::kNewTabGapDip));
    std::vector<DxuiTabStrip::Tab>    tabs;
    size_t                            index = 0;



    if (count > 0)
    {
        width = std::clamp (((int) (strip.right - strip.left) - lead - plus) / count, m_context.scaler.ToPx (kTabMinWidthDip), width);
    }

    for (index = 0; index < model.GetTabCount(); index++)
    {
        DxuiTabStrip::Tab  tab;
        const Location &   location = model.GetTab (index).location;

        //  Tabs start below the strip's top, as Explorer's do, and reach its
        //  bottom, where the selected one joins the row below.
        tab.label = m_context.browser.GetTabLabel (index);
        tab.rect  = RECT { strip.left + lead + (int) index * width, strip.top + m_context.scaler.ToPx (kTabTopDip), strip.left + lead + (int) (index + 1) * width, strip.bottom };

        switch (location.kind)
        {
            case Location::Kind::None:          tab.icon = m_context.shellIcons.GetForKind (IShellIcons::Kind::ThisPc); break;
            case Location::Kind::Root:          tab.icon = m_context.shellIcons.GetForKind ((location.path == TreeModel::kCassoRootId) ? IShellIcons::Kind::Casso : (location.path == TreeModel::kCassoLinkRootId || location.path.starts_with (TreeModel::kLinkRootTag)) ? IShellIcons::Kind::AppleMachine : IShellIcons::Kind::ThisPc); break;
            case Location::Kind::RecycleBin:    tab.icon = m_context.shellIcons.GetForKind (IShellIcons::Kind::RecycleBin); break;
            case Location::Kind::DiskDirectory: tab.icon = m_context.shellIcons.GetForKind (IShellIcons::Kind::Folder); break;
            case Location::Kind::DiskImage:     tab.icon = m_context.shellIcons.GetForPath (location.path, false);     break;
            default:                            tab.icon = SearchQuery::IsId (location.path) ? m_context.shellIcons.GetForKind (IShellIcons::Kind::Search)
                                                                                         : m_context.shellIcons.GetForPath (location.path, true);
                                                break;
        }

        tabs.push_back (tab);
    }

    m_tabs->SetTabs (std::move (tabs));
    m_tabs->SetSelected ((int) model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::SwitchToTab
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::SwitchToTab (size_t index)
{
    //  An address being typed belongs to the tab it was typed in, so it is
    //  dropped, as in Explorer, and the new tab's address shows.
    if (m_context.addressBar.EndEdit())
    {
        if (m_context.input.GetFocusPane() == ExplorerPane::Address)
        {
            m_context.input.SetFocusPane (ExplorerPane::List);
        }
    }

    //  A tab left on a folder deleted since moves up as it is shown.
    if (m_context.browser.SwitchTab (index))
    {
        m_context.browser.LeaveMissingLocation();
        m_context.listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleTabDragMove
//
//  The card moves with the pointer from the first move of the drag. Torn
//  off, the tab may be over another window's strip, which opens a gap.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::HandleTabDragMove (int index, POINT client, bool tornOff)
{
    POINT  screen = client;
    POINT  grab   = m_tabs->GetDragGrab();
    BOOL   mapped = ClientToScreen (m_context.window.GetHwnd(), &screen);



    IGNORE_RETURN_VALUE (mapped, TRUE);

    if (m_tabDragVisual.IsShown())
    {
        m_tabDragVisual.MoveTo (POINT { screen.x - grab.x, screen.y - grab.y });
    }
    else
    {
        ShowTabDragVisual (index, screen);
    }

    SetTabDragTarget (tornOff ? FindTabStripAt (screen) : nullptr, screen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleTabDragEnd
//
//  A drop on this strip has already moved the tab. Torn off, it goes to the
//  strip of the window under the pointer, or into a window of its own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::HandleTabDragEnd (int index, POINT client, bool tornOff)
{
    POINT                  screen = client;
    BOOL                   mapped = ClientToScreen (m_context.window.GetHwnd(), &screen);
    CassoExplorerWindow  * target = nullptr;



    IGNORE_RETURN_VALUE (mapped, TRUE);

    m_tabDragVisual.Hide();

    if (tornOff && m_context.app.host != nullptr && m_tabDragTarget != nullptr)
    {
        target = m_context.app.host->GetWindowFor (m_tabDragTarget);
    }

    m_tabDragTarget = nullptr;

    if (target != nullptr)
    {
        target->GetTabManager().AcceptTab (m_context.browser.GetBrowserModel().GetTab ((size_t) index));
        CloseTabAt ((size_t) index);
    }
    else if (tornOff)
    {
        TearOffTab (index, screen);
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::CancelTabDrag
//
//  The window the tab was over closes its gap too. The strip has no press
//  left, so the release that follows does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::CancelTabDrag()
{
    m_tabs->CancelDrag();
    m_tabDragVisual.Hide();
    SetTabDragTarget (nullptr, POINT {});
    m_context.window.ArmTick();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::ShowTabDragVisual
//
//  The card is the tab as the strip draws the selected one, filled with the
//  row the selected tab joins. The Windows themes' row is a translucent wash
//  over Mica, and the card has no Mica behind it, so the wash is laid over
//  the theme's opaque background instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::ShowTabDragVisual (int index, POINT screen)
{
    HRESULT   hr   = S_OK;
    RECT      tab  = m_tabs->GetTabScreenRect (index);
    POINT     grab = m_tabs->GetDragGrab();
    uint32_t  text = m_context.theme->Foreground();
    uint32_t  wash = m_context.theme->navStrip;
    uint32_t  fill = DxuiColor::Mix (m_context.theme->Background() | 0xFF000000u, wash | 0xFF000000u, (float) (wash >> 24) / 255.0f);



    hr = m_tabDragVisual.Show (*m_context.window.GetPopupHost(),
                               SIZE { tab.right - tab.left, tab.bottom - tab.top },
                               POINT { screen.x - grab.x, screen.y - grab.y },
                               fill,
                               [this, index, text] (IDxuiPainter &, IDxuiTextRenderer & renderer, float width, float height)
                               {
                                   m_tabs->PaintFloatingTab (renderer, index, width, height, text);
                               });
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::FindTabStripAt
//
//  Another window of the app under the point, with the point over its tab
//  strip, or null. WindowFromPoint returns the card that moves with the
//  pointer, so when it does, the window under the card is found by walking
//  the z-order below it: the first one visible, not cloaked and not
//  click-through whose rectangle has the point.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * ExplorerTabManager::FindTabStripAt (POINT screen) const
{
    HWND                   under  = WindowFromPoint (screen);
    HWND                   root   = (under != nullptr) ? GetAncestor (under, GA_ROOT) : nullptr;
    HWND                   card   = m_tabDragVisual.GetHwnd();
    CassoExplorerWindow  * window = nullptr;



    if (root != nullptr && root == card)
    {
        root = FindWindowBelow (card, screen);
    }

    if (root != nullptr && root != m_context.window.GetHwnd() && m_context.app.host != nullptr)
    {
        window = m_context.app.host->GetWindowFor (root);
    }

    return (window != nullptr && window->GetTabManager().IsOverTabStrip (screen)) ? window : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::FindWindowBelow
//
//  The first top-level window below `above` in the z-order that is visible,
//  not cloaked (a window on another virtual desktop, or a suspended app's),
//  not click-through, and whose rectangle has the point; or null.
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerTabManager::FindWindowBelow (HWND above, POINT screen)
{
    HWND     next    = GetWindow (above, GW_HWNDNEXT);
    HWND     found   = nullptr;
    RECT     rect    = {};
    DWORD    cloaked = 0;
    HRESULT  hr      = S_OK;
    bool     passes  = false;



    for (; next != nullptr && found == nullptr; next = GetWindow (next, GW_HWNDNEXT))
    {
        cloaked = 0;
        hr      = DwmGetWindowAttribute (next, DWMWA_CLOAKED, &cloaked, sizeof (cloaked));
        passes  = !IsWindowVisible (next) || IsIconic (next) || (SUCCEEDED (hr) && cloaked != 0)
               || (GetWindowLongW (next, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0;

        if (!passes && GetWindowRect (next, &rect) && PtInRect (&rect, screen))
        {
            found = next;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::SetTabDragTarget
//
//  The window the tab was over hears that it left; the one it is over hears
//  where it is, on every move. A window is stored by its handle and found
//  again each time, so one closed during the drag is passed over.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::SetTabDragTarget (CassoExplorerWindow * target, POINT screen)
{
    HWND                   hwnd     = (target != nullptr) ? target->GetHwnd() : nullptr;
    CassoExplorerWindow  * previous = nullptr;



    if (m_tabDragTarget != nullptr && m_tabDragTarget != hwnd && m_context.app.host != nullptr)
    {
        previous = m_context.app.host->GetWindowFor (m_tabDragTarget);
    }

    if (previous != nullptr)
    {
        previous->GetTabManager().HandleTabHover (false, screen);
    }

    m_tabDragTarget = hwnd;

    if (target != nullptr)
    {
        target->GetTabManager().HandleTabHover (true, screen);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::GetTearOffRect
//
//  A torn-off tab's window is as big as this one, placed so the tab sits
//  where it was let go: its first tab under the card, which is under the
//  pointer where the drag took it.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerTabManager::GetTearOffRect (POINT screen) const
{
    WINDOWPLACEMENT  place  = { sizeof (place) };
    RECT             window = {};
    RECT             strip  = m_tabs->GetBounds();
    POINT            client = {};
    POINT            grab   = m_tabs->GetDragGrab();
    int              width  = 0;
    int              height = 0;
    int              left   = 0;
    int              top    = 0;



    if (!GetWindowRect (m_context.window.GetHwnd(), &window) || !ClientToScreen (m_context.window.GetHwnd(), &client))
    {
        return RECT {};
    }

    width  = window.right  - window.left;
    height = window.bottom - window.top;

    //  A maximized window's new one takes the size it would restore to.
    if (GetWindowPlacement (m_context.window.GetHwnd(), &place) && IsZoomed (m_context.window.GetHwnd()))
    {
        width  = place.rcNormalPosition.right  - place.rcNormalPosition.left;
        height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
    }

    left = screen.x - grab.x - (strip.left + (int) std::lround (m_context.scaler.ToPxf (kTabLeadDip))) - (client.x - window.left);
    top  = screen.y - grab.y - (strip.top  + m_context.scaler.ToPx (kTabTopDip))                       - (client.y - window.top);

    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::TearOffTab
//
//  Released away from every strip, the tab opens a window of its own. A
//  window's only tab takes the window with it instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::TearOffTab (int index, POINT screen)
{
    RECT                        rect    = GetTearOffRect (screen);
    BOOL                        moved   = FALSE;
    CassoExplorerWindowRequest  request;
    CassoExplorerWindow       * opened  = nullptr;



    if (rect.right <= rect.left || index < 0 || index >= (int) m_context.browser.GetBrowserModel().GetTabCount())
    {
        return;
    }

    if (m_context.browser.GetBrowserModel().GetTabCount() == 1)
    {
        if (IsZoomed (m_context.window.GetHwnd()))
        {
            moved = ShowWindow (m_context.window.GetHwnd(), SW_RESTORE);
            IGNORE_RETURN_VALUE (moved, TRUE);
        }

        moved = SetWindowPos (m_context.window.GetHwnd(), nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        IGNORE_RETURN_VALUE (moved, TRUE);
        m_context.window.SaveSession();
        return;
    }

    if (m_context.app.host == nullptr)
    {
        return;
    }

    request.tabs.push_back (m_context.browser.GetBrowserModel().GetTab ((size_t) index));
    request.screenRect    = rect;
    request.hasScreenRect = true;

    opened = m_context.app.host->OpenWindow (std::move (request));

    if (opened == nullptr)
    {
        m_context.window.ShowMessage (L"A new window could not be opened.", MB_ICONERROR);
        return;
    }

    CloseTabAt ((size_t) index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::CloseTabAt
//
//  A window left with no tabs closes. Its last tab stays in it until then,
//  since a window always has a location open.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::CloseTabAt (size_t index)
{
    if (m_context.browser.GetBrowserModel().GetTabCount() <= 1)
    {
        PostMessageW (m_context.window.GetHwnd(), WM_CLOSE, 0, 0);
        return;
    }

    if (m_context.browser.CloseTab (index))
    {
        m_context.listPane.FillList();
        m_context.window.SaveSession();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::IsOverTabStrip
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTabManager::IsOverTabStrip (POINT screen) const
{
    RECT   strip  = m_tabs->GetBounds();
    POINT  origin = {};



    if (m_context.window.GetHwnd() == nullptr || IsIconic (m_context.window.GetHwnd()) || !ClientToScreen (m_context.window.GetHwnd(), &origin))
    {
        return false;
    }

    OffsetRect (&strip, origin.x, origin.y);

    return PtInRect (&strip, screen) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::HandleTabHover
//
//  A tab from another window over this strip opens a gap as wide as this
//  window's own tabs, centered on the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::HandleTabHover (bool over, POINT screen)
{
    POINT  point  = screen;
    int    width  = m_context.scaler.ToPx (kTabWidthDip);
    BOOL   mapped = FALSE;



    if (!m_tabs->GetTabs().empty())
    {
        const RECT &  first = m_tabs->GetTabs().front().rect;

        width = first.right - first.left;
    }

    if (over)
    {
        mapped = ScreenToClient (m_context.window.GetHwnd(), &point);
        IGNORE_RETURN_VALUE (mapped, TRUE);

        m_tabs->SetIncomingDrag (point.x, width);
    }
    else
    {
        m_tabs->ClearIncomingDrag();
    }

    m_context.window.ArmTick();
    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager::AcceptTab
//
//  The tab goes in at the gap, or at the end with none, and the window comes
//  forward with it selected, as File Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabManager::AcceptTab (BrowserModel::Tab tab)
{
    int  index = m_tabs->GetIncomingIndex();



    m_tabs->ClearIncomingDrag();

    if (index < 0)
    {
        index = (int) m_context.browser.GetBrowserModel().GetTabCount();
    }

    m_context.browser.InsertTab ((size_t) index, std::move (tab));
    m_context.browser.LeaveMissingLocation();
    m_context.listPane.FillList();
    CassoExplorerShell::FrontWindow (m_context.window.GetHwnd());
    m_context.window.SaveSession();
}
