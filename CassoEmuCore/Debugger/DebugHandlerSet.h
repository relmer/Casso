#pragma once

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/CallStackHandlers.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "Debugger/Handlers/MonitorHandlers.h"
#include "Debugger/Handlers/RegisterHandlers.h"
#include "Debugger/Handlers/SymbolHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHandlerSet
//
//  Every AppleWin-mode command family, owned together and attached to a
//  session in one call, so batch mode, the channel and the window offer the
//  same commands. The execution family is also attached as the session's
//  instruction observer.
//
////////////////////////////////////////////////////////////////////////////////

class DebugHandlerSet
{
public:
    void  Attach (DebugSession & session);

private:
    RegisterHandlers       m_registers;
    BreakpointHandlers     m_breakpoints;
    MemoryHandlers         m_memory;
    DataDirectiveHandlers  m_data;
    ExecutionHandlers      m_execution;
    WatchHandlers          m_watches;
    ConfigHandlers         m_config;
    SymbolHandlers         m_symbols;
    MonitorHandlers        m_monitor;
    CallStackHandlers      m_callStack;
};
