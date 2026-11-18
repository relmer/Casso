#pragma once

#include "Pch.h"
#include "Core/DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockOps
//
//  The dock site operations a menu item runs. Each that returns a bool
//  returns whether the layout changed.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockOps
{
public:
    virtual       ~IDxuiDockOps() = default;

    virtual bool  DockPaneToEdge (const std::wstring & pane, DxuiDockSide edge)                         = 0;
    virtual bool  TabPaneWith    (const std::wstring & pane, const std::wstring & target)               = 0;
    virtual bool  PinPane        (const std::wstring & pane, DxuiDockSide edge)                         = 0;
    virtual bool  UnpinPane      (const std::wstring & pane)                                            = 0;
    virtual void  FloatPane      (const std::wstring & pane)                                            = 0;
    virtual void  FloatGroup     (const std::vector<std::wstring> & panes, const std::wstring & active) = 0;
    virtual void  ClosePane      (const std::wstring & pane)                                            = 0;
};
