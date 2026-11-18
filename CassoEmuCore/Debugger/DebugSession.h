#pragma once

#include "Cpu.h"
#include "Debugger/Basic/BasicStopMonitor.h"
#include "Debugger/BreakpointTable.h"
#include "Debugger/CallRecordKeeper.h"
#include "Debugger/CallStack.h"
#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugHook.h"
#include "Debugger/DebugHostFiles.h"
#include "Debugger/DebugLineInterpreter.h"
#include "Debugger/DebugProgramFiles.h"
#include "Debugger/DebugSessionView.h"
#include "Debugger/DebugStopMonitor.h"
#include "Debugger/DiskBreakpointMonitor.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/LineTable.h"
#include "Debugger/MonitorState.h"
#include "Debugger/StepFilter.h"
#include "Debugger/SymbolTable.h"
#include "Debugger/WatchTable.h"
#include "Debugger/WatchpointTable.h"
#include "Debugger/Reverse/ReverseCommand.h"

class HostAccessGuard;
class IDebugCommandHandler;
class IDebugNotificationSink;
class IFileSystem;
class IInstructionObserver;
class IIoEventSource;
class IMachineControl;
class ITapeControl;
struct DebugSessionView;
enum class HostAccess;





////////////////////////////////////////////////////////////////////////////////
//
//  RunState
//
////////////////////////////////////////////////////////////////////////////////

enum class RunState
{
    FreeRunning,
    Paused,
    DebugRun,
    Stepping,
};

// LOG: which notifications batch mode and the channel print. Error is
// stops only; Info adds resumed, reset, machine and mode changes; All adds
// every reply's text.
enum class LogLevel
{
    Error,
    Info,
    All,
};

