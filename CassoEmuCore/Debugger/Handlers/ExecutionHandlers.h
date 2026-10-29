#pragma once

#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/CycleStopwatch.h"
#include "Debugger/Events/ProfileEventProbe.h"
#include "Debugger/IInstructionObserver.h"
#include "Debugger/ProfileTable.h"
#include "Debugger/Reverse/ReverseCommand.h"
#include "Debugger/RoutineProfile.h"

class IDebugTarget;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  ExecutionHandlers
//
//  =, JSR, NOP and ZAP, KEY, BPV, BPBEAM, FRAME and VIDEOINFO, LBR, TF,
//  PROFILE, STOPWATCH, CYCLES and RCC, and the reverse commands T-, P-, GU-,
//  G- and LIVE. The run commands themselves (G, GG, T, TL, P, RTS) are the
//  session's own.
//
//  The family is also the session's instruction observer: the profile
//  counters (while PROFILE ON), the stopwatch (while armed) and the trace file are fed from each
//  instruction a debugger-driven run executes, and never from a machine
//  running freely. The key queue is fed during a run and after each slice of
//  a free run. The branch record is the CPU's own, kept on every path.
//
////////////////////////////////////////////////////////////////////////////////

class ExecutionHandlers : public IDebugCommandHandler,
                          public IInstructionObserver
{
public:
    //  The commands that set the trace file, the profiler and the stopwatch
    //  as they are now, for a debug project to run again.
    std::vector<std::string>  MakeScript () const;

    //  The profile as PROFILE LIST has it, with the list given.
    void  BuildProfile (DebugSession & session, ProfileList list, ProfileData & data) const;

    //  PROFILE HISTORY's build, from a cycle to another or, with none, to the
    //  machine's position, which the shell's history replays; and its
    //  finished counts, which take the place of the profile's, whether
    //  profiling is on or not.
    using HistoryProfiler = std::function<HRESULT (uint64_t fromCycle, std::optional<uint64_t> toCycle, std::string & error)>;

    void  SetHistoryProfiler (HistoryProfiler profiler) { m_historyProfiler = std::move (profiler); }
    void  InstallProfile     (const ProfileCounts & counts);

    //  Whether PROFILE ON is in force.
    bool  IsProfiling () const override { return m_profile.IsOn(); }

    //  The trace lines not yet written to the file; for a test.
    size_t  GetPendingTraceBytes () const { return m_trace.lines.size(); }

    bool  TryExecute       (DebugSession & session, const DebugCommand & command, Reply & reply) override;
    void  OnInstruction    (DebugSession & session, Word pc) override;
    void  OnInterrupt      (DebugSession & session, Word pc) override;
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
        bool          isStarted = false;     // the file was written this trace, so a flush appends
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
    void         BuildProfileFromHistory (DebugSession & session, const std::string & arguments, Reply & reply);
    void         Stopwatch         (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ShowStopwatch     (Reply & reply) const;
    void         ShowCycles        (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         ResetCycles       (DebugSession & session, Reply & reply);
    static void  RunReverse        (DebugSession & session, const DebugCommand & command, Reply & reply);

    //  The reverse command a reverse verb stands for.
    static ReverseCommand  GetReverseCommand (DebugVerb verb);

    void         RecordProfile     (DebugSession & session, Word pc);
    void         BillProfile       (DebugSession & session);
    void         BuildRoutines     (DebugSession & session, ProfileData & data) const;
    void         SaveProfile       (DebugSession & session, const std::string & name, Reply & reply) const;
    void         RecordTrace       (DebugSession & session, Word pc);
    HRESULT      FlushTrace        (DebugSession & session);
    static HRESULT  ProbeWritable  (IFileSystem & files, const std::wstring & path);
    void         FeedKeys          (DebugSession & session);
    static Byte  Peek              (IDebugTarget & target, Word address);
    static std::string  TrimSpaces (const std::string & text);

    //  The list for PROFILE LIST's word: none, ADDR, ROUTINES or TREE.
    static bool  TryGetProfileList (const std::string & word, ProfileList & list);

    TraceState                         m_trace;
    ProfileTable                       m_profile;
    RoutineProfile                     m_routines;
    HistoryProfiler                    m_historyProfiler;
    CycleStopwatch                     m_stopwatch;
    std::optional<PendingInstruction>  m_profilePending;
    std::optional<CycleRange>          m_historyRange;                    // the installed profile's, from history
    uint64_t                           m_historyBuilds  = 0;              // the builds from history installed
    std::deque<Byte>                   m_keys;
    uint64_t                           m_lastRunCycles  = 0;
};
