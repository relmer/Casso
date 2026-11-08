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
//  The family is also the session's instruction observer: the profile
//  counters (while PROFILE ON), the trace file and the key queue are fed from
//  each instruction a debugger-driven run executes, and never from a machine
//  running freely. The branch record is the CPU's own, kept on every path.
//
//  BENCHMARK, BENCH and EXITBENCH report not available in every session:
//  Casso has no benchmark runner.
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
    static constexpr Byte          kNop             = 0xEA;
    static constexpr Word          kStackPage       = 0x0100;
    static constexpr Word          kNmiVector       = 0xFFFA;
    static constexpr Word          kIrqVector       = 0xFFFE;
    static constexpr Byte          kInterruptPushes = 3;
    static constexpr const char  * kDefaultTrace    = "Trace.txt";
    static constexpr const char  * kDefaultProfile  = "Profile.txt";
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

    // The instruction the hook last let through. Whether it ran, and what it
    // cost, is known only at the next instruction or at the stop: an
    // interrupt can be dispatched in its place.
    struct PendingInstruction
    {
        Word      pc          = 0;
        Byte      opcode      = 0;
        Byte      sp          = 0;
        uint64_t  startCycles = 0;
        bool      isProfiled  = false;
        bool      isTraced    = false;
        size_t    traceStart  = 0;
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

    void         SettlePending     (DebugSession & session);
    void         BillProfile       (DebugSession & session, const PendingInstruction & pending);
    void         BuildProfile      (DebugSession & session, bool isByAddress, ProfileData & data) const;
    void         SaveProfile       (DebugSession & session, const std::string & name, Reply & reply) const;
    void         FlushTrace        (DebugSession & session);
    void         FeedKeys          (DebugSession & session);
    void         SkipPendingBill   ();

    static bool                      WasPreempted    (DebugSession & session, const PendingInstruction & pending);
    static std::string               FormatTraceLine (DebugSession & session, Word pc, bool withVideo);
    static std::vector<std::string>  SplitWords      (const std::string & text);
    static Byte                      Peek            (IDebugTarget & target, Word address);
    static Word                      PeekWord        (IDebugTarget & target, Word address);

    TraceState                         m_trace;
    ProfileTable                       m_profile;
    std::optional<PendingInstruction>  m_pending;
    std::deque<Byte>                   m_keys;
    uint64_t                           m_cycleMarker    = 0;
    uint64_t                           m_lastRunCycles  = 0;
};
