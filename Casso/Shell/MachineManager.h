#pragma once

#include "Pch.h"


class EmulatorShell;
struct MachineConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineManager
//
//  Owner of the machine-construction lifecycle: building every
//  MemoryDevice that the active MachineConfig calls for, wiring the
//  language card and page table, instantiating the video-mode renderers,
//  building the CPU, hot-switching the active machine config, and
//  driving the SoftReset / PowerCycle entry points. Also owns the
//  per-frame video-mode selection that the CPU thread invokes between
//  ExecuteCpuSlices and RenderFramebuffer.
//
//  Holds a back-reference to EmulatorShell and is declared a friend of
//  that class so it can read and write the machine state that the
//  shell still owns directly (memory bus, owned-device vector, MMU,
//  CPU, video modes, soft-switch shadow state, etc.). No new global
//  state is added; the back-reference is the only coupling.
//
////////////////////////////////////////////////////////////////////////////////

class MachineManager
{
public:
    explicit MachineManager (EmulatorShell & shell);

    HRESULT  CreateMemoryDevices  (const MachineConfig & config);
    void     WireLanguageCard     ();
    void     WirePageTable        ();
    void     RebuildBankingPages  ();
    void     CreateVideoModes     ();
    HRESULT  CreateCpu            (const MachineConfig & config);

    Byte *   GetAuxRamBuffer      ();

    void     ShowMachinePicker    ();
    HRESULT  SwitchMachine        (const std::wstring & machineName);

    void     SoftReset            ();
    void     PowerCycle           ();

    void     SelectVideoMode      ();

private:
    EmulatorShell &  m_shell;
};