// SAVESTATE and LOADSTATE: which way a state file goes.
enum class StateFileRequest
{
    Save,
    Load,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugSession
//
//  The debugger's attachment to one machine. It executes commands against
//  the target, owns the breakpoint, watchpoint and watch tables, and tracks
//  whether the machine is running freely, paused, or in a run the debugger
//  started.
//
//  The session is also the target's stop-conditions hook, its run observer,
//  and the context expressions are evaluated in. Its parts are components:
//  DebugLineInterpreter parses and runs command lines, DebugStopMonitor
//  tests the stop conditions and reports a stop's cause, CallRecordKeeper
//  has the call-stack record, DebugProgramFiles the loaded debug and symbol
//  files, and DebugHostFiles the paths to host files.
//
////////////////////////////////////////////////////////////////////////////////

class DebugSession : public DebugHook,
                     public IRunObserver,
                     public IDebugExpressionContext
{
public:
    DebugSession (IDebugTarget & target, IDebugNotificationSink & sink, RunState initialState);
    ~DebugSession() override;

    DebugSession             (const DebugSession &) = delete;
    DebugSession & operator= (const DebugSession &) = delete;

    Reply  Execute               (const DebugCommand & command);
    void   AddHandler            (IDebugCommandHandler * handler);

    // One line in the session's mode, parsed and executed; the reply echoes
    // the line. FormatReply renders a reply's text in the session's mode.
    Reply  ExecuteLine           (const std::string & line) { return m_interpreter.ExecuteLine (line, m_mode); }

    // The same, in a mode the caller chose for this one line. The session's
    // own mode is left as it was, so a channel request naming a mode does not
    // switch the mode every other client is using.
    Reply  ExecuteLine           (const std::string & line, CommandMode mode) { return m_interpreter.ExecuteLine (line, mode); }

    // The same for a pane's read: it runs as a command even while the line
    // assembler waits for the user's next line.
    Reply  ExecutePaneLine       (const std::string & line, CommandMode mode) { return m_interpreter.ExecutePaneLine (line, mode); }

    //  A command that changes registers or memory, refused while the machine runs.
    static bool  IsMachineWrite  (DebugVerb verb);

    //  A step forward, refused while the machine runs.
    static bool  IsStepVerb  (DebugVerb verb);
    void         FormatReply (Reply & reply) const;
    void         FormatReply (Reply & reply, CommandMode mode) const;

    // A reply's text in a given output format, whatever the session's is.
    static void  RenderReply     (Reply & reply, OutputFormat format);

    // Sets the cycle budget later runs get, as `BUDGET` does; empty for none.
    void   SetBudget             (std::optional<uint64_t> budget) { m_budget = budget; }

    // Called by command handlers after they change a table.
    void  OnStopConditionsChanged ();
    void  ClearAllBreakpoints     ();

    // Machine events the host reports.
    void   OnMachineChanged      (const std::string & machineName, bool isPaused = true, bool isKeepingStops = false);
    void   OnReset               (bool isPowerCycle);

    //  Takes each RWTS and driver breakpoint's routine from its vector again,
    //  for a machine whose memory changed other than by the CPU: a move
    //  through history or a keyframe loaded under it.
    void   ResolveDiskCalls      ();

    //  Takes the values the Applesoft variable breakpoints compare against
    //  from memory as it is now, for a machine that moved through history.
    void   TakeBasicValues       () { m_basicMonitor.TakeValues (*this); }
    void   OnUserPaused          ();
    void   OnUserResumed         ();

    // Detach takes the CPU hook off the machine, so it runs untouched, and
    // keeps every table; attaching again puts the hook back for them.
    void   SetAttached           (bool isAttached);
    bool   IsAttached            () const { return m_isAttached; }

    // What each mode prints before a command line: `>`, the Monitor's `*`,
    // and WinDbg's `0:000>`.
    static const char *     GetPrompt      (CommandMode mode);

    RunState                GetRunState    () const { return m_state; }
    CommandMode             GetMode        () const { return m_mode; }

    // The mode of the line being run, which a pipe request sets for itself;
    // the session's mode outside a line.
    CommandMode                GetLineMode     () const { return m_interpreter.GetLineMode().value_or (m_mode); }
    OutputFormat               GetOutputFormat () const { return m_outputFormat; }
    void                       SetOutputFormat (OutputFormat format) { m_outputFormat = format; }
    std::optional<uint64_t>    GetBudget       () const { return m_budget; }
    LogLevel                   GetLogLevel     () const { return m_logLevel; }
    void                       SetLogLevel     (LogLevel level) { m_logLevel = level; }
    IDebugTarget             & GetTarget       ()       { return m_target; }
    BreakpointTable          & GetBreakpoints  ()       { return m_breakpoints; }
    WatchpointTable          & GetWatchpoints  ()       { return m_watchpoints; }
    WatchTable               & GetWatches      ()       { return m_watches; }
    WatchTable               & GetZeroPage     ()       { return m_zeroPage; }
    WatchTable               & GetBookmarks    ()       { return m_bookmarks; }
    DataBlockTable           & GetDataBlocks   ()       { return m_dataBlocks; }

    //  The blocks the debug file's data directives laid down. They are data
    //  while the file matches memory and no data directive covers them; after
    //  X over one, its bytes are disassembled as code until the file loads
    //  again.
    bool                 TryFindData      (Word address, DataBlockEntry & block, DataOrigin & origin) const;
    void                 RemoveData       (Word first, Word last);
    SymbolTable        & GetSymbols       ()       { return m_symbols; }
    const SymbolTable  & GetSymbols       () const { return m_symbols; }

    //  The session's parts: the program's debug files and symbol loads, the
    //  call records, and the host file rules for paths typed to commands.
    DebugProgramFiles        & GetFiles       ()       { return m_files; }
    const DebugProgramFiles  & GetFiles       () const { return m_files; }
    CallRecordKeeper         & GetCallRecords ()       { return m_callRecords; }
    const CallRecordKeeper   & GetCallRecords () const { return m_callRecords; }
    DebugHostFiles           & GetHostFiles   ()       { return m_hostFiles; }
    const DebugHostFiles     & GetHostFiles   () const { return m_hostFiles; }

    // Whether the program in memory is the one each loaded file was made for
    // (ProgramCheck), checked at each stop and when a check is set while
    // the machine is paused. While the debug file's doesn't match, the session
    // has no source lines, and while a file's doesn't match, its table is
    // hidden. A debug file's check is set after it loads; a table's after a
    // symbol file without lines loads into it.
    void                  SetDebugFileCheck (std::vector<ProgramCheck::Entry> entries);
    void                  SetLabelCheck     (SymbolTableId table);

    // The messages logging breakpoints and watchpoints wrote at their hits
    // since the last call, in that order. The debugger window and the batch
    // runner print them.
    std::vector<std::string>                  TakeHitLogs       ();

    // The last count messages, oldest first, of the last kRecentHitLogs
    // written: the ones taken and the ones not taken yet. BPLOGS lists them,
    // for a client that doesn't receive them as they are written.
    static constexpr size_t                   kRecentHitLogs    = 1000;
    std::vector<std::string>                  GetRecentHitLogs  (size_t count) const;

    //  The cycle count RCC last reset the trip count at: CYCLES PART and the
    //  Registers pane's Trip row count from it.
    uint64_t              GetCycleMarker  () const        { return m_cycleMarker; }

    //  The id the next breakpoint or watchpoint takes; the two share ids.
    int                   GetNextId       () const        { return m_nextId; }
    void                  SetNextId       (int nextId)    { m_nextId = nextId; }
    void                  SetCycleMarker  (uint64_t cycle) { m_cycleMarker = cycle; }
    uint64_t              GetTripCycles   (uint64_t total) const { return (total >= m_cycleMarker) ? total - m_cycleMarker : total; }

    // The routines a step into runs as a step over, in every dialect.
    const StepFilter    & GetStepFilter  () const { return m_stepFilter; }

    //  Each symbol or debug file loaded, with the table and the offset, for
    //  a debug project to load again (DebugProgramFiles::SymbolLoad).
    using SymbolLoad = DebugProgramFiles::SymbolLoad;


    //  Each symbol the user added, for a debug project to add again after
    //  its symbol files load (DebugProgramFiles::AddedSymbol).
    using AddedSymbol = DebugProgramFiles::AddedSymbol;


    // CALLS and the call-stack pane: the chain to PC by the chosen mechanism,
    // each frame's target named from the symbol tables.
    CallStackData         GetCallStack     ();

    // What the debugger's panes read, copied for a second session that builds
    // them on another thread, and that copy installed into the second session.
    void                  TakeView         (DebugSessionView & out);
    void                  InstallView      (const DebugSessionView & view);

    // What the Monitor's line scan carries between lines: where the last
    // examine stopped, where a bare `:` stores, and whether `^E` armed the
    // next one to set registers.
    MonitorState          & GetMonitorState ()      { return m_interpreter.GetMonitorState(); }

    // The shipped tables for the target's machine into Main, Basic, Dos33
    // and ProDos; the constructor and a machine switch do this.
    void   LoadRomSymbols        ();

    // The video and sound logs VIDEOLOG and SOUNDLOG list; only the debugger
    // window's shell and a batch run have them, and without them both
    // commands are not available.
    void                  SetIoEventSource     (IIoEventSource * source)          { m_ioEventSource = source; }
    IIoEventSource      * GetIoEventSource     () const                           { return m_ioEventSource; }

    // The cassette deck's keys in the emulator (ITapeControl); null in a batch
    // run, where TAPE calls the deck directly.
    void                  SetTapeControl       (ITapeControl * control)           { m_tapeControl = control; }
    ITapeControl        * GetTapeControl       () const                           { return m_tapeControl; }

    // RESET, PASTE, the disk commands, SCREEN, SCREENSHOT and EXIT in the
    // emulator (IMachineControl); null in a batch run, where they are not
    // available.
    void                  SetMachineControl    (IMachineControl * control)        { m_machineControl = control; }
    IMachineControl     * GetMachineControl    () const                           { return m_machineControl; }

    // The last S or SH results, reachable as @1, @2 and so on.
    const std::vector<Word>  & GetSearchResults () const                          { return m_searchResults; }
    void                       SetSearchResults (std::vector<Word> results)        { m_searchResults = std::move (results); m_searchSerial++; }
    uint64_t                   GetSearchSerial  () const                           { return m_searchSerial; }

    //  The last MC's differences, numbered as the searches are, for a Memory
    //  pane's comparison.
    const std::vector<CompareDifference>  & GetCompareResults () const              { return m_compareResults; }
    void                                    SetCompareResults (std::vector<CompareDifference> differences) { m_compareResults = std::move (differences); m_compareSerial++; }
    uint64_t                                GetCompareSerial  () const                           { return m_compareSerial; }

    // Line-assembly mode: each following line is assembled at the address,
    // and a blank line ends it.
    void   BeginAssembly         (Word address) { m_interpreter.BeginAssembly (address); }
    bool   IsAssembling          () const       { return m_interpreter.IsAssembling(); }

    //  Why the machine last stopped, and at which breakpoint, if one.
    StopReason          GetLastStopReason     () const { return m_lastStopReason; }
    std::optional<int>  GetLastStopBreakpoint () const { return m_lastStopBreakpoint; }

    //  A line about how the last run was made, such as a step out with no
    //  call on record to return from, empty when there is nothing to report.
    //  The next run clears it.
    const std::string &  GetRunNotice         () const { return m_runNotice; }


    void   SetInstructionObserver (IInstructionObserver * observer) { m_instructionObserver = observer; }

    // RUN, LOAD and STARTUP run each script line through the runner when one
    // is set, in the mode the script's command gives, so the debugger window
    // runs them as it runs a typed line. The runner's replies come back
    // formatted.
    using ScriptLineRunner = std::function<Reply (const std::string &, CommandMode)>;

    void                       SetScriptLineRunner (ScriptLineRunner runner)       { m_scriptLineRunner = std::move (runner); }
    const ScriptLineRunner   & GetScriptLineRunner () const                        { return m_scriptLineRunner; }

    // The rest of a script whose line started a run that goes on after the
    // line returns, as G does in the window: the host runs it once the
    // machine stops. TakePendingScript returns false with no script waiting.
    struct PendingScript
    {
        std::string                 content;
        std::optional<CommandMode>  mode;
    };

    void                       SetPendingScript    (PendingScript script)          { m_pendingScript = std::move (script); }
    bool                       TakePendingScript   (PendingScript & outScript);
    bool                       HasPendingScript    () const                        { return m_pendingScript.has_value(); }

    // Reverse execution, where the host records the machine's history: the
    // requester carries a reverse command to the machine and returns whether
    // it was taken. Empty where nothing is recorded.
    using ReverseRequester = std::function<bool (ReverseCommand, uint64_t)>;

    void                       SetReverseRequester (ReverseRequester requester)    { m_reverseRequester = std::move (requester); }
    const ReverseRequester   & GetReverseRequester () const                        { return m_reverseRequester; }

    // A reverse command the requester took has not run yet. The machine is
    // paused all the while, so a script waits on this as it waits on a run.
    void                       SetReversePending   (bool isPending)                { m_isReversePending = isPending; }
    bool                       IsReversePending    () const                        { return m_isReversePending; }

    //  The addresses executed branches, jumps and calls go to, sorted, as
    //  the debugger window last found them. A listing starts a row at each.
    void                       SetBranchTargets    (std::vector<Word> targets)     { m_branchTargets = std::move (targets); }
    bool                       IsBranchTarget      (Word address) const            { return std::ranges::binary_search (m_branchTargets, address); }

    // SAVESTATE and LOADSTATE: the host saves the whole machine to a state
    // file, or replaces it with one, as File > Save state and Load state do,
    // and returns whether it took the request. Empty where there is no host.
    using StateFileRequester = std::function<bool (StateFileRequest, const std::wstring &)>;

    void                       SetStateFileRequester (StateFileRequester requester) { m_stateFileRequester = std::move (requester); }
    const StateFileRequester & GetStateFileRequester () const                       { return m_stateFileRequester; }

    // PROJECT: the host's debug projects, given the words after PROJECT and
    // the reply to fill. Empty where there is no host, and PROJECT is then
    // not available.
    using ProjectRequester = std::function<void (const std::string &, Reply &)>;

    void                       SetProjectRequester   (ProjectRequester requester)   { m_projectRequester = std::move (requester); }

    // Behind live, an edit of registers or memory discards recorded history,
    // so the guard is asked first with the line the edit came from, and the
    // edit is made only when it returns true. Empty where nothing is recorded.
    using HistoryGuard = std::function<bool (const DebugCommand &)>;

    void                       SetHistoryGuard     (HistoryGuard guard)            { m_historyGuard = std::move (guard); }
    // BPV: stop when the video scanline enters the range, once.
    void   SetVideoBreak         (uint32_t first, uint32_t last) { m_stops.GetBeamBreaks().SetVideoBreak (first, last); UpdateHookInstalled(); }
    void   ClearVideoBreak       ()       { m_stops.GetBeamBreaks().ClearVideoBreak(); UpdateHookInstalled(); }
    bool   HasVideoBreak         () const { return m_stops.GetBeamBreaks().HasVideoBreak(); }

    //  The BPV range, first and last scanline, while one is set.
    std::optional<std::pair<uint32_t, uint32_t>>  GetVideoBreak () const { return m_stops.GetBeamBreaks().GetVideoBreak(); }

    // BPBEAM and FRAME: stop when the beam reaches a place in the frame,
    // given as the cycle within it, after passing it `passes - 1` times.
    // A break for one run goes at the run's stop, whatever stopped it.
    void   SetBeamBreak          (uint32_t cycleInFrame, uint32_t passes = 1, bool isForOneRun = false) { m_stops.GetBeamBreaks().SetBeamBreak (cycleInFrame, passes, isForOneRun); UpdateHookInstalled(); }
    void   ClearBeamBreak        (bool isForOneRun = false) { m_stops.GetBeamBreaks().ClearBeamBreak (isForOneRun); UpdateHookInstalled(); }
    bool   HasBeamBreak          () const { return m_stops.GetBeamBreaks().HasBeamBreak(); }

    //  BPBEAM's place in the frame, as the cycle within it; none for no
    //  break, or for FRAME's, which goes with its run.
    std::optional<uint32_t>  GetBeamBreak () const { return m_stops.GetBeamBreaks().GetBeamBreak(); }

    // BRKUNINIT: stop after a read of RAM nothing has written since power-on,
    // reported as a watchpoint hit is, with the address and the instruction.
    void   SetUnwrittenBreak     (bool isOn) { m_stops.SetUnwrittenBreak (isOn); UpdateHookInstalled(); }
    bool   HasUnwrittenBreak     () const    { return m_stops.HasUnwrittenBreak(); }

    // DebugHook: the stop conditions consulted before each instruction.
    // None is pending while the debugger is detached, which a run's stop
    // conditions must not pause the machine for.
    bool   ShouldStopBefore         (Word pc) override { return m_stops.ShouldStopBefore (pc); }
    bool   HasPendingStop           () const override  { return m_isAttached && m_stops.HasPendingStop(); }
    void   OnInstruction            (Word pc) override;
    void   OnInterrupt              (Word pc) override;
    bool   ShouldStopAtRunStart     (Word pc) override { return m_stops.ShouldStopAtRunStart (pc); }
    bool   ShouldStopAfterInterrupt (Word pc) override { return m_stops.ShouldStopAfterInterrupt (pc); }

    // The reads the instruction at pc makes on its own account, which no
    // watchpoint sees: its length in bytes, and an indexed store's target.
    void   GetCpuOwnReads        (Word pc, Word & length, std::optional<Word> & storeTarget) const { m_stops.GetCpuOwnReads (pc, length, storeTarget); }

    // IRunObserver
    void  OnStopped      (const StopEvent & stop) override;
    void  OnFreeRunSlice () override;

    // IDebugExpressionContext
    bool      TryGetRegister     (const std::string & name, Word & value) const override;
    bool      TryPeek            (Word address, Byte & value) const override;
    bool      TryPeekIo          (Word address, Byte & value) const override;
    bool      TryResolveSymbol   (const std::string & name, Word & address) const override;
    uint64_t  GetSymbolRevision  (const std::string & name) const override;

private:
    //  Where a Monitor `G` returns to. The ROM's own G pushes the address of
    //  the code that re-enters the Monitor, so an RTS from the program lands
    //  back at the prompt; Casso pushes the same address and stops there.
    static constexpr Word  kMonitorReentry = 0xFF69;

    bool   TryExecuteEngineCommand (const DebugCommand & command, Reply & reply);
    void   ExecuteRun              (const DebugCommand & command, Reply & reply);
    bool   TryStartNextStep        ();
    void   ExecuteStepFilter       (const DebugCommand & command, Reply & reply);
    void   ExecuteBasicStep        (const DebugCommand & command, Reply & reply);
    void   PushMonitorReturn       ();
    void   UpdateHookInstalled     ();
    void   RefreshHookFilter       ();
    bool   IsProfiling             () const;

    static bool         TryGetRunKind          (DebugVerb verb, RunKind & kind);
    static bool         TryGetFlagMask         (const std::string & name, Byte & mask);
    static void         SetError               (Reply & reply, CommandStatus status, const std::string & label, const std::string & detail);

    IDebugTarget                        & m_target;
    IDebugNotificationSink              & m_sink;
    std::vector<IDebugCommandHandler *>   m_handlers;
    IInstructionObserver                * m_instructionObserver = nullptr;
    IIoEventSource                      * m_ioEventSource       = nullptr;
    ITapeControl                        * m_tapeControl         = nullptr;
    IMachineControl                     * m_machineControl      = nullptr;
    DebugHostFiles                        m_hostFiles;

    int                                   m_nextId        = 0;
    BreakpointTable                       m_breakpoints   { m_nextId };
    WatchpointTable                       m_watchpoints   { m_nextId };
    DiskBreakpointMonitor                 m_diskMonitor   { m_breakpoints, m_target };
    BasicStopMonitor                      m_basicMonitor  { m_breakpoints };
    DebugStopMonitor                      m_stops         { m_target, *this, m_breakpoints, m_watchpoints, m_diskMonitor, m_basicMonitor };
    WatchTable                            m_watches;
    WatchTable                            m_zeroPage;
    WatchTable                            m_bookmarks;
    DataBlockTable                        m_dataBlocks;
    SymbolTable                           m_symbols;
    DebugProgramFiles                     m_files         { m_target, m_symbols };
    std::deque<std::string>               m_recentHitLogs;
    uint64_t                              m_cycleMarker   = 0;
    StepFilter                            m_stepFilter;
    CallRecordKeeper                      m_callRecords   { m_target, m_watchpoints };
    std::vector<Word>                     m_searchResults;
    uint64_t                              m_searchSerial  = 0;
    std::vector<CompareDifference>        m_compareResults;
    uint64_t                              m_compareSerial = 0;

    RunState                              m_state         = RunState::Paused;
    CommandMode                           m_mode          = CommandMode::AppleWin;
    OutputFormat                          m_outputFormat  = OutputFormat::AppleWin;
    LogLevel                              m_logLevel      = LogLevel::Info;
    std::optional<uint64_t>               m_budget;
    bool                                  m_hookInstalled = false;
    bool                                  m_isAttached    = true;
    DebugHookFilter                       m_hookFilter;

    //  Why the machine last stopped.
    StopReason                            m_lastStopReason = StopReason::Pause;
    std::optional<int>                    m_lastStopBreakpoint;

    DebugLineInterpreter          m_interpreter        { *this };
    ScriptLineRunner              m_scriptLineRunner;
    std::optional<PendingScript>  m_pendingScript;
    ReverseRequester              m_reverseRequester;
    bool                          m_isReversePending   = false;
    std::vector<Word>             m_branchTargets;
    StateFileRequester            m_stateFileRequester;
    ProjectRequester              m_projectRequester;
    HistoryGuard                  m_historyGuard;

    //  A counted step over, step out or source step runs as that many single
    //  steps: the one to start after each step completes, how many are left,
    //  and the cycles the steps before it took.
    std::optional<DebugCommand>           m_nextStep;
    uint32_t                              m_stepsLeft     = 0;
    uint64_t                              m_stepCycles    = 0;
    StopEvent                             m_heldStop;
    bool                                  m_isStartingRun = false;
    bool                                  m_isStepPending = false;
    std::string                           m_runNotice;
};
