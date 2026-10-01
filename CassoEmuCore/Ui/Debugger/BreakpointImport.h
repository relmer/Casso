#pragma once

#include "Debugger/DebugCommand.h"
#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport
//
//  Import in the breakpoints pane (FR-121): the breakpoints of a script in the
//  form BPSAVE writes, added to those already set. Only breakpoint lines are
//  read. A line that sets a breakpoint runs as typed; a BPD, BPE or BPCHANGE
//  line is applied to the breakpoint its number stands for -- the script
//  counts its breakpoints from 0, as it would after its own BPC * -- and
//  every other line, BPC among them, is skipped rather than run.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointImport
{
public:
    using LineRunner = std::function<Reply (const std::string & line)>;

    //  Runs the script's breakpoint lines through `run` and gives how many
    //  of its lines were skipped. Blank lines are not counted.
    static int  Run (DebugSession & session, const std::string & script, const LineRunner & run);

private:
    static bool  IsDefinition (DebugVerb verb);
    static bool  TryApplyFlags (const std::string & name, std::istringstream & rest, const std::vector<int> & created, const LineRunner & run);
    static std::vector<int>  GetIds (DebugSession & session);
};
