#pragma once

class IWatchSink;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  IReverseStopTest
//
//  The debugger's stop conditions, asked about every instruction a replay
//  runs, so running backward stops where running forward would have. The
//  machine is in the state that instruction begins with, so a breakpoint
//  condition or a memory comparison reads the restored machine.
//
//  ShouldStopBefore is a breakpoint: true stops before the instruction at
//  pc. TakePendingStop is a watchpoint: true if the instruction just run
//  raised a stop for the next boundary, and the pending stop is cleared.
//  GetWatchSink is where the bus reports watched accesses during the
//  replay, in place of the debugger's own sink, or null for none. None of
//  them may log, count a hit or stop anything itself; the reverse command
//  decides which hit it lands on.
//
////////////////////////////////////////////////////////////////////////////////

class IReverseStopTest
{
public:
    virtual               ~IReverseStopTest() = default;

    virtual bool          ShouldStopBefore (MachineHost & machine, Word pc) = 0;
    virtual bool          TakePendingStop  ()                               = 0;
    virtual IWatchSink  * GetWatchSink     ()                               { return nullptr; }
};
