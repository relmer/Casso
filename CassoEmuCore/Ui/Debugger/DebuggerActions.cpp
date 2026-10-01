#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"

#include "Debugger/CommandModeNames.h"
#include "Ui/Debugger/DebuggerViewState.h"





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



    action.echo = DebuggerViewState::GetModeLine (appleWinLine, mode);
    start       = action.echo.starts_with ("/") ? 1 : 0;
    name        = action.echo.substr (start, action.echo.find (' ', start) - start);

    action.command.verb       = verb;
    action.command.sourceName = name;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetRun / GetStepInto
//
//  A step with no count steps once.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetRun (CommandMode mode)
{
    return Make (DebugVerb::Go, DebuggerViewState::GetRunLine(), mode);
}


DebuggerAction DebuggerActions::GetStepInto (CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::StepInto, DebuggerViewState::GetStepLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetStepOver
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetStepOver (CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::StepOver, DebuggerViewState::GetStepOverLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetStepOut
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetStepOut (CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::StepOut, DebuggerViewState::GetStepOutLine(), mode);



    action.command.count = 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetRunToCursor
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetRunToCursor (Word address)
{
    DebuggerAction  action = Make (DebugVerb::Go, DebuggerViewState::GetRunToCursorLine (address), DebuggerViewState::kRunToCursorMode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.echoMode      = DebuggerViewState::kRunToCursorMode;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetToggleBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetToggleBreakpoint (const DebuggerViewSnapshot & snapshot, Word address, CommandMode mode)
{
    DebuggerAction  action;



    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (DebuggerViewState::IsCodeBreakpointAt (bp, address))
        {
            return GetClearBreakpoint (bp.id, mode);
        }
    }

    action               = Make (DebugVerb::SetBreakpoint, std::format ("BP {:04X}", address), mode);
    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnableBreakpoint
//
//  Each goes by id, which the command carries as its count.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnableBreakpoint (int id, bool enable, CommandMode mode)
{
    DebuggerAction  action = Make (enable ? DebugVerb::EnableBreakpoint : DebugVerb::DisableBreakpoint,
                                   std::format ("{} {}", enable ? "BPE" : "BPD", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearBreakpoint (int id, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearBreakpoint, std::format ("BPC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearWatch (int id, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearWatch, std::format ("WC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPanel
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPanel (const std::string & id, bool open, CommandMode mode)
{
    DebuggerAction  action = Make (open ? DebugVerb::OpenPanel : DebugVerb::ClosePanel, DebuggerViewState::GetPanelLine (id, open), mode);



    action.command.text = id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPoke
//
//  One byte, which must match in full, as a typed deposit's values do.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPoke (Word address, Byte value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = Make (DebugVerb::EnterBytes, DebuggerViewState::GetPokeLine (address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { value };
    action.command.mask   = { kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetTraceToggle
//
//  The command's count is 1 to turn tracing on and 0 to turn it off.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetTraceToggle (bool isOn, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetHistory, DebuggerViewState::GetTraceToggleLine (isOn), mode);



    action.command.count = isOn ? 0 : 1;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSetMode
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSetMode (CommandMode target, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetMode, "MODE " + CommandModeNames::GetUpperName (target), mode);



    action.command.mode = target;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSetRegister
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSetRegister (const std::string & name, Byte value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetRegister, std::format ("R {} {:02X}", name, value), mode);



    action.command.text  = name;
    action.command.a1    = value;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetForKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> DebuggerActions::GetForKey (
    DebuggerKeySchemes::Action   action,
    const DebuggerViewSnapshot * snapshot,
    int                          selectedRow,
    CommandMode                  mode)
{
    using Action = DebuggerKeySchemes::Action;

    std::optional<DebuggerAction>  taken;
    std::optional<Word>            selected;
    std::optional<Word>            current;



    if (snapshot != nullptr && selectedRow >= 0 && selectedRow < (int) snapshot->code.size())
    {
        selected = snapshot->code[(size_t) selectedRow].address;
    }

    if (snapshot != nullptr)
    {
        current = selected.has_value() ? selected : std::optional<Word> (snapshot->pc);
    }

    switch (action)
    {
    case Action::Run:      taken = GetRun      (mode); break;
    case Action::StepInto: taken = GetStepInto (mode); break;
    case Action::StepOver: taken = GetStepOver (mode); break;
    case Action::StepOut:  taken = GetStepOut  (mode); break;

    case Action::RunToCursor:
        if (selected.has_value())
        {
            taken = GetRunToCursor (*selected);
        }

        break;

    case Action::ToggleBreakpoint:
        if (snapshot != nullptr && current.has_value())
        {
            taken = GetToggleBreakpoint (*snapshot, *current, mode);
        }

        break;

    case Action::Pause:
    default:
        break;
    }

    return taken;
}
