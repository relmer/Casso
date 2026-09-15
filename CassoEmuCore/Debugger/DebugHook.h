#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DebugHook
//
//  Consulted by MachineHost before each instruction while a debugger has a
//  stop condition or a run in progress. ShouldStopBefore stops the machine
//  before the instruction at pc executes. HasPendingStop reports a stop raised
//  during the instruction that just ran, such as a watchpoint hit, which takes
//  effect at the next instruction boundary.
//
////////////////////////////////////////////////////////////////////////////////

class DebugHook
{
public:
    virtual ~DebugHook() = default;

    virtual bool  ShouldStopBefore (Word pc)  = 0;
    virtual bool  HasPendingStop   () const   = 0;
};
