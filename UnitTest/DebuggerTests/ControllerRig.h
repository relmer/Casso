#pragma once

#include "Debugger/DebuggerController.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "UiTests/InMemoryFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerRig
//
//  The debugger as the window sees it: a controller over a real machine, with
//  every handler, so a command runs as it does when typed into the console.
//  The panes read real memory, and a transport lets a channel client share
//  the session with the window.
//
//  The machine holds LDA #$41 / STA $0400 / RTS at $0300, with the PC on it.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerRig
{
public:
    TestMachine            machine;
    CpuManager             cpuManager;
    InMemoryPipeTransport  transport;
    InMemoryFileSystem     files;
    DebuggerController     controller;
    DebuggerViewState      view;



    ControllerRig() :
        machine    (std::string ("Apple2e"), TestMachine::Slots::Empty),
        controller (machine, Paused (cpuManager), transport, files, nullptr, 1)
    {
        machine.GetMemoryBus().WriteByte (0x0300, 0xA9);
        machine.GetMemoryBus().WriteByte (0x0301, 0x41);
        machine.GetMemoryBus().WriteByte (0x0302, 0x8D);
        machine.GetMemoryBus().WriteByte (0x0303, 0x00);
        machine.GetMemoryBus().WriteByte (0x0304, 0x04);
        machine.GetMemoryBus().WriteByte (0x0305, 0x60);

        Cpu6502Registers  r = controller.GetSession().GetTarget().GetRegisters();

        r.pc = 0x0300;
        controller.GetSession().GetTarget().SetRegisters (r);
    }



    static CpuManager & Paused (CpuManager & cpu)
    {
        cpu.SetPaused (true);
        return cpu;
    }



    //  A line typed into the console in the given dialect.
    Reply Run (const std::string & line, CommandMode mode = CommandMode::AppleWin)
    {
        return DebuggerViewState::ExecuteLine (controller.GetSession(), line, mode);
    }
};