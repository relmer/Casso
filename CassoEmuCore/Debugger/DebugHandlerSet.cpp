#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"

#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHandlerSet::Attach
//
////////////////////////////////////////////////////////////////////////////////

void DebugHandlerSet::Attach (DebugSession & session)
{
    session.AddHandler (&m_registers);
    session.AddHandler (&m_breakpoints);
    session.AddHandler (&m_memory);
    session.AddHandler (&m_data);
    session.AddHandler (&m_execution);
    session.AddHandler (&m_watches);
    session.AddHandler (&m_config);
    session.AddHandler (&m_symbols);
    session.AddHandler (&m_monitor);
    session.SetInstructionObserver (&m_execution);
}
