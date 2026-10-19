#include "Pch.h"

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/UndoText.h"

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

DebuggerAction DebuggerActions::GetPatch (Word address, std::span<const Byte> bytes, CommandMode mode, std::span<const Byte> required)
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
    action.required.assign (required.begin(), required.end());
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
//  DebuggerActions::GetBasicStep
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetBasicStep (DebugVerb verb, CommandMode mode)
{
    const char  * name = (verb == DebugVerb::BasicStepLine) ? "BASICLINE"
                       : (verb == DebugVerb::BasicStepOut)  ? "BASICOUT"
                       :                                      "BASICSTEP";



    return Make (verb, name, mode);
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
//  DebuggerActions::GetLoadBinary
//
//  As the parser reads `BLOAD "path",FORMAT addr,len`: the name without its
//  quotes, then the format after a comma, and the last address, not the
//  length.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetLoadBinary (const std::string & path, const std::string & format, std::optional<Word> address,
                                               std::optional<uint32_t> length, CommandMode mode)
{
    std::string     suffix = format.empty() ? std::string() : "," + format;
    std::string     range;
    DebuggerAction  action;



    if (address.has_value())
    {
        range = std::format (" {:04X}", *address) + (length.has_value() ? std::format (",{:X}", *length) : std::string());
    }

    action = Make (DebugVerb::LoadBinary, DebuggerViewState::GetLineWithFileName ("BLOAD ", path) + suffix + range, mode);

    action.command.text  = path + suffix;
    action.command.a1    = address.value_or (0);
    action.command.hasA1 = address.has_value();

    if (address.has_value() && length.has_value())
    {
        action.command.a2    = (Word) (*address + *length - 1);
        action.command.hasA2 = true;
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSaveBinary
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSaveBinary (const std::string & path, Word address, uint32_t length, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SaveBinary, DebuggerViewState::GetLineWithFileName ("BSAVE ", path) + std::format (" {:04X},{:X}", address, length), mode);



    action.command.text  = path;
    action.command.a1    = address;
    action.command.hasA1 = true;
    action.command.a2    = (Word) (address + length - 1);
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetResetCycles
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetResetCycles (CommandMode mode)
{
    return Make (DebugVerb::ResetCycles, "RCC", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPushByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPushByte (Byte value, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::PushStack, std::format ("PUSH {:02X}", value), mode);



    action.command.values = { value };
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPopByte
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPopByte (CommandMode mode)
{
    return Make (DebugVerb::PopStack, "POP", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetPopWord
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetPopWord (CommandMode mode)
{
    return Make (DebugVerb::PopStackWord, "PPOP", mode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnableWatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnableWatch (int id, bool enable, CommandMode mode)
{
    DebuggerAction  action = Make (enable ? DebugVerb::EnableWatch : DebugVerb::DisableWatch, std::format ("{} {}", enable ? "WE" : "WD", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetAddZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetAddZeroPagePointer (Byte address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddZeroPagePointer, std::format ("ZP {:02X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearZeroPagePointer (int slot, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearZeroPagePointer, std::format ("ZPC {}", slot), mode);



    action.command.count = (uint32_t) slot;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnableZeroPagePointer
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnableZeroPagePointer (int slot, bool enable, CommandMode mode)
{
    DebuggerAction  action = Make (enable ? DebugVerb::EnableZeroPagePointer : DebugVerb::DisableZeroPagePointer,
                                   std::format ("{} {}", enable ? "ZPE" : "ZPD", slot), mode);



    action.command.count = (uint32_t) slot;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSearchMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSearchMemory (const std::string & items, CommandMode mode)
{
    std::string     line   = "S 0000:FFFF " + items;
    DebuggerAction  action = Make (DebugVerb::SearchMemory, line, mode);



    action.definition = line;
    action.isResult   = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetFillMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetFillMemory (Word first, Word last, std::span<const Byte> bytes, CommandMode mode)
{
    std::string     line = std::format ("F {:04X}:{:04X}", first, last);
    DebuggerAction  action;



    for (Byte each : bytes)
    {
        line += std::format (" {:02X}", each);
    }

    action               = Make (DebugVerb::FillMemory, line, mode);
    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    action.command.values.assign (bytes.begin(), bytes.end());
    action.command.mask.assign (bytes.size(), 0xFF);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetMoveMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetMoveMemory (Word destination, Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::MoveMemory, std::format ("M {:04X} {:04X}:{:04X}", destination, first, last), mode);



    SetRangeFrom (action.command, destination, first, last);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::SetRangeFrom
//
//  M's and MC's fields: the other address in a3 and the range in a1 and a2.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerActions::SetRangeFrom (DebugCommand & command, Word other, Word first, Word last)
{
    command.a1    = first;
    command.a2    = last;
    command.a3    = other;
    command.hasA1 = true;
    command.hasA2 = true;
    command.hasA3 = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSourceStepping
//
//  SRC ON or SRC OFF; the count is 1 for source lines.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSourceStepping (bool isSource, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetSourceStepping, isSource ? "SRC ON" : "SRC OFF", mode);



    action.command.count = isSource ? 1 : 0;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetLoadSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetLoadSymbols (const std::string & path, CommandMode mode)
{
    std::string     quoted = std::format ("\"{}\"", path);
    DebuggerAction  action = Make (DebugVerb::LoadSymbols, "SYM LOAD " + quoted, mode);



    action.command.text = quoted;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSaveBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSaveBreakpoints (const std::string & path, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SaveBreakpoints, "BPSAVE " + path, mode);



    action.command.text = path;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetProject
//
//  PROJECT with its subcommand and a file name, as `SAVEAS "file"`, or the
//  subcommand alone when there is no file, as `MERGE`.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetProject (const std::string & subcommand, const std::string & path, CommandMode mode)
{
    std::string     text   = path.empty() ? subcommand : std::format ("{} \"{}\"", subcommand, path);
    DebuggerAction  action = Make (DebugVerb::Project, "PROJECT " + text, mode);



    action.command.text = text;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetMarkData
//
//  The directive's name is the session's, whatever the mode: the handler
//  reads the data's kind from it.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetMarkData (const std::string & directive, Word first, Word last, CommandMode mode)
{
    static constexpr std::pair<const char *, DebugVerb>  kVerbs[] =
    {
        { "DB", DebugVerb::DefineBytes }, { "DW", DebugVerb::DefineWords }, { "DA", DebugVerb::DefineAddress },
        { "ASC", DebugVerb::DefineText }, { "DF", DebugVerb::DefineFloat },
    };
    DebugVerb       verb   = DebugVerb::DefineBytes;
    DebuggerAction  action;



    for (const auto & [name, each] : kVerbs)
    {
        verb = (directive == name) ? each : verb;
    }

    action = Make (verb, std::format ("{} {:04X}:{:04X}", directive, first, last), mode);

    action.command.sourceName = directive;
    action.command.a1         = first;
    action.command.a2         = last;
    action.command.hasA1      = true;
    action.command.hasA2      = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearData
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearData (Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::RemoveData, std::format ("X {:04X}:{:04X}", first, last), mode);



    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetAddSymbol
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetAddSymbol (const std::string & name, Word address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddSymbol, std::format ("SYM {} = {:04X}", name, address), mode);



    action.command.text  = name;
    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSkipRoutine
//
//  By the routine's symbol where it has one, so the filter follows the
//  symbol, and otherwise by its address.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSkipRoutine (const std::string & name, Word address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddStepFilter, name.empty() ? std::format ("SKIP {:04X}", address) : "SKIP " + name, mode);



    action.command.text  = name;
    action.command.a1    = address;
    action.command.a2    = address;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSkipRange
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSkipRange (Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddStepFilter, (first == last) ? std::format ("SKIP {:04X}", first) : std::format ("SKIP {:04X}.{:04X}", first, last), mode);



    action.command.a1    = first;
    action.command.a2    = last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetAddBookmark
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetAddBookmark (Word address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::AddBookmark, std::format ("BMA {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearBookmark
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearBookmark (int id, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearBookmark, std::format ("BMC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetStopwatch
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetStopwatch (Word start, Word stop, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::Stopwatch, std::format ("STOPWATCH {:04X} {:04X}", start, stop), mode);



    action.command.text = std::format ("{:04X} {:04X}", start, stop);
    action.isResult     = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetCallSubroutine
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetCallSubroutine (Word address, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::CallSubroutine, std::format ("JSR {:04X}", address), mode);



    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetEnableSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetEnableSymbols (const std::string & table, bool enable, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::EnableSymbols, std::format ("SYM{} {}", table, enable ? "ON" : "OFF"), mode);



    action.command.count       = enable ? 1 : 0;
    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetClearSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetClearSymbols (const std::string & table, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::ClearSymbols, std::format ("SYM{} CLEAR", table), mode);



    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetSaveSymbols
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetSaveSymbols (const std::string & table, const std::string & path, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SaveSymbols, std::format ("SYM{} SAVE \"{}\"", table, path), mode);



    action.command.text        = std::format ("\"{}\"", path);
    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetRemoveSymbol
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetRemoveSymbol (const std::string & table, const std::string & name, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::RemoveSymbol, std::format ("SYM{} ! {}", table, name), mode);



    action.command.text        = name;
    action.command.symbolTable = table;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetProfile
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetProfile (const std::string & words, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::Profile, "PROFILE " + words, mode);



    action.command.text = words;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetRemoveSkip
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetRemoveSkip (const StepFilterEntry & entry, CommandMode mode)
{
    std::string     text   = !entry.name.empty()         ? entry.name
                           : (entry.first == entry.last) ? std::format ("{:04X}", entry.first)
                           :                               std::format ("{:04X}.{:04X}", entry.first, entry.last);
    DebuggerAction  action = Make (DebugVerb::RemoveStepFilter, "SKIP -" + text, mode);



    action.command.text  = text;
    action.command.a1    = entry.first;
    action.command.a2    = entry.last;
    action.command.hasA1 = true;
    action.command.hasA2 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::GetCompareMemory
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetCompareMemory (Word other, Word first, Word last, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::CompareMemory, std::format ("MC {:04X} {:04X}:{:04X}", other, first, last), mode);



    SetRangeFrom (action.command, other, first, last);
    action.isResult = true;
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
    action.command.text  = std::format ("{:04X}", address);
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
//  DebuggerActions::GetNameBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction DebuggerActions::GetNameBreakpoint (int id, const std::string & name, CommandMode mode)
{
    DebuggerAction  action = Make (DebugVerb::SetBreakpointName,
                                   name.empty() ? std::format ("BPNAME {}", id) : std::format ("BPNAME {} \"{}\"", id, name), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = name;
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





//  The verbs a watch edit's action can have, each by the AppleWin command it
//  is, which a saved action has in place of the verb's number.
static constexpr std::pair<DebugVerb, const char *>  s_kSavedVerbs[] =
{
    { DebugVerb::AddWatch,    "W"   },
    { DebugVerb::ClearWatch,  "WC"  },
    { DebugVerb::EnterBytes,  "MEB" },
    { DebugVerb::EnterWords,  "MEW" },
    { DebugVerb::SetRegister, "R"   },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::ToText
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerActions::ToText (const DebuggerAction & action)
{
    const DebugCommand  & command = action.command;



    for (const auto & [verb, name] : s_kSavedVerbs)
    {
        if (verb == command.verb)
        {
            return std::format ("{}.{:X}.{}.{:X}.{}.{}.{}.{}.{}", name, command.a1, command.hasA1 ? 1 : 0, command.count,
                                UndoText::FormatBytes (command.values), UndoText::FormatBytes (command.mask),
                                UndoText::FormatText (action.operand), UndoText::FormatText (command.text), UndoText::FormatText (action.echo));
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions::TryFromText
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerActions::TryFromText (const std::string & text, DebuggerAction & action)
{
    std::vector<std::string>  fields = UndoText::Split (text, '.');
    const auto              * found  = std::ranges::end (s_kSavedVerbs);
    uint32_t                  a1     = 0;
    uint32_t                  count  = 0;
    size_t                    start  = 0;



    action = {};

    if (fields.size() != 9)
    {
        return false;
    }

    found = std::ranges::find_if (s_kSavedVerbs, [&fields] (const auto & each) { return fields[0] == each.second; });

    if (found == std::ranges::end (s_kSavedVerbs) || !UndoText::TryParseHex (fields[1], a1) || a1 > 0xFFFF ||
        (fields[2] != "0" && fields[2] != "1") || !UndoText::TryParseHex (fields[3], count) ||
        !UndoText::TryParseBytes (fields[4], action.command.values) || !UndoText::TryParseBytes (fields[5], action.command.mask) ||
        !UndoText::TryParseText (fields[6], action.operand) || !UndoText::TryParseText (fields[7], action.command.text) ||
        !UndoText::TryParseText (fields[8], action.echo) || action.echo.empty())
    {
        action = {};
        return false;
    }

    start                     = action.echo.starts_with ("/") ? 1 : 0;
    action.command.verb       = found->first;
    action.command.sourceName = action.echo.substr (start, action.echo.find (' ', start) - start);
    action.command.a1         = (Word) a1;
    action.command.hasA1      = fields[2] == "1";
    action.command.count      = count;
    return true;
}
