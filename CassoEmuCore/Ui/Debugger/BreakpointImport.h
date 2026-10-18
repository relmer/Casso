#pragma once

#include "Debugger/DebugCommand.h"

class DebugSession;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport
//
//  Import in the breakpoints pane (FR-121): the breakpoints of a script in
//  any console dialect -- the AppleWin form BPSAVE writes, or WinDbg's or
//  GSSquared's commands -- added to those already set. Only breakpoint lines
//  are read. A MODE line sets the dialect tried first for the lines after it.
//  A line that sets a breakpoint in some dialect runs as typed, in that
//  dialect; a line that disables, enables or changes one is applied to the
//  breakpoint its number stands for -- the script counts its breakpoints from
//  0, as it would after its own clear-all -- and every other line, a clear
//  among them, is skipped rather than run.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointImport
{
public:
    using LineRunner = std::function<Reply (const std::string & line, CommandMode mode)>;

    //  Runs the script's breakpoint lines through `run` and gives how many
    //  of its lines were skipped. Blank lines and MODE lines are not counted.
    static int  Run (DebugSession & session, const std::string & script, const LineRunner & run);

private:
    static bool  IsDefinition (DebugVerb verb);
    static bool  TryReadMode   (const std::string & line, CommandMode & mode);
    static bool  TryParse      (DebugSession & session, const std::string & line, CommandMode mode, DebugCommand & command);
    static bool  TryApply      (const std::string & line, const DebugCommand & command, const std::vector<int> & created, const LineRunner & run);
    static bool  TryApplyFlags (const std::string & line, const std::vector<int> & created, const LineRunner & run);
    static std::vector<int>  GetIds (DebugSession & session);
};
