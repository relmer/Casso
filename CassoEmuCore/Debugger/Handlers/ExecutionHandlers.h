#pragma once

#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/IInstructionObserver.h"

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
//  the profile counters, the trace file and the key queue are fed from each
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
    static constexpr Byte          kNop            = 0xEA;
    static constexpr Byte          kJsr            = 0x20;
    static constexpr Byte          kJmpAbsolute    = 0x4C;
    static constexpr Byte          kJmpIndirect    = 0x6C;
    static constexpr Byte          kRts            = 0x60;
    static constexpr Byte          kRti            = 0x40;
    static constexpr Byte          kBrk            = 0x00;
    static constexpr Byte          kBranchMask     = 0x1F;
    static constexpr Byte          kBranchBits     = 0x10;
    static constexpr Byte          kBraCmos        = 0x80;
    static constexpr Word          kStackPage      = 0x0100;
    static constexpr const char  * kDefaultTrace   = "Trace.txt";
    static constexpr const char  * kDefaultProfile = "Profile.txt";

    struct TraceState
    {
        bool          isOn      = false;
        bool          withVideo = false;
        std::wstring  path;
        std::string   name;
        std::string   lines;
    };

    struct ProfileState
    {
        std::map<std::string, uint64_t>  opcodes;
        std::map<std::string, uint64_t>  modes;
        uint64_t                         instructions = 0;
        uint64_t                         startCycles  = 0;
        bool                             hasStart     = false;
    };

    static void  SetProgramCounter (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  CallSubroutine    (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  WriteNop          (DebugSession & session, Reply & reply);
    void         QueueKeys         (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  BreakOnVideoLine  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowVideoInfo     (DebugSession & session, Reply & reply);
    void         ShowBranchRecord  (Reply & reply) const;
    void         ToggleTrace       (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Profile           (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ShowCycles        (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ResetCycles       (DebugSession & session, Reply & reply);
    static void  Benchmark         (const DebugCommand & command, Reply & reply);

    void         RecordBranch      (DebugSession & session, Word pc);
    void         RecordProfile     (DebugSession & session, Word pc);
    void         RecordTrace       (DebugSession & session, Word pc);
    void         FlushTrace        (DebugSession & session);
    void         FeedKeys          (DebugSession & session);
    static bool  IsControlTransfer (Byte opcode, bool isCmos);
    static Byte  Peek              (IDebugTarget & target, Word address);

    std::optional<Word>  m_lastBranch;
    std::optional<Word>  m_previousPc;
    Byte                 m_previousOpcode = 0;
    TraceState           m_trace;
    ProfileState         m_profile;
    std::deque<Byte>     m_keys;
    uint64_t             m_cycleMarker    = 0;
    uint64_t             m_lastRunCycles  = 0;
};
