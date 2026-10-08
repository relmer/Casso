#pragma once

#include "Debugger/DebugViewCapture.h"
#include "Debugger/IDebugTarget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedDebugTarget
//
//  A debug target that answers from a capture rather than a machine, so the
//  debugger's panes can be built on any thread while the machine runs on.
//  Every read answers as the machine did when the capture was taken. Nothing
//  can be changed through it: a write, a run or a trace switch does nothing,
//  and a poke or patch reports that it did not land.
//
////////////////////////////////////////////////////////////////////////////////

class CapturedDebugTarget : public IDebugTarget
{
public:
    explicit CapturedDebugTarget (std::shared_ptr<const DebugViewCapture> capture);

    const DebugViewCapture & GetCapture  () const { return *m_capture; }

    Cpu6502Registers    GetRegisters      () const override { return m_capture->registers; }
    void                SetRegisters      (const Cpu6502Registers &) override {}

    bool                TryPeek           (Word address, Byte & value) const override;
    bool                TryPoke           (Word, Byte) override { return false; }
    bool                TryPatch          (Word, Byte) override { return false; }
    void                TakeMemoryImage   (DebugMemoryImage & image) const override { image = m_capture->memory; }
    MemoryRegion        GetRegion         (Word address) const override;
    Byte                ReadIo            (Word) override { return 0; }
    void                WriteIo           (Word, Byte) override {}
    void                GetSoftSwitches   (std::vector<SoftSwitch> & switches) const override { switches = m_capture->softSwitches; }

    void                SetRunObserver    (IRunObserver *) override {}
    HRESULT             StartRun          (const RunRequest &) override { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }
    void                RequestPause      () override {}
    void                SetHookInstalled  (bool) override {}
    void                SetStopConditions (DebugHook *) override {}
    void                SetWatchedPages   (const WatchedPages &) override {}
    void                SetWatchSink      (IWatchSink *) override {}

    VideoPosition       GetVideoPosition  () const override { return m_capture->video; }
    uint64_t            GetCycleCount     () const override { return m_capture->cycleCount; }
    Byte                GetLastPenalties  () const override { return m_capture->lastPenalties; }
    bool                TryGetLastBranch  (Word & from) const override;
    DebugCpuKind        GetCpuKind        () const override { return m_capture->cpuKind; }
    const Microcode   * GetInstructionSet () const override { return m_capture->instructionSet; }
    DebugMachineInfo    GetMachineInfo    () const override { return m_capture->machineInfo; }

    void                InjectKey         (Byte) override {}
    bool                IsKeyPending      () const override { return m_capture->isKeyPending; }

    void                SetTraceOn        (bool) override {}
    bool                IsTraceOn         () const override { return m_capture->isTraceOn; }
    void                ClearTrace        () override {}
    size_t              GetTraceSize      () const override { return m_capture->traceSize; }
    void                GetTraceWindow    (size_t first, size_t count, std::vector<TraceRecord> & entries) const override;

private:
    std::shared_ptr<const DebugViewCapture>  m_capture;
};
