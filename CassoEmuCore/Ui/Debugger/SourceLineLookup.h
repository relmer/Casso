#pragma once

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceLineLookup
//
//  Queries of the snapshot's source state for a line of a file: where the
//  PC's branch goes, and the command a double click on the line sends.
//
////////////////////////////////////////////////////////////////////////////////

class SourceLineLookup
{
public:
    //  The line the PC's branch goes to in a file, if a line there starts at
    //  it, and whether it lies below the marked line; false for no branch.
    static bool  GetArrowTarget (const DebuggerViewSnapshot::SourceState & state, int fileId, int markedLine,
                                 std::optional<int> & outTargetLine, bool & outIsBelow);

    //  The command a double click on a line sends: clearing the breakpoint
    //  already on it, or setting one.
    static std::string                    GetToggleLine   (const DebuggerViewSnapshot::SourceState & state, int fileId, int line);
    static std::optional<DebuggerAction>  GetToggleAction (const DebuggerViewSnapshot::SourceState & state, int fileId, int line, CommandMode mode);
};
