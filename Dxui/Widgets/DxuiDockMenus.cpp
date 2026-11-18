#include "Pch.h"

#include "DxuiDockMenus.h"
#include "IDxuiDockMenuHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMenus::GetDockToMenu
//
//  Against each edge of the site, into each other shown group, and out into
//  a window of its own; or back, for a pane that is floating or hidden.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockMenus::MenuItem> DxuiDockMenus::GetDockToMenu (const std::wstring & pane)
{
    static constexpr DxuiDockSide  kSides[]    = { DxuiDockSide::Left, DxuiDockSide::Top, DxuiDockSide::Right, DxuiDockSide::Bottom };
    static const wchar_t * const   kNames[]    = { L"Dock left", L"Dock top", L"Dock right", L"Dock bottom" };
    std::vector<MenuItem>          items;
    std::vector<std::wstring>      mine        = m_host.EditMenuLayout().GetGroup (pane);
    auto                           commit      = [this] (bool changed) { return m_host.CommitMenuChange (changed); };



    if (!m_host.EditMenuLayout().Contains (pane))
    {
        return items;
    }

    if (!m_host.EditMenuLayout().IsDocked (pane))
    {
        items.push_back ({ L"Dock", [this, pane] { return m_host.UnpinFromMenu (pane); } });
        return items;
    }

    for (size_t i = 0; i < std::size (kSides); i++)
    {
        DxuiDockSide  side = kSides[i];

        items.push_back ({ kNames[i], [this, pane, side, commit] { return commit (m_host.EditMenuLayout().DockToEdge (pane, side)); } });
    }

    for (const std::wstring & target : m_host.GetActivePanes())
    {
        if (target.empty() || std::find (mine.begin(), mine.end(), target) != mine.end())
        {
            continue;
        }

        items.push_back ({ L"Tab with " + m_host.GetMenuPaneTitle (target),
                           [this, pane, target, commit] { return commit (m_host.EditMenuLayout().TabWith (pane, target)); } });
    }

    for (const DxuiPaneLayout::GroupRect & group : m_host.ArrangeForMenu())
    {
        RECT          area  = m_host.GetMenuDockedArea();
        long          gaps[] = { group.rect.left - area.left, group.rect.top - area.top,
                                 area.right - group.rect.right, area.bottom - group.rect.bottom };
        size_t        nearest = 0;

        if (std::find (group.panes.begin(), group.panes.end(), pane) == group.panes.end())
        {
            continue;
        }

        //  Against the edge the pane's group lies nearest. Between a side and
        //  the top or bottom it touches, a group no wider than half the
        //  window is in a side bar and hides to its side; a wider one hides
        //  to the top or bottom. So the top of the right-hand column hides
        //  right, and the memory row under the code hides to the bottom.
        bool   sideBar = 2 * (group.rect.right - group.rect.left) <= (area.right - area.left);

        for (size_t i = 1; i < std::size (gaps); i++)
        {
            bool  sideways = (i % 2) == 0;

            nearest = (gaps[i] < gaps[nearest] || (gaps[i] == gaps[nearest] && sideways == sideBar)) ? i : nearest;
        }

        items.push_back ({ kAutoHideLabel, [this, pane, nearest] { return m_host.PinFromMenu (pane, kSides[nearest]); } });
    }

    if (m_host.CanFloatFromMenu())
    {
        items.push_back ({ L"Float", [this, pane] { m_host.FloatFromMenu (pane); return true; } });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMenus::GetPaneMenu
//
//  Dock returns a floating or hidden pane to its place. Dock in tab group
//  tabs a tool window in with the documents. Auto hide pins the pane to the
//  edge its pin would. Move to new window floats the pane; All to new
//  window floats its whole group in one window, and the group's selected tab
//  is selected in the window. Close is the application's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockMenus::MenuItem> DxuiDockMenus::GetPaneMenu (const std::wstring & pane, bool fromTab)
{
    std::vector<MenuItem>          items;
    std::vector<MenuItem>          dockTo     = GetDockToMenu (pane);
    std::vector<std::wstring>      mine       = m_host.EditMenuLayout().GetGroup (pane);
    bool                           docked     = m_host.EditMenuLayout().IsDocked (pane);
    bool                           canFloat   = docked && m_host.CanFloatFromMenu() && !m_host.IsMenuFloatingSite();
    bool                           canClose   = m_host.CanCloseFromMenu (pane);
    IDxuiDockMenuHost::PaneTestFn  isDocument = m_host.GetMenuDocumentFn();
    std::wstring                   documents;
    MenuItem                       autoHide   = { kAutoHideLabel, nullptr, false };
    MenuItem                       dock       = { L"Dock", nullptr, false };
    auto                           commit     = [this] (bool changed) { return m_host.CommitMenuChange (changed); };
    auto                           floatPane  = [this, pane] { m_host.FloatFromMenu (pane); return true; };
    std::wstring                   active     = pane;



    if (!m_host.EditMenuLayout().Contains (pane))
    {
        return items;
    }

    for (const MenuItem & item : dockTo)
    {
        autoHide = (item.label == kAutoHideLabel) ? item : autoHide;
        dock     = (!docked && item.label == L"Dock") ? item : dock;
    }

    //  The documents' group, when this pane is a tool window outside it.
    for (const DxuiPaneLayout::GroupRect & group : m_host.ArrangeForMenu())
    {
        bool  holdsDocument = isDocument && std::any_of (group.panes.begin(), group.panes.end(), isDocument);
        bool  holdsMe       = std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end();

        documents = (documents.empty() && holdsDocument && !holdsMe) ? group.active : documents;
        active    = holdsMe ? group.active : active;
    }

    documents = (isDocument && !isDocument (pane)) ? documents : std::wstring();

    items.push_back ({ L"Dock", dock.action, dock.action != nullptr });
    items.push_back ({ L"Dock in tab group", [this, pane, documents, commit] { return commit (m_host.EditMenuLayout().TabWith (pane, documents)); },
                       !documents.empty() && !m_host.IsMenuFloatingSite() });
    items.push_back ({ kAutoHideLabel, autoHide.action, autoHide.action != nullptr });

    if (fromTab)
    {
        items.push_back ({ L"Move to new window", floatPane, canFloat });
    }

    if (mine.size() > 1 && m_host.CanFloatGroupFromMenu())
    {
        items.push_back ({ L"All to new window", [this, mine, active] { m_host.FloatGroupFromMenu (mine, active); return true; }, canFloat });
    }
    else
    {
        items.push_back ({ L"All to new window", floatPane, canFloat && mine.size() == 1 });
    }

    items.push_back ({ L"", nullptr, false });
    items.push_back ({ L"Close", [this, pane] { m_host.CloseFromMenu (pane); return true; }, canClose, L"Shift+Esc" });

    return items;
}
