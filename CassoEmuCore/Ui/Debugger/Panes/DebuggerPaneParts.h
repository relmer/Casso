#pragma once

#include "Ui/Debugger/Panes/PaneHostInterfaces.h"

class DebuggerPaneLook;
class IDebuggerWindowHost;
class LatestSnapshot;
class SymbolComments;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneParts
//
//  The parts of the debugger window a pane is built with. Each pane and
//  controller has references to the parts it uses; a group that creates
//  panes later has a reference to this. The window owns every part, and the
//  panes go before it. windowHost is the window's pointer to its host, which
//  is set after the panes are built; it is null for a window with no host.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerPaneParts
{
    DxuiWindow                   & window;
    const DxuiDpiScaler          & scaler;
    IDebuggerWindowHost * const  & windowHost;
    const DebuggerPaneLook       & look;
    const LatestSnapshot         & latest;
    SymbolComments               & symbolComments;
    IPaneCommands                & commands;
    IPaneNavigator               & navigator;
    IPanePlaces                  & places;
    IPaneTabs                    & tabs;
    IPaneFocus                   & focus;
    IPaneRouting                 & routing;
    IPaneBars                    & bars;
    IPaneTips                    & tips;
    IPaneLayout                  & layout;
    IPaneViews                   & views;
    IPaneDialogs                 & dialogs;
    IPaneReplies                 & replies;
};
