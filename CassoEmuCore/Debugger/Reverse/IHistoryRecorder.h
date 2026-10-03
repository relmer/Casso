#pragma once

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  IHistoryRecorder
//
//  Told of every instruction the machine is about to execute, after a debug
//  hook has let it through, so reverse execution can keep the registers each
//  instruction began with and take its snapshots on instruction boundaries.
//  An interrupt dispatched in place of an opcode fetch counts as one
//  instruction. Called on the thread that runs the machine.
//
//  Also told when a disk is mounted, ejected, swapped for a changed file or
//  has the write protection in its file changed, after the change: the disks
//  the machine holds do not follow from its earlier state, so history marks
//  the change as a boundary there.
//
////////////////////////////////////////////////////////////////////////////////

class IHistoryRecorder
{
public:
    virtual       ~IHistoryRecorder() = default;

    virtual void  OnInstructionStart (MachineHost & machine) = 0;
    virtual void  OnMediaChanged     (MachineHost & machine) = 0;
};
