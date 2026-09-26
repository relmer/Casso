#pragma once

#include "Debugger/DebugMemoryView.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/RunStopHook.h"
#include "Debugger/TraceController.h"

class IRunDriver;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDebugTarget
//
//  IDebugTarget over a real machine. Runs execute through the injected
//  IRunDriver, which shares this target's RunStopHook: a synchronous driver
//  for batch mode, the CPU manager's for the emulator.
//
////////////////////////////////////////////////////////////////////////////////

class MachineDebugTarget : public IDebugTarget
{
public:
    explicit MachineDebugTarget (MachineHost & host);

    void                SetRunDriver      (IRunDriver * driver);
    RunStopHook       & GetRunHook        () { return m_runHook; }

    Cpu6502Registers    GetRegisters      () const override;
    void                SetRegisters      (const Cpu6502Registers & registers) override;

    bool                TryPeek           (Word address, Byte & value) const override;
    bool                TryPoke           (Word address, Byte value) override;
    bool                TryPatch          (Word address, Byte value) override;
    MemoryRegion        GetRegion         (Word address) const override;
    Byte                ReadIo            (Word address) override;
    void                WriteIo           (Word address, Byte value) override;
    void                GetSoftSwitches   (std::vector<SoftSwitch> & switches) const override;

    void                SetRunObserver    (IRunObserver * observer) override;
    HRESULT             StartRun          (const RunRequest & request) override;
    void                RequestPause      () override;
    void                SetHookInstalled  (bool installed) override;
    void                SetOpcodeWatch    (const bool * opcodes, IOpcodeWatcher * watcher) override;
    void                SetStopConditions (DebugHook * conditions) override;
    void                SetWatchedPages   (const WatchedPages & pages) override;
    void                SetWatchSink      (IWatchSink * sink) override;

    VideoPosition       GetVideoPosition  () const override;
    uint64_t            GetCycleCount     () const override;
    Byte                GetLastPenalties  () const override;
    bool                TryGetLastBranch  (Word & from) const override;
    DebugCpuKind        GetCpuKind        () const override;
    const Microcode   * GetInstructionSet () const override;
    DebugMachineInfo    GetMachineInfo    () const override;
    void                InjectKey         (Byte key) override;
    bool                IsKeyPending      () const override;
    void                SetTraceOn        (bool on) override;
    bool                IsTraceOn         () const override { return m_trace.IsOn(); }
    void                ClearTrace        () override       { m_trace.Clear(); }
    size_t              GetTraceSize      () const override { return m_trace.GetSize(); }
    void                GetTraceWindow    (size_t first, size_t count, std::vector<TraceRecord> & entries) const override;

    TraceController   & GetTrace          () { return m_trace; }

    std::vector<const IDiagnosticsProvider *>  GetDiagnosticsProviders () const override;

private:
    MachineHost       & m_host;
    DebugMemoryView     m_view;
    RunStopHook         m_runHook;
    TraceController     m_trace;
    IRunDriver        * m_driver        = nullptr;
    IRunObserver      * m_observer      = nullptr;
};
