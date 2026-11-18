#pragma once

struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarEntryStates
//
//  Which of the command bar's entries are enabled for a snapshot.
//
////////////////////////////////////////////////////////////////////////////////

class CommandBarEntryStates
{
public:
    static bool  IsEnabled (int id, const DebuggerViewSnapshot * snapshot, bool isReplaying, bool hasSelectedRow);
};
