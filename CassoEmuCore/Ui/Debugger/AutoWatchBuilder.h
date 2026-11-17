#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/InstructionTouches.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  AutoWatchBuilder
//
//  The automatic watches of a stopped machine: what the instruction at the
//  PC touches and, after a step, what the instruction that just ran touched.
//
////////////////////////////////////////////////////////////////////////////////

class AutoWatchBuilder
{
public:
    //  Notes the stop a build arrives at. Before the code views, which mark
    //  the line a step just ran.
    void  UpdateLastStop   (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildAutoWatches (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

private:
    static bool  IsSameRegisters (const Cpu6502Registers & left, const Cpu6502Registers & right);
    static void  AddAutoWatches  (DebugSession & session, const InstructionTouches::Result & touches,
                                  const Cpu6502Registers & now, bool isPrevious,
                                  std::vector<DebuggerViewSnapshot::AutoWatchLine> & lines);

    //  The instruction at the last stop and where it would leave the
    //  machine. At the next stop, if the registers are exactly that, it IS
    //  the instruction that just ran -- a step -- and its touches are shown
    //  as the previous instruction's. After a free run they are not, and it
    //  is left out rather than shown for an instruction that did not just run.
    //
    //  A paused machine rebuilds on every click, so the stop is recognized by
    //  its registers: the same registers are the same stop, and what was
    //  worked out on arriving there stands.
    struct LastStop
    {
        Cpu6502Registers                           at       = {};
        InstructionTouches::Result                 here;
        std::optional<InstructionTouches::Result>  previous;
        bool                                       isValid  = false;
    };

    mutable LastStop  m_lastStop;
};
