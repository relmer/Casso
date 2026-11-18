#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerCommandLines.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::Make
//
//  The echo is the AppleWin line in the words of the mode, as the console
//  shows it. The command's name is the echo's first word, without Monitor
//  mode's `/`, so a reply quotes the command as the echo shows it.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::Make (DebugVerb verb, const std::string & appleWinLine, CommandMode mode)
{
    DebuggerAction  action;
    std::string     name;
    size_t          start = 0;



    action.echo = DebuggerCommandLines::GetModeLine (appleWinLine, mode);
    start       = action.echo.starts_with ("/") ? 1 : 0;
    name        = action.echo.substr (start, action.echo.find (' ', start) - start);

    action.command.verb       = verb;
    action.command.sourceName = name;
    return action;
}
