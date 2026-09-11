#pragma once

#include "Pch.h"


class EmulatorShell;
class JsonValue;
struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineManager
//
//  Switching the emulator from one machine to another, and the reset and
//  power-cycle entry points the user reaches from the menu.
//
//  Constructing a machine is MachineBuilder's job. What is left here is
//  everything a switch does AROUND that: saving the outgoing machine's
//  disks and pending printout, merging the incoming machine's user
//  config, re-pointing the debug panels at the new CPU, re-titling the
//  window, and remembering the choice. None of it is construction, and
//  all of it needs the shell.
//
//  Holds a back-reference to EmulatorShell and is declared a friend of
//  that class so it can reach the parts of the emulator a switch has to
//  touch. No new global state is added; the back-reference is the only
//  coupling.
//
////////////////////////////////////////////////////////////////////////////////

class MachineManager
{
public:
    explicit MachineManager (EmulatorShell & shell);

    void     ShowMachinePicker    ();
    HRESULT  SwitchMachine        (const std::wstring & machineName);

    void     SoftReset            ();
    void     PowerCycle           ();

private:
    static WORD  ResolveMachineSpeedCommand (const JsonValue & mergedJson);

    EmulatorShell &  m_shell;
};
