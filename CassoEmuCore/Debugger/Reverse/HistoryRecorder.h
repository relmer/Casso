#pragma once

#include "Pch.h"
#include "Cpu6502.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryRecorder
//
//  Told of every instruction the machine is about to execute, after a debug
//  hook has let it through, so reverse execution can take its snapshots on
//  instruction boundaries. An interrupt dispatched in place of an opcode
//  fetch counts as one instruction. Called on the thread that runs the
//  machine.
//
//  The per-instruction part is forced inline and not virtual: it is one
//  compare of the cycle count, and only a snapshot falling due, a few times
//  a second, reaches the derived class through OnCaptureDue. Left to the
//  compiler it stayed a call.
//
//  Also told when a disk is mounted, ejected, swapped for a changed file or
//  has the write protection in its file changed, after the change: the disks
//  the machine holds do not follow from its earlier state, so history marks
//  the change as a boundary there. This is rare, so it stays virtual.
//
//  Also told when the debugger edits memory, registers or I/O state, which
//  no recorded input reproduces.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryRecorder
{
public:
    virtual             ~HistoryRecorder() = default;

    __forceinline void  OnInstructionStart (const Cpu6502 & cpu)
    {
        uint64_t  cycle = cpu.Cpu6502::GetCycleCount();      // qualified: no virtual call



        if (cycle >= m_nextDueCycle)
        {
            OnCaptureDue (cycle);
        }
    }

    //  Behind live, after each instruction the machine runs itself, so the
    //  inputs recorded where it arrives are applied there, as a replay does.
    __forceinline void  OnInstructionEnd()
    {
        if (m_isArrivalDue)
        {
            OnArrived();
        }
    }

    virtual void  OnMediaChanged  (MachineHost & machine) = 0;
    virtual void  OnMachineEdited (MachineHost & machine) = 0;

protected:
    virtual void        OnCaptureDue (uint64_t cycle) = 0;
    virtual void        OnArrived    () {}

    uint64_t  m_nextDueCycle = 0;     // the first cycle at which OnCaptureDue is called
    bool      m_isArrivalDue = false; // whether OnArrived is called after each instruction
};
