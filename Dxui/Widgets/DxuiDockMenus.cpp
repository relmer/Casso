#include "Pch.h"

#include "DxuiDockMenus.h"
#include "DxuiDockEdges.h"
#include "DxuiDockGroups.h"
#include "DxuiDockPanes.h"
#include "IDxuiDockOps.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMenus::GetDockToMenu
//
//  Against each edge of the site, into each other shown group, and out into
//  a window of its own; or back, for a pane that is floating or hidden.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiDockMenus::MenuItem> DxuiDockMenus::GetDockToMenu (const std::wstring & pane, const SiteState & site)
{
    static constexpr DxuiDockSide  kSides[]    = { DxuiDockSide::Left, DxuiDockSide::Top, DxuiDockSide::Right, DxuiDockSide::Bottom };
    static const wchar_t * const   kNames[]    = { L"Dock left", L"Dock top", L"Dock right", L"Dock bottom" };
    std::vector<MenuItem>          items;
    std::vector<std::wstring>      mine        = m_layout.GetGroup (pane);



    if (!m_layout.Contains (pane))
    {
        return items;
    }

    if (!m_layout.IsDocked (pane))
    {
        items.push_back ({ L"Dock", [this, pane] { return m_ops.UnpinPane (pane); } });
        return items;
    }

    for (size_t i = 0; i < std::size (kSides); i++)
    {
        DxuiDockSide  side = kSides[i];

        items.push_back ({ kNames[i], [this, pane, side] { return m_ops.DockPaneToEdge (pane, side); } });
    }

    for (const std::wstring & target : m_groups.GetActivePanes())
    {
        if (target.empty() || std::find (mine.begin(), mine.end(), target) != mine.end())
        {
            continue;
        }

        items.push_back ({ L"Tab with " + m_panes.GetTitle (target),
                           [this, pane, target] { return m_ops.TabPaneWith (pane, target); } });
    }

    for (const DxuiPaneLayout::GroupRect & group : site.arranged)
    {
        DxuiDockSide  edge = DxuiDockEdges::GetNearestEdge (group.rect, site.dockedArea);

        if (std::find (group.panes.begin(), group.panes.end(), pane) == group.panes.end())
        {
            continue;
        }

        items.push_back ({ kAutoHideLabel, [this, pane, edge] { return m_ops.PinPane (pane, edge); } });
    }

    if (site.canFloat)
    {
        items.push_back ({ L"Float", [this, pane] { m_ops.FloatPane (pane); return true; } });
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

std::vector<DxuiDockMenus::MenuItem> DxuiDockMenus::GetPaneMenu (const std::wstring & pane, bool fromTab, const SiteState & site)
{
    std::vector<MenuItem>       items;
    std::vector<MenuItem>       dockTo     = GetDockToMenu (pane, site);
    std::vector<std::wstring>   mine       = m_layout.GetGroup (pane);
    bool                        docked     = m_layout.IsDocked (pane);
    bool                        canFloat   = docked && site.canFloat && !site.isFloatingSite;
    const PaneTestFn          & isDocument = site.isDocument;
    std::wstring                documents;
    MenuItem                    autoHide   = { kAutoHideLabel, nullptr, false };
    MenuItem                    dock       = { L"Dock", nullptr, false };
    auto                        floatPane  = [this, pane] { m_ops.FloatPane (pane); return true; };
    std::wstring                active     = pane;



    if (!m_layout.Contains (pane))
    {
        return items;
    }

    for (const MenuItem & item : dockTo)
    {
        autoHide = (item.label == kAutoHideLabel) ? item : autoHide;
        dock     = (!docked && item.label == L"Dock") ? item : dock;
    }

    //  The documents' group, when this pane is a tool window outside it.
    for (const DxuiPaneLayout::GroupRect & group : site.arranged)
    {
        bool  holdsDocument = isDocument && std::any_of (group.panes.begin(), group.panes.end(), isDocument);
        bool  holdsMe       = std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end();

        documents = (documents.empty() && holdsDocument && !holdsMe) ? group.active : documents;
        active    = holdsMe ? group.active : active;
    }

    documents = (isDocument && !isDocument (pane)) ? documents : std::wstring();

    items.push_back ({ L"Dock", dock.action, dock.action != nullptr });
    items.push_back ({ L"Dock in tab group", [this, pane, documents] { return m_ops.TabPaneWith (pane, documents); },
                       !documents.empty() && !site.isFloatingSite });
    items.push_back ({ kAutoHideLabel, autoHide.action, autoHide.action != nullptr });

    if (fromTab)
    {
        items.push_back ({ L"Move to new window", floatPane, canFloat });
    }

    if (mine.size() > 1 && site.canFloatGroup)
    {
        items.push_back ({ L"All to new window", [this, mine, active] { m_ops.FloatGroup (mine, active); return true; }, canFloat });
    }
    else
    {
        items.push_back ({ L"All to new window", floatPane, canFloat && mine.size() == 1 });
    }

    items.push_back ({ L"", nullptr, false });
    items.push_back ({ L"Close", [this, pane] { m_ops.ClosePane (pane); return true; }, site.canClose, L"Shift+Esc" });

    return items;
}
