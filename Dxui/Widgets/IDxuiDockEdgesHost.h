#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDockEdgesHost
//
//  The dock site whose auto-hidden panes a DxuiDockEdges lays along its
//  edges: its bounds, and whether a pane is visible.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDockEdgesHost
{
public:
    virtual        ~IDxuiDockEdgesHost() = default;

    virtual RECT   GetEdgesBounds  () const                          = 0;
    virtual bool   IsEdgePaneShown (const std::wstring & pane) const = 0;
};