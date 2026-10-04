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
//  DebuggerActions::GetReverse
//
//  The AppleWin line each reverse verb is typed as, which the echo shows in
//  the words of the mode.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetReverse (DebugVerb verb, CommandMode mode)
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

    return Make (verb, line, mode);
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
//  DebuggerActions::GetRunFrame
//
//  FRAME is Casso's own command, so it runs in Casso mode whatever mode the
//  console is in, as run to cursor does.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetRunFrame()
{
    DebuggerAction  action = Make (DebugVerb::RunFrame, "FRAME", CommandMode::Casso);



    action.command.count = 1;
    action.echoMode      = CommandMode::Casso;
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
//  DebuggerActions::GetBreakpointStep
//
//  The lines the step runs are echoed one by one as they run, in AppleWin's
//  words, so the action itself carries no echo.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetBreakpointStep (BreakpointStep step)
{
    DebuggerAction  action;



    action.breakpointStep = std::move (step);
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

DebuggerAction DebuggerActions::GetSetRegister (const std::string & name, Word value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetRegister, std::format ("R {} {:02X}", name, value), mode);



    action.command.text  = name;
    action.command.a1    = value;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSetRegister
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSetRegister (const std::string & name, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetRegister, std::format ("R {} {}", name, value), mode);



    action.command.text  = name;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPatch (Word address, std::span<const Byte> bytes, CommandMode mode)
{
    std::string     line = std::format ("PATCH {:04X}", address);
    DebuggerAction  action;



    for (Byte value : bytes)
    {
        line += std::format (" {:02X}", value);
    }

    action               = Make (DebugVerb::PatchBytes, line, mode);
    action.command.a1    = address;
    action.command.hasA1 = true;
    action.command.values.assign (bytes.begin(), bytes.end());
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSourceBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSourceBreakpoint (const std::string & fileName, int line, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetSourceBreakpoint, std::format ("BP {}:{}", fileName, line), mode);



    action.command.text  = fileName;
    action.command.count = (uint32_t) line;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetCallStackMode
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetCallStackMode (const std::string & mechanism, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetCallStackMode, "CALLS MODE " + mechanism, mode);



    action.command.text = mechanism;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSaveHistory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSaveHistory (const std::string & path, CommandMode mode)
{
    static constexpr const char * kVerb   = "HISTORY SAVE ";
    std::string                   line    = DebuggerViewState::GetLineWithFileName (kVerb, path);
    DebuggerAction                action  = Make (DebugVerb::SaveHistory, line, mode);



    //  The file name as the line carries it, quoted when it needs to be, as
    //  the parser leaves it.
    action.command.text = line.substr (strlen (kVerb));
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetAddWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetAddWatch (Word address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddWatch, std::format ("W {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetAddWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetAddWatch (const std::string & expression, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddWatch, "W " + expression, mode);



    action.command.hasA1 = true;
    action.operand       = expression;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnterWord / GetEnterByte
//
//  A value given as a number is written whole, as a typed deposit's values
//  are; one given as text is evaluated when the action runs.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnterWord (Word address, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::EnterWords, std::format ("MEW {:04X} {}", address, value), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnterWord
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnterWord (Word address, Word value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = Make (DebugVerb::EnterWords, std::format ("MEW {:04X} {:04X}", address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { (Byte) value, (Byte) (value >> 8) };
    action.command.mask   = { kFullMask, kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnterByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnterByte (Word address, const std::string & value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::EnterBytes, std::format ("MEB {:04X} {}", address, value), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    action.operand       = value;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnterByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnterByte (Word address, Byte value, CommandMode mode)
{
    static constexpr Byte  kFullMask = 0xFF;
    DebuggerAction         action    = Make (DebugVerb::EnterBytes, std::format ("MEB {:04X} {:02X}", address, value), mode);



    action.command.a1     = address;
    action.command.hasA1  = true;
    action.command.values = { value };
    action.command.mask   = { kFullMask };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnableAllBreakpoints / GetClearAllBreakpoints
//
//  The command carries `*` as its text, as the parser leaves it.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnableAllBreakpoints (bool enable, CommandMode mode)
{
    DebuggerAction  action = Make (enable ? DebugVerb::EnableBreakpoint : DebugVerb::DisableBreakpoint, enable ? "BPE *" : "BPD *", mode);



    action.command.text = "*";
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearAllBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearAllBreakpoints (CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearBreakpoint, "BPC *", mode);



    action.command.text = "*";
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEditBreakpoint
//
//  The handler reads the new definition as its own line.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEditBreakpoint (int id, const std::string & definition, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::EditBreakpoint, std::format ("BPEDIT {} {}", id, definition), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = definition;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetChangeBreakpoint
//
//  E or e for enabled, T or t for temporary, S or s for whether a hit stops
//  the machine.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetChangeBreakpoint (int id, bool enabled, bool temporary, bool stops, CommandMode mode)
{
    std::string     flags  = std::format ("{}{}{}", enabled ? 'E' : 'e', temporary ? 'T' : 't', stops ? 'S' : 's');
    DebuggerAction  action = Make (DebugVerb::ChangeBreakpoint, std::format ("BPCHANGE {} {}", id, flags), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = flags;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetDefineBreakpoint
//
//  The verb is the definition's own, which only parsing it can tell.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetDefineBreakpoint (const std::string & definition, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetBreakpoint, definition, mode);



    action.definition = definition;
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
    case Action::RunFrame: taken = GetRunFrame();      break;

    case Action::StepBackInto: taken = GetReverse (DebugVerb::StepBack,     mode); break;
    case Action::StepBackOver: taken = GetReverse (DebugVerb::StepBackOver, mode); break;
    case Action::StepBackOut:  taken = GetReverse (DebugVerb::StepBackOut,  mode); break;
    case Action::ReverseRun:   taken = GetReverse (DebugVerb::ReverseGo,    mode); break;

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

    if (snapshot != nullptr && !snapshot->isPaused && IsStepAction (action))
    {
        taken.reset();
    }

    return taken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::IsStepAction
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerActions::IsStepAction (DebuggerKeySchemes::Action action)
{
    using Action = DebuggerKeySchemes::Action;



    switch (action)
    {
    case Action::StepInto:
    case Action::StepOver:
    case Action::StepOut:
    case Action::RunToCursor:
    case Action::RunFrame:
    case Action::StepBackInto:
    case Action::StepBackOver:
    case Action::StepBackOut:
    case Action::ReverseRun:
        return true;

    default:
        return false;
    }
}
