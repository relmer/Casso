#include "Pch.h"

#include "Ui/Debugger/DebuggerCommandLines.h"

#include "Debugger/MonitorParser.h"
#include "Debugger/MonitorState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetActionLine
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::string> DebuggerCommandLines::GetActionLine (
    DebuggerKeySchemes::Action   action,
    const DebuggerViewSnapshot * snapshot,
    int                          selectedRow)
{
    using Action = DebuggerKeySchemes::Action;

    std::optional<std::string>  line;
    std::optional<Word>         selected;
    std::optional<Word>         current;



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
    case Action::Run:      line = GetRunLine();      break;
    case Action::StepInto: line = GetStepLine();     break;
    case Action::StepOver: line = GetStepOverLine(); break;
    case Action::StepOut:  line = GetStepOutLine();  break;

    case Action::RunToCursor:
        if (selected.has_value())
        {
            line = GetRunToCursorLine (*selected);
        }

        break;

    case Action::ToggleBreakpoint:
        if (snapshot != nullptr && current.has_value())
        {
            line = GetToggleBreakpointLine (*snapshot, *current);
        }

        break;

    case Action::Pause:
    default:
        break;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetModeLine
//
//  The controls build AppleWin lines and the session reads lines in its own
//  mode. Monitor mode reads an AppleWin line after its `/`; GSSquared has its
//  own words for most of what they send. WinDbg has a word for each. Run to
//  cursor does not come through here, since it runs in Casso mode.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetModeLine (const std::string & line, CommandMode mode)
{
    size_t       space = line.find (' ');
    std::string  name  = line.substr (0, space);
    std::string  rest  = (space == std::string::npos) ? std::string() : line.substr (space + 1);
    size_t       split = rest.find (' ');



    if (mode == CommandMode::Monitor)
    {
        return "/" + line;
    }

    if (mode == CommandMode::WinDbg)
    {
        return GetWinDbgLine (name, rest, line);
    }

    if (mode != CommandMode::GSSquared)
    {
        return line;
    }

    if (rest.empty())
    {
        if (name == "T")   { return "s"; }
        if (name == "P")   { return "o"; }
        if (name == "RTS") { return "r"; }
        if (name == "G")   { return "g"; }
    }

    if (name == "BP")  { return "bp " + rest; }
    if (name == "BPC") { return "nobp " + rest; }

    //  The deposit takes hex bytes only; any other value stays an AppleWin
    //  MEB, which GSSquared mode runs too.
    if (name == "MEB" && split != std::string::npos && IsHexBytes (rest.substr (split)))
    {
        return rest.substr (0, split) + ":" + rest.substr (split);
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::IsHexBytes
//
//  True when text is one or more values of one or two hex digits each.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommandLines::IsHexBytes (const std::string & text)
{
    static constexpr size_t  kMaxDigits = 2;
    std::istringstream       values (text);
    std::string              value;
    bool                     any = false;



    while (values >> value)
    {
        if (value.size() > kMaxDigits || value.find_first_not_of ("0123456789ABCDEFabcdef") != std::string::npos)
        {
            return false;
        }

        any = true;
    }

    return any;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetWinDbgLine
//
//  A control's AppleWin line in WinDbg's words: t, p, gu, g, bp, bc and eb.
//  Any other goes through WinDbg mode's engine marker, `!`, which is how
//  that mode reaches Casso's own commands (FR-014) -- MODE among them.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetWinDbgLine (const std::string & name, const std::string & rest, const std::string & line)
{
    static constexpr std::pair<const char *, const char *>  kWords[] =
    {
        { "T", "t" }, { "P", "p" }, { "RTS", "gu" }, { "G", "g" }, { "BP", "bp" }, { "BPC", "bc" }, { "MEB", "eb" },
    };



    for (const auto & [appleWin, windbg] : kWords)
    {
        if (name == appleWin)
        {
            return rest.empty() ? std::string (windbg) : std::string (windbg) + " " + rest;
        }
    }

    return "!" + line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetConsoleKeyAction
//
//  GSSquared steps and resumes by key, not by command, so a reader used to it
//  presses Space at an empty prompt. Once something is typed the keys type.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerKeySchemes::Action> DebuggerCommandLines::GetConsoleKeyAction (
    CommandMode  mode,
    WPARAM       vk,
    bool         ctrl,
    bool         alt,
    bool         shift,
    bool         isLineEmpty)
{
    if (mode != CommandMode::GSSquared || !isLineEmpty || ctrl || alt || shift)
    {
        return std::nullopt;
    }

    if (vk == VK_SPACE || vk == VK_F10)
    {
        return DebuggerKeySchemes::Action::StepInto;
    }

    if (vk == VK_RETURN)
    {
        return DebuggerKeySchemes::Action::Run;
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::DoesAssemblerKeepKey
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommandLines::DoesAssemblerKeepKey (const DebuggerViewSnapshot * snapshot, WPARAM vk, bool ctrl, bool alt)
{
    return snapshot != nullptr && snapshot->isAssembling && !ctrl && !alt && (vk == VK_RETURN || vk == VK_SPACE);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::DoesConsoleKeepKey
//
//  The Monitor reads an empty line as a request for the next row of bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommandLines::DoesConsoleKeepKey (CommandMode mode, WPARAM vk, bool ctrl, bool alt)
{
    return mode == CommandMode::Monitor && vk == VK_RETURN && !ctrl && !alt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetToggleBreakpointLine
//
//  A click sets a breakpoint where there is none and clears the one that is
//  there. Clearing goes by id, so a click never clears a different breakpoint
//  that happens to cover the same address.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address)
{
    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (IsCodeBreakpointAt (bp, address))
        {
            return std::format ("BPC {}", bp.id);
        }
    }

    return std::format ("BP {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::IsCodeBreakpointAt
//
//  An execution breakpoint listed at the address. Watchpoints and the entries
//  that stop on an opcode, a register, BRK or an interrupt list an address
//  too, but none of them stops before the instruction there.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommandLines::IsCodeBreakpointAt (const DebuggerViewSnapshot::BreakpointLine & bp, Word address)
{
    return bp.info.kind == BreakpointKind::Address && bp.address == address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::IsDiskRoutineAt
//
//  An RWTS or driver breakpoint resolved to the routine at the address: the
//  gutter marks it, and toggling the line adds an execution breakpoint
//  beside it rather than clearing it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerCommandLines::IsDiskRoutineAt (const DebuggerViewSnapshot::BreakpointLine & bp, Word address)
{
    return bp.info.resolved.has_value() && *bp.info.resolved == address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetPokeLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetPokeLine (Word address, Byte value)
{
    return std::format ("MEB {:04X} {:02X}", address, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetRunToCursorLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetRunToCursorLine (Word address)
{
    return std::format ("G {:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetPanelLine
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetPanelLine (const std::string & id, bool open)
{
    return std::format ("PANEL {}{}", open ? "" : "CLOSE ", id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetMissingFileVerb
//
//  ReadFile or WriteFile when a Monitor line holds an R or W with no file
//  name, which the window asks for before the line runs. Batch and the pipe
//  have no one to ask, so there the handler reports the error instead. A line
//  typed while the session is assembling is an instruction, not a command.
//
//  The line is parsed against a scratch state: a range comes from the line
//  itself, and the session's own state must not move for a line that has not
//  run yet.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebugVerb> DebuggerCommandLines::GetMissingFileVerb (const std::string & line, CommandMode mode, bool isAssembling)
{
    MonitorState        scratch;
    MonitorParseResult  parsed;



    if (mode != CommandMode::Monitor || isAssembling)
    {
        return std::nullopt;
    }

    parsed = MonitorParser::Parse (line, scratch);

    for (const DebugCommand & command : parsed.commands)
    {
        if ((command.verb == DebugVerb::ReadFile || command.verb == DebugVerb::WriteFile) && command.text.empty())
        {
            return command.verb;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerCommandLines::GetLineWithFileName
//
//  Quoted, so a path with spaces stays one name; the Monitor parser takes the
//  quotes off again.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerCommandLines::GetLineWithFileName (const std::string & line, const std::string & path)
{
    return std::format ("{}\"{}\"", line, path);
}
