#include "Pch.h"

#include "Ui/Debugger/CommandBarEntryStates.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarEntryStates::IsEnabled
//
//  A running machine has no state to step through, so everything that acts
//  on a stopped one is off while it runs, and Pause is off while it is
//  already stopped. Run to Cursor needs a line to run to as well. The
//  choices that only change what the window shows stay live throughout.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandBarEntryStates::IsEnabled (int id, const DebuggerViewSnapshot * snapshot, bool isReplaying, bool hasSelectedRow)
{
    bool  paused = snapshot != nullptr && snapshot->isPaused;



    //  While history recording is on, the trace is rebuilt from history, so
    //  there is nothing to turn on or off.
    if (id == DebuggerCommands::kTrace)
    {
        return snapshot == nullptr || !snapshot->history.isRecording;
    }

    if (id == DebuggerCommands::kRun)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunOrPause)
    {
        return IsEnabled (DebuggerCommands::kRun, snapshot, isReplaying, hasSelectedRow) || IsEnabled (DebuggerCommands::kPause, snapshot, isReplaying, hasSelectedRow);
    }

    //  A replay runs with the machine stopped, and Pause stops it.
    if (id == DebuggerCommands::kPause)
    {
        return !paused || isReplaying;
    }

    //  A load writes memory, which BLOAD does only with the machine stopped.
    if (id == DebuggerCommands::kStepInto || id == DebuggerCommands::kStepOver ||
        id == DebuggerCommands::kStepOut  || id == DebuggerCommands::kShowNext ||
        id == DebuggerCommands::kRunFrame || id == DebuggerCommands::kLoadFile)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunToCursor)
    {
        return paused && hasSelectedRow;
    }

    //  Moving through history needs a stopped machine and a recording, and
    //  going live needs the machine to be behind live.
    if (id == DebuggerCommands::kStepBackInto || id == DebuggerCommands::kStepBackOver ||
        id == DebuggerCommands::kStepBackOut  || id == DebuggerCommands::kReverseContinue)
    {
        return paused && snapshot->history.isRecording;
    }

    if (id == DebuggerCommands::kGoLive)
    {
        return paused && snapshot->history.isBehindLive;
    }

    return true;
}
