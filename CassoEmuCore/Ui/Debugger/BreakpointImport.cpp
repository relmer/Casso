#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Ui/Debugger/BreakpointImport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointImport::Run
//
//  Each definition line's new breakpoints are found as the ids that were not
//  there before it, in order, so the script's numbers map onto them.
//
////////////////////////////////////////////////////////////////////////////////

int BreakpointImport::Run (DebugSession & session, const std::string & script, const LineRunner & run)
{
    std::istringstream       lines (script);
    std::string              line;
    std::vector<int>         created;
    int                      skipped = 0;



    while (std::getline (lines, line))
    {
        std::istringstream       rest (line);
        std::string              name;
        const AppleWinCommand  * entry = nullptr;
        std::vector<int>         before;
        std::vector<int>         after;



        rest >> name;

        if (name.empty())
        {
            continue;
        }

        entry = AppleWinCommandTable::Find (name);

        if (entry != nullptr && IsDefinition (entry->verb))
        {
            before = GetIds (session);
            run (line.substr (line.find_first_not_of (" \t")));
            after  = GetIds (session);

            std::ranges::copy_if (after, std::back_inserter (created), [&before] (int id) { return std::ranges::find (before, id) == before.end(); });
            continue;
        }

        if (entry == nullptr || !TryApplyFlags (entry->name, rest, created, run))
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
    case DebugVerb::SetBreakpointAndWatchpoint:
    case DebugVerb::SetRegisterBreakpoint:
    case DebugVerb::SetMemoryWatchpoint:
    case DebugVerb::SetReadWatchpoint:
    case DebugVerb::SetWriteWatchpoint:
    case DebugVerb::SetValueBreakpoint:
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
//  BreakpointImport::TryApplyFlags
//
//  BPD n, BPE n and BPCHANGE n flags, with n the script's own number for a
//  breakpoint, sent to the breakpoint that number became. Anything else, or
//  a number the script never reached, is not applied.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointImport::TryApplyFlags (const std::string & name, std::istringstream & rest, const std::vector<int> & created, const LineRunner & run)
{
    std::string  number;
    std::string  flags;
    int          index  = -1;
    bool         isFlag = name == "BPCHANGE";



    if (!isFlag && name != "BPD" && name != "BPE")
    {
        return false;
    }

    rest >> number >> flags;

    if (number.empty() || !std::ranges::all_of (number, [] (char ch) { return isdigit ((unsigned char) ch) != 0; }))
    {
        return false;
    }

    index = std::stoi (number);

    if (index < 0 || index >= (int) created.size() || (isFlag && flags.empty()))
    {
        return false;
    }

    run (isFlag ? std::format ("{} {} {}", name, created[(size_t) index], flags) : std::format ("{} {}", name, created[(size_t) index]));
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





