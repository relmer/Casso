#include "Pch.h"

#include "CassoExplorer/Window/ExplorerTabStrip.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Core/PathResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::ExplorerTabStrip
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTabStrip::ExplorerTabStrip (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::OpenInNewWindow
//
//  Another window of the app on the path, stepped down and right from this
//  one, as File Explorer's opens.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::OpenInNewWindow (const std::wstring & path)
{
    CassoExplorerWindowRequest  request;
    CassoExplorerWindow       * opened  = nullptr;



    if (path.empty() || m_window.m_context.host == nullptr)
    {
        return;
    }

    request.openPath  = path;
    request.placement = m_window.GetPlacement();
    request.cascade   = true;

    opened = m_window.m_context.host->OpenWindow (std::move (request));

    if (opened == nullptr)
    {
        m_window.ShowMessage (L"A new window could not be opened.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::OpenPathInNewTab
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::OpenPathInNewTab (const std::wstring & path)
{
    std::wstring  full       (MAX_PATH, L'\0');
    DWORD         length     = GetFullPathNameW (path.c_str(), (DWORD) full.size(), full.data(), nullptr);
    DWORD         attributes = INVALID_FILE_ATTRIBUTES;



    //  The Recycle Bin, by its name or the shell's, in the tab that already
    //  shows it if there is one.
    if (_wcsicmp (path.c_str(), TreeModel::GetRootLabel (Location::kRecycleBinId).c_str()) == 0 || _wcsicmp (path.c_str(), L"shell:RecycleBinFolder") == 0)
    {
        for (size_t tab = 0; tab < m_window.m_browser.GetBrowserModel().GetTabCount(); tab++)
        {
            if (m_window.m_browser.GetBrowserModel().GetTab (tab).location.kind == Location::Kind::RecycleBin)
            {
                SwitchToTab (tab);
                return;
            }
        }

        m_window.m_browser.OpenInNewTab (Location::MakeRecycleBin());
        return;
    }

    //  Any other folder by the shell's name for it, such as shell:Libraries.
    if (Location::IsShellName (path))
    {
        Location  shellFolder = Location::MakeShellFolder (path, std::wstring());
        HRESULT   hr          = m_window.m_shellVerbs.GetShellItemName (path, shellFolder.label);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_window.m_browser.OpenInNewTab (shellFolder);
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
        m_window.m_browser.OpenInNewTab (Location::MakeHostFolder (full));
    }
    else if (TreeModel::IsSupportedImage (full))
    {
        m_window.m_browser.OpenInNewTab (Location::MakeDiskImage (full));
    }
    else if (m_window.m_browser.GetTreeModel().IsArchive (full))
    {
        m_window.m_browser.OpenInNewTab (Location::MakeShellFolder (full, std::filesystem::path (full).filename().wstring()));
    }
    else
    {
        m_window.m_browser.OpenInNewTab (Location::MakeHostFolder (CassoExplorerBrowser::GetParentFolder (full)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::ShowTabContextMenu
//
//  Explorer's tab menu. With one tab there are no others to close, and the
//  last tab has none to its right, so those rows are disabled rather than
//  doing nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::ShowTabContextMenu (int x, int y, int index)
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          tab   = (size_t) index;
    size_t                          count = m_window.m_browser.GetBrowserModel().GetTabCount();



    m_window.m_menuCommands.clear();

    m_window.m_menus.AddMenuCommand (items, L"&Close tab", [this, tab]() { CloseTabAt (tab); }, L"Ctrl+W");

    m_window.m_menus.AddMenuCommand (items, L"Close &other tabs", [this, tab]()
    {
        if (m_window.m_browser.CloseOtherTabs (tab))
        {
            m_window.m_listPane.FillList();
        }
    });

    m_window.m_menus.AddMenuCommand (items, L"Close tabs to the &right", [this, tab]()
    {
        if (m_window.m_browser.CloseTabsToRight (tab))
        {
            m_window.m_listPane.FillList();
        }
    });

    m_window.m_menus.AddMenuCommand (items, L"&Duplicate tab", [this, tab]()
    {
        m_window.m_browser.DuplicateTab (tab);
        m_window.m_listPane.FillList();
    });

    m_window.m_menuCommands[1]->isEnabled = [count]() { return count > 1; };
    m_window.m_menuCommands[2]->isEnabled = [tab, count]() { return tab + 1 < count; };

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::FillTabs
//
//  Tabs from the left of the strip, labeled by where each tab is. They share
//  the strip's width less the + button, no wider than a full tab and no
//  narrower than the minimum; past that the strip scrolls.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::FillTabs()
{
    const BrowserModel              & model = m_window.m_browser.GetBrowserModel();
    RECT                              strip = m_window.m_tabs->GetBounds();
    int                               width = m_window.m_scaler.ToPx (kTabWidthDip);
    int                               count = (int) model.GetTabCount();
    int                               lead  = (int) std::lround (m_window.m_scaler.ToPxf (kTabLeadDip));
    int                               plus  = m_window.m_scaler.ToPx (DxuiTabStrip::kNewTabWidthDip) + (int) std::lround (m_window.m_scaler.ToPxf (CassoExplorerWindow::kNewTabGapDip));
    std::vector<DxuiTabStrip::Tab>    tabs;
    size_t                            index = 0;



    if (count > 0)
    {
        width = std::clamp (((int) (strip.right - strip.left) - lead - plus) / count, m_window.m_scaler.ToPx (kTabMinWidthDip), width);
    }

    for (index = 0; index < model.GetTabCount(); index++)
    {
        DxuiTabStrip::Tab  tab;
        const Location &   location = model.GetTab (index).location;

        //  Tabs start below the strip's top, as Explorer's do, and reach its
        //  bottom, where the selected one joins the row below.
        tab.label = m_window.m_browser.GetTabLabel (index);
        tab.rect  = RECT { strip.left + lead + (int) index * width, strip.top + m_window.m_scaler.ToPx (kTabTopDip), strip.left + lead + (int) (index + 1) * width, strip.bottom };

        switch (location.kind)
        {
            case Location::Kind::None:          tab.icon = m_window.m_shellIcons.GetForKind (IShellIcons::Kind::ThisPc); break;
            case Location::Kind::Root:          tab.icon = m_window.m_shellIcons.GetForKind ((location.path == TreeModel::kCassoRootId) ? IShellIcons::Kind::Casso : (location.path == TreeModel::kCassoLinkRootId || location.path.starts_with (TreeModel::kLinkRootTag)) ? IShellIcons::Kind::AppleMachine : IShellIcons::Kind::ThisPc); break;
            case Location::Kind::RecycleBin:    tab.icon = m_window.m_shellIcons.GetForKind (IShellIcons::Kind::RecycleBin); break;
            case Location::Kind::DiskDirectory: tab.icon = m_window.m_shellIcons.GetForKind (IShellIcons::Kind::Folder); break;
            case Location::Kind::DiskImage:     tab.icon = m_window.m_shellIcons.GetForPath (location.path, false);     break;
            default:                            tab.icon = SearchQuery::IsId (location.path) ? m_window.m_shellIcons.GetForKind (IShellIcons::Kind::Search)
                                                                                         : m_window.m_shellIcons.GetForPath (location.path, true);
                                                break;
        }

        tabs.push_back (tab);
    }

    m_window.m_tabs->SetTabs (std::move (tabs));
    m_window.m_tabs->SetSelected ((int) model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::SwitchToTab
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::SwitchToTab (size_t index)
{
    //  An address being typed belongs to the tab it was typed in, so it is
    //  dropped, as in Explorer, and the new tab's address shows.
    if (m_window.m_address->IsEditing())
    {
        m_window.m_address->EndEdit();

        if (m_window.m_focus == ExplorerInput::Pane::Address)
        {
            m_window.m_input.SetFocusPane (ExplorerInput::Pane::List);
        }
    }

    //  A tab left on a folder deleted since moves up as it is shown.
    if (m_window.m_browser.SwitchTab (index))
    {
        m_window.m_browser.LeaveMissingLocation();
        m_window.m_listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::OnTabDragMove
//
//  The card moves with the pointer from the first move of the drag. Torn
//  off, the tab may be over another window's strip, which opens a gap.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::OnTabDragMove (int index, POINT client, bool tornOff)
{
    POINT  screen = client;
    POINT  grab   = m_window.m_tabs->GetDragGrab();
    BOOL   mapped = ClientToScreen (m_window.GetHwnd(), &screen);



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
//  ExplorerTabStrip::OnTabDragEnd
//
//  A drop on this strip has already moved the tab. Torn off, it goes to the
//  strip of the window under the pointer, or into a window of its own.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::OnTabDragEnd (int index, POINT client, bool tornOff)
{
    POINT                  screen = client;
    BOOL                   mapped = ClientToScreen (m_window.GetHwnd(), &screen);
    CassoExplorerWindow  * target = nullptr;



    IGNORE_RETURN_VALUE (mapped, TRUE);

    m_tabDragVisual.Hide();

    if (tornOff && m_window.m_context.host != nullptr && m_tabDragTarget != nullptr)
    {
        target = m_window.m_context.host->GetWindowFor (m_tabDragTarget);
    }

    m_tabDragTarget = nullptr;

    if (target != nullptr)
    {
        target->m_tabStrip.AcceptTab (m_window.m_browser.GetBrowserModel().GetTab ((size_t) index));
        CloseTabAt ((size_t) index);
    }
    else if (tornOff)
    {
        TearOffTab (index, screen);
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::CancelTabDrag
//
//  The window the tab was over closes its gap too. The strip has no press
//  left, so the release that follows does nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::CancelTabDrag()
{
    m_window.m_tabs->CancelDrag();
    m_tabDragVisual.Hide();
    SetTabDragTarget (nullptr, POINT {});
    m_window.ArmTick();
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::ShowTabDragVisual
//
//  The card is the tab as the strip draws the selected one, filled with the
//  row the selected tab joins. The Windows themes' row is a translucent wash
//  over Mica, and the card has no Mica behind it, so the wash is laid over
//  the theme's opaque background instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::ShowTabDragVisual (int index, POINT screen)
{
    HRESULT   hr   = S_OK;
    RECT      tab  = m_window.m_tabs->GetTabScreenRect (index);
    POINT     grab = m_window.m_tabs->GetDragGrab();
    uint32_t  text = m_window.m_theme->Foreground();
    uint32_t  wash = m_window.m_theme->navStrip;
    uint32_t  fill = DxuiColor::Mix (m_window.m_theme->Background() | 0xFF000000u, wash | 0xFF000000u, (float) (wash >> 24) / 255.0f);



    hr = m_tabDragVisual.Show (*m_window.GetPopupHost(),
                               SIZE { tab.right - tab.left, tab.bottom - tab.top },
                               POINT { screen.x - grab.x, screen.y - grab.y },
                               fill,
                               [this, index, text] (IDxuiPainter &, IDxuiTextRenderer & renderer, float width, float height)
                               {
                                   m_window.m_tabs->PaintFloatingTab (renderer, index, width, height, text);
                               });
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::FindTabStripAt
//
//  Another window of the app under the point, with the point over its tab
//  strip, or null. WindowFromPoint returns the card that moves with the
//  pointer, so when it does, the window under the card is found by walking
//  the z-order below it: the first one visible, not cloaked and not
//  click-through whose rectangle has the point.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow * ExplorerTabStrip::FindTabStripAt (POINT screen) const
{
    HWND                   under  = WindowFromPoint (screen);
    HWND                   root   = (under != nullptr) ? GetAncestor (under, GA_ROOT) : nullptr;
    HWND                   card   = m_tabDragVisual.GetHwnd();
    CassoExplorerWindow  * window = nullptr;



    if (root != nullptr && root == card)
    {
        root = FindWindowBelow (card, screen);
    }

    if (root != nullptr && root != m_window.GetHwnd() && m_window.m_context.host != nullptr)
    {
        window = m_window.m_context.host->GetWindowFor (root);
    }

    return (window != nullptr && window->m_tabStrip.IsOverTabStrip (screen)) ? window : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::FindWindowBelow
//
//  The first top-level window below `above` in the z-order that is visible,
//  not cloaked (a window on another virtual desktop, or a suspended app's),
//  not click-through, and whose rectangle has the point; or null.
//
////////////////////////////////////////////////////////////////////////////////

HWND ExplorerTabStrip::FindWindowBelow (HWND above, POINT screen)
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
//  ExplorerTabStrip::SetTabDragTarget
//
//  The window the tab was over hears that it left; the one it is over hears
//  where it is, on every move. A window is stored by its handle and found
//  again each time, so one closed during the drag is passed over.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::SetTabDragTarget (CassoExplorerWindow * target, POINT screen)
{
    HWND                   hwnd     = (target != nullptr) ? target->GetHwnd() : nullptr;
    CassoExplorerWindow  * previous = nullptr;



    if (m_tabDragTarget != nullptr && m_tabDragTarget != hwnd && m_window.m_context.host != nullptr)
    {
        previous = m_window.m_context.host->GetWindowFor (m_tabDragTarget);
    }

    if (previous != nullptr)
    {
        previous->m_tabStrip.OnTabHover (false, screen);
    }

    m_tabDragTarget = hwnd;

    if (target != nullptr)
    {
        target->m_tabStrip.OnTabHover (true, screen);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::GetTearOffRect
//
//  A torn-off tab's window is as big as this one, placed so the tab sits
//  where it was let go: its first tab under the card, which is under the
//  pointer where the drag took it.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerTabStrip::GetTearOffRect (POINT screen) const
{
    WINDOWPLACEMENT  place  = { sizeof (place) };
    RECT             window = {};
    RECT             strip  = m_window.m_tabs->GetBounds();
    POINT            client = {};
    POINT            grab   = m_window.m_tabs->GetDragGrab();
    int              width  = 0;
    int              height = 0;
    int              left   = 0;
    int              top    = 0;



    if (!GetWindowRect (m_window.GetHwnd(), &window) || !ClientToScreen (m_window.GetHwnd(), &client))
    {
        return RECT {};
    }

    width  = window.right  - window.left;
    height = window.bottom - window.top;

    //  A maximized window's new one takes the size it would restore to.
    if (GetWindowPlacement (m_window.GetHwnd(), &place) && IsZoomed (m_window.GetHwnd()))
    {
        width  = place.rcNormalPosition.right  - place.rcNormalPosition.left;
        height = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
    }

    left = screen.x - grab.x - (strip.left + (int) std::lround (m_window.m_scaler.ToPxf (kTabLeadDip))) - (client.x - window.left);
    top  = screen.y - grab.y - (strip.top  + m_window.m_scaler.ToPx (kTabTopDip))                       - (client.y - window.top);

    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::TearOffTab
//
//  Released away from every strip, the tab opens a window of its own. A
//  window's only tab takes the window with it instead.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::TearOffTab (int index, POINT screen)
{
    RECT                        rect    = GetTearOffRect (screen);
    BOOL                        moved   = FALSE;
    CassoExplorerWindowRequest  request;
    CassoExplorerWindow       * opened  = nullptr;



    if (rect.right <= rect.left || index < 0 || index >= (int) m_window.m_browser.GetBrowserModel().GetTabCount())
    {
        return;
    }

    if (m_window.m_browser.GetBrowserModel().GetTabCount() == 1)
    {
        if (IsZoomed (m_window.GetHwnd()))
        {
            moved = ShowWindow (m_window.GetHwnd(), SW_RESTORE);
            IGNORE_RETURN_VALUE (moved, TRUE);
        }

        moved = SetWindowPos (m_window.GetHwnd(), nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        IGNORE_RETURN_VALUE (moved, TRUE);
        m_window.SaveSession();
        return;
    }

    if (m_window.m_context.host == nullptr)
    {
        return;
    }

    request.tabs.push_back (m_window.m_browser.GetBrowserModel().GetTab ((size_t) index));
    request.screenRect    = rect;
    request.hasScreenRect = true;

    opened = m_window.m_context.host->OpenWindow (std::move (request));

    if (opened == nullptr)
    {
        m_window.ShowMessage (L"A new window could not be opened.", MB_ICONERROR);
        return;
    }

    CloseTabAt ((size_t) index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::CloseTabAt
//
//  A window left with no tabs closes. Its last tab stays in it until then,
//  since a window always has a location open.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::CloseTabAt (size_t index)
{
    if (m_window.m_browser.GetBrowserModel().GetTabCount() <= 1)
    {
        PostMessageW (m_window.GetHwnd(), WM_CLOSE, 0, 0);
        return;
    }

    if (m_window.m_browser.CloseTab (index))
    {
        m_window.m_listPane.FillList();
        m_window.SaveSession();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::IsOverTabStrip
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerTabStrip::IsOverTabStrip (POINT screen) const
{
    RECT   strip  = m_window.m_tabs->GetBounds();
    POINT  origin = {};



    if (m_window.GetHwnd() == nullptr || IsIconic (m_window.GetHwnd()) || !ClientToScreen (m_window.GetHwnd(), &origin))
    {
        return false;
    }

    OffsetRect (&strip, origin.x, origin.y);

    return PtInRect (&strip, screen) != FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::OnTabHover
//
//  A tab from another window over this strip opens a gap as wide as this
//  window's own tabs, centered on the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::OnTabHover (bool over, POINT screen)
{
    POINT  point  = screen;
    int    width  = m_window.m_scaler.ToPx (kTabWidthDip);
    BOOL   mapped = FALSE;



    if (!m_window.m_tabs->GetTabs().empty())
    {
        const RECT &  first = m_window.m_tabs->GetTabs().front().rect;

        width = first.right - first.left;
    }

    if (over)
    {
        mapped = ScreenToClient (m_window.GetHwnd(), &point);
        IGNORE_RETURN_VALUE (mapped, TRUE);

        m_window.m_tabs->SetIncomingDrag (point.x, width);
    }
    else
    {
        m_window.m_tabs->ClearIncomingDrag();
    }

    m_window.ArmTick();
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip::AcceptTab
//
//  The tab goes in at the gap, or at the end with none, and the window comes
//  forward with it selected, as File Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerTabStrip::AcceptTab (BrowserModel::Tab tab)
{
    int  index = m_window.m_tabs->GetIncomingIndex();



    m_window.m_tabs->ClearIncomingDrag();

    if (index < 0)
    {
        index = (int) m_window.m_browser.GetBrowserModel().GetTabCount();
    }

    m_window.m_browser.InsertTab ((size_t) index, std::move (tab));
    m_window.m_browser.LeaveMissingLocation();
    m_window.m_listPane.FillList();
    CassoExplorerShell::FrontWindow (m_window.GetHwnd());
    m_window.SaveSession();
}
