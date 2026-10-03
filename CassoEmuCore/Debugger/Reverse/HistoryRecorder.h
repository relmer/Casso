#pragma once

#include "Pch.h"
#include "Cpu6502.h"
#include "Debugger/Reverse/UndoRing.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryRecorder
//
//  Told of every instruction the machine is about to execute, after a debug
//  hook has let it through, so reverse execution can keep the registers each
//  instruction began with and take its snapshots on instruction boundaries.
//  An interrupt dispatched in place of an opcode fetch counts as one
//  instruction. Called on the thread that runs the machine.
//
//  The per-instruction part is forced inline and not virtual: the record
//  goes straight into the ring, and only a snapshot falling due, at most a
//  few times a frame, reaches the derived class through OnCaptureDue. Left
//  to the compiler it stayed a call.
//
//  Also told when a disk is mounted, ejected, swapped for a changed file or
//  has the write protection in its file changed, after the change: the disks
//  the machine holds do not follow from its earlier state, so history marks
//  the change as a boundary there. This is rare, so it stays virtual.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryRecorder
{
public:
    virtual             ~HistoryRecorder() = default;

    __forceinline void  OnInstructionStart (const Cpu6502 & cpu, uint64_t position)
    {
        uint64_t    cycle = cpu.Cpu6502::GetCycleCount();      // qualified: no virtual call
        UndoRecord  record;



        if (cycle >= m_nextDueCycle)
        {
            OnCaptureDue (cycle);
        }

        record.cycle = cycle;
        record.pc    = cpu.GetPC();
        record.a     = cpu.GetA();
        record.x     = cpu.GetX();
        record.y     = cpu.GetY();
        record.sp    = cpu.GetSP();
        record.p     = cpu.GetP();

        m_ring.Push (position, record);
    }

    virtual void  OnMediaChanged (MachineHost & machine) = 0;

protected:
    virtual void        OnCaptureDue (uint64_t cycle) = 0;

    UndoRing  m_ring;
    uint64_t  m_nextDueCycle = 0;     // the first cycle at which OnCaptureDue is called
};
