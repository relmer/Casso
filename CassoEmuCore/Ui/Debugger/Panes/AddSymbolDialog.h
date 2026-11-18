#pragma once

#include "Pch.h"

#include "Debugger/DebugCommand.h"

class IDebuggerPaneHost;





////////////////////////////////////////////////////////////////////////////////
//
//  AddSymbolDialog
//
//  Add symbol here..., in the disassembly's menu and the Profiler's: the
//  name the user types for an address, checked as a symbol name, then
//  added to the user table with the host's command.
//
////////////////////////////////////////////////////////////////////////////////

class AddSymbolDialog
{
public:
    static void  Run (IDebuggerPaneHost & host, CommandMode mode, Word address);
};
