#pragma once

#include "Debugger/IDebugTarget.h"
#include "Debugger/IRunObserver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockDebugTarget
//
//  An in-memory 64 KB machine for session tests. $C000-$C0FF is unreadable
//  I/O and $D000-$FFFF is read-only ROM. Every hook, mask, run and pause
//  request is recorded, and Stop delivers a stop to the observer as a real
//  target would.
//
////////////////////////////////////////////////////////////////////////////////

class MockDebugTarget : public IDebugTarget
{
public:
    static constexpr Word  kIoFirst  = 0xC000;
    static constexpr Word  kIoLast   = 0xC0FF;
    static constexpr Word  kRomFirst = 0xD000;

    Cpu6502Registers         registers        = { 0x0300, 0, 0, 0, 0xFF, 0x30 };
    std::vector<Byte>        memory           = std::vector<Byte> (0x10000, 0);
    std::vector<SoftSwitch>  softSwitches;
    const Microcode        * instructionSet   = nullptr;
    DebugCpuKind             cpuKind          = DebugCpuKind::M65C02;
    DebugMachineInfo         machineInfo      = { "Apple //e Enhanced", "", {} };
    VideoPosition            videoPosition;
    uint64_t                 cycleCount       = 0;
    Byte                     lastPenalties    = 0;
    bool                     keyPending       = false;

    IRunObserver           * observer         = nullptr;
    DebugHook              * stopConditions   = nullptr;
    bool                     hookInstalled    = false;
    int                      hookChanges      = 0;
    WatchedPages             watchedPages     = {};
    int                      maskChanges      = 0;
    IWatchSink             * watchSink        = nullptr;
    std::vector<RunRequest>  runs;
    int                      pauseRequests    = 0;
    std::vector<Byte>        injectedKeys;
    std::vector<Word>        ioReads;
    std::vector<Word>        ioWrites;
    bool                     traceOn          = false;
    std::vector<TraceRecord> trace;
    int                      traceClears      = 0;

    std::vector<const IDiagnosticsProvider *>  diagnosticsProviders;

    Cpu6502Registers GetRegisters() const override                         { return registers; }
    void             SetRegisters (const Cpu6502Registers & value) override { registers = value; }

    bool TryPeek (Word address, Byte & value) const override
    {
        if (address >= kIoFirst && address <= kIoLast)
        {
            return false;
        }

        value = memory[address];
        return true;
    }

    bool TryPoke (Word address, Byte value) override
    {
        if (address >= kIoFirst)
        {
            return false;
        }

        memory[address] = value;
        return true;
    }

    //  RAM and ROM alike are one array here, so a patch is a store anywhere
    //  outside the I/O page.
    bool TryPatch (Word address, Byte value) override
    {
        if (address >= kIoFirst && address <= kIoLast)
        {
            return false;
        }

        memory[address] = value;
        return true;
    }

    MemoryRegion GetRegion (Word address) const override
    {
        if (address >= kRomFirst)
        {
            return MemoryRegion::Rom;
        }

        return (address >= kIoFirst && address <= kIoLast) ? MemoryRegion::Io : MemoryRegion::MainRam;
    }

    Byte ReadIo  (Word address) override             { ioReads.push_back (address); return 0; }
    void WriteIo (Word address, Byte) override       { ioWrites.push_back (address); }

    void GetSoftSwitches (std::vector<SoftSwitch> & switches) const override { switches = softSwitches; }

    void    SetRunObserver   (IRunObserver * value) override         { observer = value; }
    HRESULT StartRun         (const RunRequest & request) override   { runs.push_back (request); return S_OK; }
    void    RequestPause     () override                             { ++pauseRequests; }
    void    SetHookInstalled (bool installed) override               { hookInstalled = installed; ++hookChanges; }
    void    SetStopConditions (DebugHook * conditions) override      { stopConditions = conditions; }
    void    SetWatchedPages  (const WatchedPages & pages) override   { watchedPages = pages; ++maskChanges; }
    void    SetWatchSink     (IWatchSink * sink) override            { watchSink = sink; }

    VideoPosition     GetVideoPosition  () const override    { return videoPosition; }
    uint64_t          GetCycleCount     () const override    { return cycleCount; }
    Byte              GetLastPenalties  () const override    { return lastPenalties; }
    DebugCpuKind      GetCpuKind        () const override    { return cpuKind; }
    const Microcode * GetInstructionSet () const override    { return instructionSet; }
    DebugMachineInfo  GetMachineInfo    () const override    { return machineInfo; }
    void              InjectKey         (Byte key) override  { injectedKeys.push_back (key); keyPending = true; }
    bool              IsKeyPending      () const override    { return keyPending; }

    void              SetTraceOn        (bool on) override   { traceOn = on; }
    bool              IsTraceOn         () const override    { return traceOn; }
    void              ClearTrace        () override          { traceOn = false; trace.clear(); ++traceClears; }
    size_t            GetTraceSize      () const override    { return trace.size(); }

    void GetTraceWindow (size_t first, size_t count, std::vector<TraceRecord> & entries) const override
    {
        entries.clear();

        for (size_t index = first; index < trace.size() && index - first < count; index++)
        {
            entries.push_back (trace[index]);
            entries.back().index = index;
        }
    }

    std::vector<const IDiagnosticsProvider *>  GetDiagnosticsProviders () const override { return diagnosticsProviders; }

    void Stop (const StopEvent & stop)
    {
        if (observer != nullptr)
        {
            observer->OnStopped (stop);
        }
    }
};
