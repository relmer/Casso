#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockMenuHost
//
//  The dock site a DxuiDockMenus builds menus for: its layout and groups,
//  and the site operations behind its menu items.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockMenuHost
{
public:
    using GroupRect  = DxuiPaneLayout::GroupRect;
    using PaneTestFn = std::function<bool (const std::wstring & pane)>;

    virtual                            ~IDxuiDockMenuHost() = default;

    virtual DxuiPaneLayout         &   EditMenuLayout        ()                                                                   = 0;
    virtual std::vector<std::wstring>  GetActivePanes        () const                                                             = 0;
    virtual std::wstring               GetMenuPaneTitle      (const std::wstring & pane) const                                    = 0;
    virtual RECT                       GetMenuDockedArea     () const                                                             = 0;
    virtual std::vector<GroupRect>     ArrangeForMenu        () const                                                             = 0;
    virtual const PaneTestFn       &   GetMenuDocumentFn     () const                                                             = 0;
    virtual bool                       IsMenuFloatingSite    () const                                                             = 0;
    virtual bool                       CommitMenuChange      (bool changed)                                                       = 0;
    virtual bool                       PinFromMenu           (const std::wstring & pane, DxuiDockSide edge)                       = 0;
    virtual bool                       UnpinFromMenu         (const std::wstring & pane)                                          = 0;
    virtual bool                       CanFloatFromMenu      () const                                                             = 0;
    virtual void                       FloatFromMenu         (const std::wstring & pane)                                          = 0;
    virtual bool                       CanFloatGroupFromMenu () const                                                             = 0;
    virtual void                       FloatGroupFromMenu    (const std::vector<std::wstring> & panes, const std::wstring & active) = 0;
    virtual bool                       CanCloseFromMenu      (const std::wstring & pane) const                                    = 0;
    virtual void                       CloseFromMenu         (const std::wstring & pane)                                          = 0;
};
