#pragma once

#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/CycleStopwatch.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/ProfileTable.h"
#include "Debugger/Reverse/ReverseCommand.h"

class IDebugTarget;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers
//
//  =, JSR, NOP and ZAP, KEY, BPV, BPBEAM, FRAME and VIDEOINFO, LBR, TF,
//  PROFILE, STOPWATCH, CYCLES and RCC, BENCHMARK, BENCH and EXITBENCH, and the
//  reverse commands T-, P-, GU-, G- and LIVE. The run commands themselves (G,
//  GG, T, TL, P, RTS) are the session's own.
//
//  The family is also the session's instruction observer: the profile
//  counters (while PROFILE ON), the stopwatch (while armed) and the trace file are fed from each
//  instruction a debugger-driven run executes, and never from a machine
//  running freely. The key queue is fed during a run and after each slice of
//  a free run. The branch record is the CPU's own, kept on every path.
//
//  BENCHMARK, BENCH and EXITBENCH report not available in every session: no
//  benchmark runner exists.
//
////////////////////////////////////////////////////////////////////////////////

class ExecutionHandlers : public IDebugCommandHandler,
                          public IInstructionObserver
{
public:
    bool  TryExecute       (DebugSession & session, const DebugCommand & command, Reply & reply) override;
    void  OnInstruction    (DebugSession & session, Word pc) override;
    void  OnInterrupt      (DebugSession & session) override;
    void  OnFreeRunSlice   (DebugSession & session) override;
    void  OnRunStopped     (DebugSession & session, const StopEvent & stop) override;
    void  OnMachineChanged (DebugSession & session) override;

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
        bool          hasFailed = false;
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
    static void  BreakOnBeam       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  RunFrame          (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowVideoInfo     (DebugSession & session, Reply & reply);
    static void  ShowBranchRecord  (DebugSession & session, Reply & reply);
    void         ToggleTrace       (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Profile           (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Stopwatch         (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ShowStopwatch     (Reply & reply) const;
    void         ShowCycles        (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ResetCycles       (DebugSession & session, Reply & reply);
    static void  Benchmark         (const DebugCommand & command, Reply & reply);
    static void  RunReverse        (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  The reverse command a reverse verb stands for.
    static ReverseCommand  GetReverseCommand (DebugVerb verb);

    void         RecordProfile     (DebugSession & session, Word pc);
    void         BillProfile       (DebugSession & session);
    void         BuildProfile      (DebugSession & session, bool isByAddress, ProfileData & data) const;
    void         SaveProfile       (DebugSession & session, const std::string & name, Reply & reply) const;
    void         RecordTrace       (DebugSession & session, Word pc);
    HRESULT      FlushTrace        (DebugSession & session);
    static HRESULT  ProbeWritable  (IFileSystem & files, const std::wstring & path);
    void         FeedKeys          (DebugSession & session);
    static Byte  Peek              (IDebugTarget & target, Word address);
    static std::string  TrimSpaces (const std::string & text);

    TraceState                         m_trace;
    ProfileTable                       m_profile;
    CycleStopwatch                     m_stopwatch;
    std::optional<PendingInstruction>  m_profilePending;
    std::deque<Byte>                   m_keys;
    uint64_t                           m_cycleMarker    = 0;
    uint64_t                           m_lastRunCycles  = 0;
};
