#pragma once

#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/ProfileTable.h"

class IDebugTarget;





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers
//
//  =, JSR, NOP and ZAP, KEY, BPV and VIDEOINFO, LBR, TF, PROFILE, CYCLES and
//  RCC, and BENCHMARK, BENCH and EXITBENCH. The run commands themselves (G,
//  GG, T, TL, P, RTS) are the session's own.
//
//  The family is also the session's instruction observer: the branch record,
//  the profile counters (while PROFILE ON), the trace file and the key queue are fed from each
//  instruction a debugger-driven run executes, and never from a machine
//  running freely.
//
//  BENCHMARK needs a host clock, which batch mode does not have; without a
//  benchmark runner it reports not available.
//
////////////////////////////////////////////////////////////////////////////////

class ExecutionHandlers : public IDebugCommandHandler,
                          public IInstructionObserver
{
public:
    bool  TryExecute    (DebugSession & session, const DebugCommand & command, Reply & reply) override;
    void  OnInstruction (DebugSession & session, Word pc) override;
    void  OnRunStopped  (DebugSession & session, const StopEvent & stop) override;

private:
    static constexpr Byte    kNop                 = 0xEA;
    static constexpr size_t  kMaxInstructionBytes = 3;
    static constexpr Word    kStackPage           = 0x0100;
    static constexpr const char  * kDefaultTrace   = "Trace.txt";
    static constexpr const char  * kDefaultProfile = "Profile.txt";
    static constexpr size_t    kHotAddresses    = 20;
    static constexpr uint64_t  kMaxBilledCycles = 0xFF;

    struct TraceState
    {
        bool          isOn      = false;
        bool          withVideo = false;
        std::wstring  path;
        std::string   name;
        std::string   lines;
    };

    // The instruction the hook last let through. Its cost is known only once
    // it has run, at the next instruction or at the stop.
    struct PendingInstruction
    {
        Word      pc          = 0;
        Byte      opcode      = 0;
        uint64_t  startCycles = 0;
    };

    static void  SetProgramCounter (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  CallSubroutine    (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  WriteNop          (DebugSession & session, Reply & reply);
    void         QueueKeys         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  BreakOnVideoLine  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowVideoInfo     (DebugSession & session, Reply & reply);
    static void  ShowBranchRecord  (DebugSession & session, Reply & reply);
    void         ToggleTrace       (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Profile           (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ShowCycles        (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ResetCycles       (DebugSession & session, Reply & reply);
    static void  Benchmark         (const DebugCommand & command, Reply & reply);

    void         RecordProfile     (DebugSession & session, Word pc);
    void         BillProfile       (DebugSession & session);
    void         BuildProfile      (DebugSession & session, bool isByAddress, ProfileData & data) const;
    void         SaveProfile       (DebugSession & session, const std::string & name, Reply & reply) const;
    void         RecordTrace       (DebugSession & session, Word pc);
    void         FlushTrace        (DebugSession & session);
    void         FeedKeys          (DebugSession & session);
    static Byte  Peek              (IDebugTarget & target, Word address);

    TraceState                         m_trace;
    ProfileTable                       m_profile;
    std::optional<PendingInstruction>  m_profilePending;
    std::deque<Byte>                   m_keys;
    uint64_t                           m_cycleMarker    = 0;
    uint64_t                           m_lastRunCycles  = 0;
};
