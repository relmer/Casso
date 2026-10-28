#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPane
//
//  Where the window's keyboard focus is: one of its panes, or one of the
//  toolbars, boxes and strips Tab stops at. FocusRing defines the Tab order.
//
////////////////////////////////////////////////////////////////////////////////

enum class ExplorerPane { Toolbar, Address, Tabs, Tree, List, PreviewToolbar, GoTo, Search, Preview, CommandBar, LocationSearch };
