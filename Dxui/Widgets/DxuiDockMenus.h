#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"


class DxuiDockGroups;
class DxuiDockPanes;
class IDxuiDockOps;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMenuItem
//
//  An entry of a dock site's menu. An empty label is a separator.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDockMenuItem
{
    std::wstring           label;
    std::function<bool ()> action;
    bool                   enabled = true;
    std::wstring           accelerator;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockMenus
//
//  A dock site's menus for a pane: Dock To, for keyboard docking, and Visual
//  Studio's menu for a pane's tab, title bar and menu button. The items are
//  built from the layout, the panes and the groups, and run the site's
//  operations.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockMenus
{
public:
    using MenuItem   = DxuiDockMenuItem;
    using PaneTestFn = std::function<bool (const std::wstring & pane)>;

    static constexpr wchar_t  kAutoHideLabel[] = L"Auto hide";

    //  The site's arrangement and what its handlers allow, taken from the
    //  site each time a menu is built for a pane.
    struct SiteState
    {
        std::vector<DxuiPaneLayout::GroupRect>  arranged;
        RECT                                    dockedArea     = {};
        PaneTestFn                              isDocument;
        bool                                    canFloat       = false;
        bool                                    canFloatGroup  = false;
        bool                                    canClose       = false;   // the pane the menu is for
        bool                                    isFloatingSite = false;
    };

    DxuiDockMenus (const DxuiPaneLayout  & layout,
                   const DxuiDockPanes   & panes,
                   const DxuiDockGroups  & groups,
                   IDxuiDockOps          & ops) : m_layout (layout), m_panes (panes), m_groups (groups), m_ops (ops) {}

    std::vector<MenuItem>  GetDockToMenu (const std::wstring & pane, const SiteState & site);
    std::vector<MenuItem>  GetPaneMenu   (const std::wstring & pane, bool fromTab, const SiteState & site);

private:
    const DxuiPaneLayout  & m_layout;
    const DxuiDockPanes   & m_panes;
    const DxuiDockGroups  & m_groups;
    IDxuiDockOps          & m_ops;
};
