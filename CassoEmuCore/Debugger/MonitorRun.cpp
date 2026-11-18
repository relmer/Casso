#include "Pch.h"

#include "Debugger/MonitorRun.h"

#include "Disassembler.h"
#include "Debugger/DebugStopMonitor.h"
#include "Debugger/IDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorRun::Push
//
//  The address an RTS returns to, pushed as the 6502 pushes one: high byte
//  first, and one less than the address itself, because RTS adds one.
//
//  REGISTERS ARE NOT RELOADED FROM $45-$49 the way the ROM's G does. The CPU
//  is the single truth here, so a register set in AppleWin mode survives a
//  Monitor G rather than being overwritten by five bytes of zero page.
//
////////////////////////////////////////////////////////////////////////////////

MonitorRun::Pushed MonitorRun::Push (IDebugTarget & target, DebugStopMonitor & stops)
{
    Pushed            pushed;
    Cpu6502Registers  registers = target.GetRegisters();
    Word              address   = (Word) (kMonitorReentry - 1);



    pushed.before   = registers;
    pushed.isPushed = true;

    target.TryPeek ((Word) (kStackPage + registers.sp), pushed.stackHigh);
    target.TryPeek ((Word) (kStackPage + (Byte) (registers.sp - 1)), pushed.stackLow);

    target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) (address >> 8));
    registers.sp = (Byte) (registers.sp - 1);

    target.TryPoke ((Word) (kStackPage + registers.sp), (Byte) (address & 0xFF));
    registers.sp = (Byte) (registers.sp - 1);

    target.SetRegisters (registers);
    stops.ArmMonitorReturn (kMonitorReentry);

    return pushed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorRun::Undo
//
//  A run that never started has no trace of the Monitor's return address:
//  not on the stack, not in the registers, and not as a stop.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorRun::Undo (IDebugTarget & target, DebugStopMonitor & stops, const Pushed & pushed)
{
    if (!pushed.isPushed)
    {
        return;
    }

    target.TryPoke ((Word) (kStackPage + pushed.before.sp), pushed.stackHigh);
    target.TryPoke ((Word) (kStackPage + (Byte) (pushed.before.sp - 1)), pushed.stackLow);
    target.SetRegisters (pushed.before);
    stops.DisarmMonitorReturn();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorRun::GetStepLine
//
//  The registers follow it, as the stop prints them.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyData MonitorRun::GetStepLine (IDebugTarget & target)
{
    Disassembler     disassembler (target.GetInstructionSet());
    DisassemblyData  data;
    DisassemblyLine  shown;
    Byte             bytes[Disassembler::kMaxInstructionBytes] = {};
    Word             pc                                        = target.GetRegisters().pc;
    HRESULT          hr                                        = S_OK;



    for (size_t i = 0; i < std::size (bytes); ++i)
    {
        target.TryPeek ((Word) (pc + i), bytes[i]);
    }

    hr = disassembler.DisassembleOne (pc, bytes, shown.instruction);
    IGNORE_RETURN_VALUE (hr, S_OK);

    data.lines.push_back (shown);
    return data;
}
