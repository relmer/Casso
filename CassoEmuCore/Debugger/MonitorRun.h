#pragma once

#include "Pch.h"

#include "Debugger/Reply.h"

class DebugStopMonitor;
class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorRun
//
//  The Monitor's run conventions. A Monitor `G` pushes the address of the
//  code that re-enters the Monitor, as the ROM's G does, so an RTS from the
//  program lands back at the prompt, and the session stops there. The
//  original ]['s step display is the instruction the step runs.
//
////////////////////////////////////////////////////////////////////////////////

class MonitorRun
{
public:
    //  The address the ROM's G returns to.
    static constexpr Word  kMonitorReentry = 0xFF69;
    static constexpr Word  kStackPage      = 0x0100;

    //  The stack bytes and registers a push replaced, for Undo.
    struct Pushed
    {
        Cpu6502Registers  before    = {};
        Byte              stackHigh = 0;
        Byte              stackLow  = 0;
        bool              isPushed  = false;
    };

    //  Pushes the return address and arms the stop at it; returns what the
    //  push replaced. Undo puts that back and disarms the stop, for a run
    //  that never started.
    static Pushed           Push        (IDebugTarget & target, DebugStopMonitor & stops);
    static void             Undo        (IDebugTarget & target, DebugStopMonitor & stops, const Pushed & pushed);

    //  The instruction at PC, disassembled, for a Monitor step's display.
    static DisassemblyData  GetStepLine (IDebugTarget & target);
};
