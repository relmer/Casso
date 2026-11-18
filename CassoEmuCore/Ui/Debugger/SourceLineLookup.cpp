#include "Pch.h"

#include "Ui/Debugger/SourceLineLookup.h"
#include "Ui/Debugger/BreakpointActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineLookup::GetArrowTarget
//
//  The first line in the file that starts at the branch's target. With none,
//  the target lies below when its address is past the marked line's.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceLineLookup::GetArrowTarget (const DebuggerViewSnapshot::SourceState & state, int fileId, int markedLine,
                                       std::optional<int> & outTargetLine, bool & outIsBelow)
{
    std::optional<Word>  markedAt;



    outTargetLine.reset();

    if (!state.pcTarget.has_value() || markedLine <= 0 || state.lineAddresses == nullptr)
    {
        return false;
    }

    for (const auto & [place, address] : *state.lineAddresses)
    {
        if (place.first != fileId)
        {
            continue;
        }

        if (address == *state.pcTarget && !outTargetLine.has_value())
        {
            outTargetLine = place.second;
        }

        if (place.second == markedLine)
        {
            markedAt = address;
        }
    }

    if (outTargetLine.has_value())
    {
        outIsBelow = *outTargetLine > markedLine;
    }
    else
    {
        outIsBelow = !markedAt.has_value() || *state.pcTarget > *markedAt;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineLookup::GetToggleLine
//
//  Empty for a file with no record in the debug file, which has no line a
//  breakpoint could be set on.
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceLineLookup::GetToggleLine (const DebuggerViewSnapshot::SourceState & state, int fileId, int line)
{
    std::optional<DebuggerAction>  action = GetToggleAction (state, fileId, line, CommandMode::AppleWin);



    return action.has_value() ? action->echo : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineLookup::GetToggleAction
//
//  Clears the breakpoint on the line by its id, or sets one there. Nothing
//  for a file with no record in the debug file.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> SourceLineLookup::GetToggleAction (const DebuggerViewSnapshot::SourceState & state, int fileId, int line, CommandMode mode)
{
    for (const std::tuple<int, int, int> & bp : state.breakpointLines)
    {
        if (std::get<0> (bp) == fileId && std::get<1> (bp) == line)
        {
            return BreakpointActions::GetClearBreakpoint (std::get<2> (bp), mode);
        }
    }

    for (const DebugSourceFile & record : state.files)
    {
        if (record.id == fileId)
        {
            return BreakpointActions::GetSourceBreakpoint (record.name, line, mode);
        }
    }

    return std::nullopt;
}
