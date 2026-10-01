#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugSession.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/WinDbgParser.h"
#include "Ui/Debugger/BreakpointImport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::Run
//
//  Each line is read in the script's current dialect first, then in
//  AppleWin's, WinDbg's and GSSquared's. The first dialect that reads it as
//  setting a breakpoint runs it; only when none does is it read as a change
//  to one, so GSSquared's `bpd 400 r` is a read breakpoint rather than
//  AppleWin's disable of breakpoint 400. Each definition line's new
//  breakpoints are found as the ids that were not there before it, in order,
//  so the script's numbers map onto them.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointImport::Run (DebugSession & session, const std::string & script, const LineRunner & run)
{
    std::istringstream  lines (script);
    std::string         line;
    std::vector<int>    created;
    CommandMode         mode    = CommandMode::AppleWin;
    int                 skipped = 0;



    while (std::getline (lines, line))
    {
        size_t                    first = line.find_first_not_of (" \t\r");
        size_t                    last  = line.find_last_not_of  (" \t\r");
        std::vector<CommandMode>     order;
        DebugCommand                 command;
        std::optional<CommandMode>   setter;
        std::optional<DebugCommand>  change;
        std::vector<int>             before;
        std::vector<int>             after;



        if (first == std::string::npos)
        {
            continue;
        }

        line = line.substr (first, last - first + 1);

        if (TryReadMode (line, mode))
        {
            continue;
        }

        order = { mode, CommandMode::AppleWin, CommandMode::WinDbg, CommandMode::GSSquared };

        for (CommandMode candidate : order)
        {
            if (setter.has_value() || !TryParse (session, line, candidate, command))
            {
                continue;
            }

            if (IsDefinition (command.verb))
            {
                setter = candidate;
            }
            else if (!change.has_value())
            {
                change = command;
            }
        }

        if (setter.has_value())
        {
            before = GetIds (session);
            run (line, *setter);
            after  = GetIds (session);

            std::ranges::copy_if (after, std::back_inserter (created), [&before] (int id) { return std::ranges::find (before, id) == before.end(); });
            continue;
        }

        if (!change.has_value() || !TryApply (line, *change, created, run))
        {
            ++skipped;
        }
    }

    return skipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::IsDefinition
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::IsDefinition (DebugVerb verb)
{
    switch (verb)
    {
    case DebugVerb::SetBreakpoint:
    case DebugVerb::SetConditionalBreakpoint:
    case DebugVerb::SetBreakpointAndWatchpoint:
    case DebugVerb::SetRegisterBreakpoint:
    case DebugVerb::SetMemoryWatchpoint:
    case DebugVerb::SetReadWatchpoint:
    case DebugVerb::SetWriteWatchpoint:
    case DebugVerb::SetValueBreakpoint:
    case DebugVerb::SetSourceBreakpoint:
    case DebugVerb::BreakOnBrk:
    case DebugVerb::BreakOnOpcode:
    case DebugVerb::BreakOnInterrupt:
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::TryReadMode
//
//  `MODE name`, bare or behind a dialect's engine marker.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::TryReadMode (const std::string & line, CommandMode & mode)
{
    std::istringstream  words (line);
    std::string         verb;
    std::string         name;
    std::string         extra;
    CommandMode         parsed = CommandMode::AppleWin;



    words >> verb >> name >> extra;

    if (!verb.empty() && (verb[0] == '/' || verb[0] == '!'))
    {
        verb.erase (0, 1);
    }

    std::ranges::transform (verb, verb.begin(), [] (char ch) { return (char) toupper ((unsigned char) ch); });

    if (verb != "MODE" || name.empty() || !extra.empty() || !CommandModeNames::TryParse (name, parsed))
    {
        return false;
    }

    mode = parsed;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::TryParse
//
//  The line's first command in one dialect, without running it. Casso's set
//  reads as AppleWin's, and the Monitor has no breakpoint commands.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::TryParse (DebugSession & session, const std::string & line, CommandMode mode, DebugCommand & command)
{
    bool  isParsed = false;



    switch (mode)
    {
    case CommandMode::AppleWin:
    case CommandMode::Casso:
    {
        AppleWinParseResult  result = AppleWinParser::Parse (line, session);

        isParsed = result.status == ParseStatus::Ok;
        command  = result.command;
        break;
    }

    case CommandMode::WinDbg:
    {
        WinDbgParseResult  result = WinDbgParser::Parse (line, session);

        isParsed = result.status == ParseStatus::Ok;
        command  = result.command;
        break;
    }

    case CommandMode::GSSquared:
    {
        GSSquaredParseResult  result = GSSquaredParser::Parse (line, session);

        isParsed = result.status == ParseStatus::Ok && !result.commands.empty();

        if (isParsed)
        {
            command = result.commands.front();
        }

        break;
    }

    default:
        break;
    }

    return isParsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::TryApply
//
//  A disable or enable of the script's breakpoint n, sent to the breakpoint
//  that number became. A change of flags is read in AppleWin's BPCHANGE
//  form. Anything else, all-breakpoint forms among them, or a number the
//  script never reached, is not applied.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::TryApply (const std::string & line, const DebugCommand & command, const std::vector<int> & created, const LineRunner & run)
{
    const char  * verb = nullptr;



    if (command.verb == DebugVerb::ChangeBreakpoint)
    {
        return TryApplyFlags (line, created, run);
    }

    if (command.verb == DebugVerb::DisableBreakpoint)
    {
        verb = "BPD";
    }
    else if (command.verb == DebugVerb::EnableBreakpoint)
    {
        verb = "BPE";
    }

    if (verb == nullptr || command.text == "*" || command.count >= created.size())
    {
        return false;
    }

    run (std::format ("{} {}", verb, created[command.count]), CommandMode::AppleWin);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::TryApplyFlags
//
//  BPCHANGE n flags, with n the script's own number for a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::TryApplyFlags (const std::string & line, const std::vector<int> & created, const LineRunner & run)
{
    std::istringstream  words (line);
    std::string         name;
    std::string         number;
    std::string         flags;
    int                 index  = -1;



    words >> name >> number >> flags;

    if (flags.empty() || number.empty() || !std::ranges::all_of (number, [] (char ch) { return isdigit ((unsigned char) ch) != 0; }))
    {
        return false;
    }

    index = std::stoi (number);

    if (index < 0 || index >= (int) created.size())
    {
        return false;
    }

    run (std::format ("BPCHANGE {} {}", created[(size_t) index], flags), CommandMode::AppleWin);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::GetIds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> BreakpointImport::GetIds (DebugSession & session)
{
    BreakpointListData  list;
    std::vector<int>    ids;



    BreakpointHandlers::ListAll (session, list);

    for (const BreakpointInfo & info : list.breakpoints)
    {
        ids.push_back (info.id);
    }

    return ids;
}
