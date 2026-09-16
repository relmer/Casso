#pragma once

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  IInstructionObserver
//
//  Told of each instruction a debugger-driven run is about to execute, before
//  it executes. The session forwards the hook's report only while a run it
//  started is in progress, so a machine running freely with a breakpoint set
//  pays nothing for tracing, profiling or the branch record.
//
////////////////////////////////////////////////////////////////////////////////

class IInstructionObserver
{
public:
    virtual ~IInstructionObserver() = default;

    virtual void  OnInstruction (DebugSession & session, Word pc) = 0;
};
