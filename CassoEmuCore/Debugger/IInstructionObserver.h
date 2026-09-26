#pragma once

#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  IInstructionObserver
//
//  Told of each instruction a debugger-driven run is about to execute, before
//  it executes, of every stop and of a machine switch. The session forwards
//  the hook's report only while a run it started is in progress, so a machine
//  running freely with a breakpoint set pays nothing for tracing, profiling
//  or the branch record.
//
//  OnInterrupt stands in for OnInstruction when an interrupt is dispatched in
//  place of the instruction at the PC, which then runs only after the return.
//  OnFreeRunSlice is told after each slice a machine running freely executes.
//
////////////////////////////////////////////////////////////////////////////////

class IInstructionObserver
{
public:
    virtual ~IInstructionObserver() = default;

    virtual void  OnInstruction    (DebugSession & session, Word pc) = 0;
    virtual void  OnInterrupt      (DebugSession &) {}
    virtual void  OnFreeRunSlice   (DebugSession &) {}
    virtual void  OnRunStopped     (DebugSession &, const StopEvent &) {}
    virtual void  OnMachineChanged (DebugSession &) {}
};
