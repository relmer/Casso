#pragma once

#include "Ui/Debugger/DebuggerActions.h"

struct StepFilterEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  RunActions
//
//  The actions that run the machine: run, the steps forward and back, run to
//  a line or one frame, BASIC steps, a call of a routine, the stopwatch, the
//  step filter and stepping by source line.
//
////////////////////////////////////////////////////////////////////////////////

class RunActions
{
public:
    static DebuggerAction  GetRun              (CommandMode mode);
    static DebuggerAction  GetStepInto         (CommandMode mode);
    static DebuggerAction  GetStepOver         (CommandMode mode);
    static DebuggerAction  GetStepOut          (CommandMode mode);

    //  BASICSTEP, BASICLINE or BASICOUT, by its verb.
    static DebuggerAction  GetBasicStep        (DebugVerb verb, CommandMode mode);

    //  T-, P-, GU-, G- or LIVE: the verb is one of the reverse verbs.
    static DebuggerAction  GetReverse          (DebugVerb verb, CommandMode mode);

    //  Runs to the address. Its echo is Casso's G, whatever the mode, since
    //  GSSquared's g takes no address.
    static DebuggerAction  GetRunToCursor      (Word address);
    static DebuggerAction  GetRunFrame         ();

    //  RCC, which starts the trip count again.
    static DebuggerAction  GetResetCycles    (CommandMode mode);
    static DebuggerAction  GetSkipRoutine    (const std::string & name, Word address, CommandMode mode);
    static DebuggerAction  GetSkipRange      (Word first, Word last, CommandMode mode);
    static DebuggerAction  GetStopwatch      (Word start, Word stop, CommandMode mode);
    static DebuggerAction  GetCallSubroutine (Word address, CommandMode mode);
    static DebuggerAction  GetRemoveSkip     (const StepFilterEntry & entry, CommandMode mode);

    //  SRC ON or OFF: stepping by source line or by instruction.
    static DebuggerAction  GetSourceStepping   (bool isSource, CommandMode mode);
};
