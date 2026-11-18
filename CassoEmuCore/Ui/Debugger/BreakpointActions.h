#pragma once

#include "Ui/Debugger/DebuggerActions.h"

struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions
//
//  The actions on breakpoints: setting, clearing, enabling and editing one,
//  the whole list at once, BPNAME, BPGROUP and BPLOG, and the Breakpoints
//  pane's undo steps.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointActions
{
public:
    //  Sets a breakpoint at the address, or clears the execution breakpoint
    //  there by its id, so a click never clears a different breakpoint that
    //  happens to cover the same address.
    static DebuggerAction  GetToggleBreakpoint (const DebuggerViewSnapshot & snapshot, Word address, CommandMode mode);
    static DebuggerAction  GetEnableBreakpoint (int id, bool enable, CommandMode mode);
    static DebuggerAction  GetClearBreakpoint  (int id, CommandMode mode);
    static DebuggerAction  GetBreakpointStep   (BreakpointStep step);
    static DebuggerAction  GetSaveBreakpoints  (const std::string & path, CommandMode mode);
    static DebuggerAction  GetSourceBreakpoint (const std::string & fileName, int line, CommandMode mode);

    //  The breakpoints pane's changes, where `*` is every breakpoint.
    static DebuggerAction  GetEnableAllBreakpoints (bool enable, CommandMode mode);
    static DebuggerAction  GetClearAllBreakpoints  (CommandMode mode);
    static DebuggerAction  GetEditBreakpoint       (int id, const BreakpointDefinition & definition, CommandMode mode);
    static DebuggerAction  GetChangeBreakpoint     (int id, bool enabled, bool temporary, bool stops, CommandMode mode);
    static DebuggerAction  GetDefineBreakpoint     (const BreakpointDefinition & definition, CommandMode mode);

    //  A definition as the line that would set it, for the console's echo.
    //  Nothing parses it.
    static std::string     GetBreakpointEcho       (const BreakpointDefinition & definition);

    //  BPNAME # "text"; an empty text returns the breakpoint to its default
    //  name.
    static DebuggerAction  GetNameBreakpoint       (int id, const std::string & name, CommandMode mode);

    //  BPGROUP with its arguments as typed: a quoted name and ids, ! and ids,
    //  or a word and a quoted name.
    static DebuggerAction  GetGroupBreakpoints     (const std::string & arguments, CommandMode mode);

    //  BPLOG # [GO] "text", GO when a hit goes on running; an empty text is
    //  BPLOG # CLEAR.
    static DebuggerAction  GetLogBreakpoint        (int id, const std::string & message, bool isGo, CommandMode mode);
};
