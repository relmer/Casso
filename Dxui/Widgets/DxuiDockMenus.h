#pragma once

#include "Pch.h"


class IDxuiDockMenuHost;





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
//  Studio's menu for a pane's tab, title bar and menu button.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDockMenus
{
public:
    using MenuItem = DxuiDockMenuItem;

    static constexpr wchar_t  kAutoHideLabel[] = L"Auto hide";

    explicit DxuiDockMenus (IDxuiDockMenuHost & host) : m_host (host) {}

    std::vector<MenuItem>  GetDockToMenu (const std::wstring & pane);
    std::vector<MenuItem>  GetPaneMenu   (const std::wstring & pane, bool fromTab);

private:
    IDxuiDockMenuHost  & m_host;
};
