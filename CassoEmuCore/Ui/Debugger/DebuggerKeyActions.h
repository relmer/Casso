#pragma once

#include "Debugger/DebugCommand.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"

struct DebuggerAction;
struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeyActions
//
//  The action each keyboard-scheme key takes, and which keys step and which
//  run the machine.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerKeyActions
{
public:
    //  The action a keyboard-scheme action takes, which is the action its
    //  button takes. The cursor actions use the selected code line (toggling
    //  falls back to the PC's line); Pause has none, since it is the channel's
    //  pause rather than a command. A step, forward or back, takes nothing
    //  while the snapshot shows the machine running.
    static std::optional<DebuggerAction>  GetForKey (DebuggerKeySchemes::Action   action,
                                                     const DebuggerViewSnapshot * snapshot,
                                                     int                          selectedRow,
                                                     CommandMode                  mode);

    //  Step into, over and out, run to cursor, run one frame and the step
    //  back keys: the actions that need a paused machine.
    static bool  IsStepAction (DebuggerKeySchemes::Action action);

    //  Run, reverse run and run to cursor: the actions after which the
    //  machine runs, whose key pauses it while it runs.
    static bool  IsRunAction  (DebuggerKeySchemes::Action action);
};
