#pragma once

#include "Pch.h"


class DxuiTabGroup;
class IDxuiControl;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockQueryHost
//
//  The dock site a DxuiDockQuery looks up panes in: its tab groups, each
//  pane's control and tab tip, and whether the site is a floating window's
//  and a pane is auto-hidden.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockQueryHost
{
public:
    using Groups = std::vector<std::unique_ptr<DxuiTabGroup>>;

    virtual                  ~IDxuiDockQueryHost() = default;

    virtual const Groups   & GetQueryGroups      () const                              = 0;
    virtual std::wstring     GetQueryPaneOf      (const IDxuiControl * content) const  = 0;
    virtual IDxuiControl   * GetQueryPaneContent (const std::wstring & pane) const     = 0;
    virtual std::wstring     GetQueryPaneTip     (const std::wstring & pane) const     = 0;
    virtual bool             IsQueryFloatingSite () const                              = 0;
    virtual bool             IsQueryAutoHidden   (const std::wstring & pane) const     = 0;
};
