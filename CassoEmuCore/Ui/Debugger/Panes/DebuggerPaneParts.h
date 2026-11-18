#pragma once

class DebuggerPaneLook;
class IDebuggerPaneHost;
class LatestSnapshot;
class SymbolComments;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerPaneParts
//
//  The parts of the debugger window a pane is built with. Each pane and
//  controller has references to the parts it uses; a group that creates
//  panes later has a reference to this. The window owns every part, and the
//  panes go before it.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerPaneParts
{
    IDebuggerPaneHost       & host;
    const DebuggerPaneLook  & look;
    const LatestSnapshot    & latest;
    SymbolComments          & symbolComments;
};
