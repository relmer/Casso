#pragma once

#include "Pch.h"


class DxuiDpiScaler;
class DxuiPaneLayout;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockEdgesHost
//
//  The dock site whose auto-hidden panes a DxuiDockEdges lays along its
//  edges: its layout, bounds and scale, and each pane's title, whether it is
//  visible, and whether it has an indicator.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockEdgesHost
{
public:
    virtual                         ~IDxuiDockEdgesHost() = default;

    virtual const DxuiPaneLayout  & GetEdgesLayout       () const                          = 0;
    virtual RECT                    GetEdgesBounds       () const                          = 0;
    virtual const DxuiDpiScaler   & GetEdgesScaler       () const                          = 0;
    virtual bool                    IsEdgePaneShown      (const std::wstring & pane) const = 0;
    virtual std::wstring            GetEdgePaneTitle     (const std::wstring & pane) const = 0;
    virtual bool                    HasEdgePaneIndicator (const std::wstring & pane) const = 0;
};
