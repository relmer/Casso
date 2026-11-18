#include "Pch.h"

#include "Ui/Debugger/RunActions.h"
#include "Ui/Debugger/DebuggerCommandLines.h"

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetRun / GetStepInto
//
//  A step with no count steps once.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetRun (CommandMode mode)
{
    return DebuggerActions::Make (DebugVerb::Go, DebuggerCommandLines::GetRunLine(), mode);
}


DebuggerAction RunActions::GetStepInto (CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::StepInto, DebuggerCommandLines::GetStepLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetStepOver
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetStepOver (CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::StepOver, DebuggerCommandLines::GetStepOverLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetStepOut
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetStepOut (CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::StepOut, DebuggerCommandLines::GetStepOutLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetReverse
//
//  The AppleWin line each reverse verb is typed as, which the echo shows in
//  the words of the mode.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetReverse (DebugVerb verb, CommandMode mode)
{
    const char  * line = "T-";



    switch (verb)
    {
    case DebugVerb::StepBackOver: line = "P-";   break;
    case DebugVerb::StepBackOut:  line = "GU-";  break;
    case DebugVerb::ReverseGo:    line = "G-";   break;
    case DebugVerb::GoLive:       line = "LIVE"; break;
    default:                                     break;
    }

    return DebuggerActions::Make (verb, line, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetRunToCursor
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetRunToCursor (Word address)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::Go, DebuggerCommandLines::GetRunToCursorLine (address), DebuggerCommandLines::kRunToCursorMode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.echoMode      = DebuggerCommandLines::kRunToCursorMode;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetRunFrame
//
//  FRAME is Casso's own command, so it runs in Casso mode whatever mode the
//  console is in, as run to cursor does.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetRunFrame()
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::RunFrame, "FRAME", CommandMode::Casso);



    action.command.count = 1;
    action.echoMode      = CommandMode::Casso;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetBasicStep
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetBasicStep (DebugVerb verb, CommandMode mode)
{
    const char  * name = (verb == DebugVerb::BasicStepLine) ? "BASICLINE"
                       : (verb == DebugVerb::BasicStepOut)  ? "BASICOUT"
                       :                                      "BASICSTEP";



    return DebuggerActions::Make (verb, name, mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetResetCycles
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetResetCycles (CommandMode mode)
{
    return DebuggerActions::Make (DebugVerb::ResetCycles, "RCC", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetSourceStepping
//
//  SRC ON or SRC OFF; the count is 1 for source lines.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetSourceStepping (bool isSource, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetSourceStepping, isSource ? "SRC ON" : "SRC OFF", mode);



    action.command.count = isSource ? 1 : 0;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetSkipRoutine
//
//  By the routine's symbol where it has one, so the filter follows the
//  symbol, and otherwise by its address.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetSkipRoutine (const std::string & name, Word address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddStepFilter, name.empty() ? std::format ("SKIP {:04X}", address) : "SKIP " + name, mode);



    action.command.text  = name;
    action.command.a1    = address;
    action.command.a2    = address;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetSkipRange
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetSkipRange (Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::AddStepFilter, (first == last) ? std::format ("SKIP {:04X}", first) : std::format ("SKIP {:04X}.{:04X}", first, last), mode);



    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetStopwatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetStopwatch (Word start, Word stop, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::Stopwatch, std::format ("STOPWATCH {:04X} {:04X}", start, stop), mode);



    action.command.text = std::format ("{:04X} {:04X}", start, stop);
    action.isResult     = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetCallSubroutine
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetCallSubroutine (Word address, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::CallSubroutine, std::format ("JSR {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions::GetRemoveSkip
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction RunActions::GetRemoveSkip (const StepFilterEntry & entry, CommandMode mode)
{
    std::string     text   = !entry.name.empty()         ? entry.name
                           : (entry.first == entry.last) ? std::format ("{:04X}", entry.first)
                           :                               std::format ("{:04X}.{:04X}", entry.first, entry.last);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::RemoveStepFilter, "SKIP -" + text, mode);



    action.command.text  = text;
    action.command.a1    = entry.first;
    action.command.a2    = entry.last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}
