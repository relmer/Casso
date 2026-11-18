#pragma once

#include "Debugger/Reply.h"

class DebugSession;
struct DebugCommand;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointGroupHandlers
//
//  BPGROUP: lists the breakpoint groups, puts breakpoints in a group or takes
//  them out, and enables, disables or clears a group's breakpoints.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointGroupHandlers
{
public:
    static void  Group (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  The breakpoint groups, in the order of the first breakpoint in each;
    //  the group a name matches without regard to case, empty for none; and
    //  a breakpoint or watchpoint put in a group, the name matched to one.
    static std::vector<std::string>  GetGroups (DebugSession & session);
    static std::string               FindGroup (DebugSession & session, const std::string & group);
    static bool                      SetGroup  (DebugSession & session, int id, const std::string & group);

private:
    static void                      ActOnGroup      (DebugSession & session, const std::vector<std::string> & words, Reply & reply);
    static bool                      TryParseIds     (DebugSession & session, const std::vector<std::string> & words, size_t first, std::vector<int> & ids, Reply & reply);
    static std::vector<std::string>  SplitGroupWords (const std::string & text);
};
