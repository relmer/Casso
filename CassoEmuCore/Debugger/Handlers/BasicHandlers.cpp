#include "Pch.h"

#include "Debugger/Handlers/BasicHandlers.h"

#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool BasicHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::SetBasicLineBreakpoint:     SetLine       (session, command, reply); return true;
    case DebugVerb::SetBasicVariableBreakpoint: SetVariable   (session, command, reply); return true;
    case DebugVerb::BreakOnBasicError:          SetErrorBreak (session, command, reply); return true;
    case DebugVerb::ShowBasicVariables:         ShowVariables (session, reply);          return true;
    case DebugVerb::ShowBasicStack:             ShowStack     (session, reply);          return true;
    default:                                    return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::TryParseLine
//
////////////////////////////////////////////////////////////////////////////////

bool BasicHandlers::TryParseLine (const std::string & text, Word & line)
{
    uint32_t  value = 0;



    if (text.empty() || text.size() > 5 || !std::ranges::all_of (text, [] (char ch) { return std::isdigit ((unsigned char) ch) != 0; }))
    {
        return false;
    }

    std::from_chars (text.data(), text.data() + text.size(), value);
    line = (Word) value;
    return value <= kMaxLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::SetLine
//
//  The line may be missing from the program: a breakpoint set before the
//  program is typed or loaded stops once the line is there.
//
////////////////////////////////////////////////////////////////////////////////

void BasicHandlers::SetLine (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word        line = 0;
    int         id   = 0;
    Breakpoint  entry;



    if (!TryParseLine (command.text, line))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BPBASIC takes a line number, 0 to 63999.");
        return;
    }

    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

    id = session.GetBreakpoints().AddBasicLine (line);
    session.GetBreakpoints().TrySetCondition (id, command.expression);
    session.OnStopConditionsChanged();
    session.GetBreakpoints().TryFind (id, entry);
    reply.data = BreakpointSetData { BreakpointHandlers::MakeInfo (entry) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::SetVariable
//
//  The name is stored as typed, and the listing has it that way; Applesoft
//  compares only a name's first two characters and its type.
//
////////////////////////////////////////////////////////////////////////////////

void BasicHandlers::SetVariable (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    BasicReference  reference;
    int             id = 0;
    Breakpoint      entry;



    if (!ApplesoftProgram::TryParseReference (command.text, reference))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        "BPBASICVAR takes a variable or an array element as a program writes it: A, B%, C$ or D(3,4).");
        return;
    }

    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

    id = session.GetBreakpoints().AddBasicVariable (command.text);
    session.GetBreakpoints().TrySetCondition (id, command.expression);
    session.OnStopConditionsChanged();
    session.GetBreakpoints().TryFind (id, entry);
    reply.data = BreakpointSetData { BreakpointHandlers::MakeInfo (entry) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::SetErrorBreak
//
//  BRKBASIC [ON|OFF]; alone, it reports the setting.
//
////////////////////////////////////////////////////////////////////////////////

void BasicHandlers::SetErrorBreak (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Breakpoint  entry;
    auto        isError = [] (const Breakpoint & candidate) { return candidate.kind == BreakpointKind::BasicError; };



    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

    if (command.text == "ON")
    {
        if (!HasErrorBreak (session))
        {
            session.GetBreakpoints().AddBasicError();
            session.OnStopConditionsChanged();
        }

        entry = *std::ranges::find_if (session.GetBreakpoints().GetAll(), isError);
        session.GetBreakpoints().TrySetCondition (entry.id, command.expression);
        entry.condition = command.expression;
        reply.data = BreakpointSetData { BreakpointHandlers::MakeInfo (entry) };
    }
    else if (command.text == "OFF")
    {
        session.GetBreakpoints().ClearKind (BreakpointKind::BasicError);
        session.OnStopConditionsChanged();
        reply.data = MessageData { { "Break on BASIC error: off" } };
    }
    else if (command.text.empty())
    {
        reply.data = MessageData { { std::string ("Break on BASIC error: ") + (HasErrorBreak (session) ? "on" : "off") } };
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BRKBASIC takes ON or OFF.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::HasErrorBreak
//
////////////////////////////////////////////////////////////////////////////////

bool BasicHandlers::HasErrorBreak (DebugSession & session)
{
    return std::ranges::any_of (session.GetBreakpoints().GetAll(), [] (const Breakpoint & entry) { return entry.kind == BreakpointKind::BasicError; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::ShowVariables
//
////////////////////////////////////////////////////////////////////////////////

void BasicHandlers::ShowVariables (DebugSession & session, Reply & reply)
{
    BasicVariablesData  data;
    ApplesoftProgram    program ([&session] (Word address) { Byte value = 0; session.TryPeek (address, value); return value; });



    if (!BasicStopMonitor::IsApplesoftVisible (session))
    {
        reply.SetError (CommandStatus::Error, "no Applesoft", "Applesoft is not in memory: the $D000 bank has other code.");
        return;
    }

    program.GetVariables (data.variables);
    program.GetArrays    (data.arrays);
    reply.data = data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers::ShowStack
//
////////////////////////////////////////////////////////////////////////////////

void BasicHandlers::ShowStack (DebugSession & session, Reply & reply)
{
    BasicStackData    data;
    ApplesoftProgram  program ([&session] (Word address) { Byte value = 0; session.TryPeek (address, value); return value; });



    if (!BasicStopMonitor::IsApplesoftVisible (session))
    {
        reply.SetError (CommandStatus::Error, "no Applesoft", "Applesoft is not in memory: the $D000 bank has other code.");
        return;
    }

    program.GetFrames (data.frames);
    reply.data = data;
}
