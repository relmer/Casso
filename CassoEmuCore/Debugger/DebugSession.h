#pragma once

#include "Debugger/BreakpointTable.h"
#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugHook.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/IRunObserver.h"
#include "Debugger/LineTable.h"
#include "Debugger/MonitorState.h"
#include "Debugger/SymbolTable.h"
#include "Debugger/WatchTable.h"
#include "Debugger/WatchpointTable.h"

class IDebugCommandHandler;
class IDebugNotificationSink;
class IFileSystem;
class IInstructionObserver;
class OpcodeTable;





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
//  and the context expressions are evaluated in.
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
    Reply  ExecuteLine           (const std::string & line);

    // The same, in a mode the caller chose for this one line. The session's
    // own mode is left as it was, so a channel request naming a mode does not
    // switch the mode every other client is using.
    Reply  ExecuteLine           (const std::string & line, CommandMode mode);
    void   FormatReply           (Reply & reply) const;
    void   FormatReply           (Reply & reply, CommandMode mode) const;

    // Sets the cycle budget later runs get, as `BUDGET` does; empty for none.
    void   SetBudget             (std::optional<uint64_t> budget) { m_budget = budget; }

    // Called by command handlers after they change a table.
    void   OnStopConditionsChanged ();
    void   ClearAllBreakpoints   ();

    // Machine events the host reports.
    void   OnMachineChanged      (const std::string & machineName);
    void   OnReset               (bool isPowerCycle);
    void   OnUserPaused          ();
    void   OnUserResumed         ();

    RunState                GetRunState    () const { return m_state; }
    CommandMode             GetMode        () const { return m_mode; }
    std::optional<uint64_t> GetBudget      () const { return m_budget; }
    LogLevel                GetLogLevel    () const { return m_logLevel; }
    void                    SetLogLevel    (LogLevel level) { m_logLevel = level; }
    IDebugTarget          & GetTarget      ()       { return m_target; }
    BreakpointTable       & GetBreakpoints ()       { return m_breakpoints; }
    WatchpointTable       & GetWatchpoints ()       { return m_watchpoints; }
    WatchTable            & GetWatches     ()       { return m_watches; }
    WatchTable            & GetZeroPage    ()       { return m_zeroPage; }
    WatchTable            & GetBookmarks   ()       { return m_bookmarks; }
    DataBlockTable        & GetDataBlocks  ()       { return m_dataBlocks; }
    SymbolTable           & GetSymbols     ()       { return m_symbols; }
    const SymbolTable     & GetSymbols     () const { return m_symbols; }

    // The program's debug file, loaded by SYM LOAD, and its lines indexed both
    // ways. The path is where it was read from, so its sources are looked for
    // beside it.
    void                  SetDebugFile   (DebugFile file, const std::wstring & path);
    void                  ClearDebugFile ();
    bool                  HasDebugFile   () const { return !m_debugFilePath.empty(); }
    const DebugFile     & GetDebugFile   () const { return m_debugFile; }
    const std::wstring  & GetDebugFilePath () const { return m_debugFilePath; }
    const LineTable     & GetLineTable   () const { return m_lineTable; }

    // Whether T, P and RTS, and every dialect's step commands, step by source
    // line. Only while a debug file is loaded; otherwise by instruction.
    void                  SetStepBySource (bool bySource) { m_stepBySource = bySource; }
    bool                  IsStepBySource  () const        { return m_stepBySource; }

    // The innermost source line at an address, as file name and line; false
    // where no loaded line produced it.
    bool                  TryGetSourceLine (Word address, std::string & file, int & line) const;

    // What the Monitor's line scan carries between lines: where the last
    // examine stopped, where a bare `:` stores, and whether `^E` armed the
    // next one to set registers.
    MonitorState          & GetMonitorState ()      { return m_monitorState; }

    // The shipped tables for the target's machine into Main, Basic, Dos33
    // and ProDos; the constructor and a machine switch do this.
    void   LoadRomSymbols        ();

    // Host files, through the injected file system; a relative path is taken
    // from the current directory. Absent a file system, file commands fail.
    void                  SetFileSystem        (IFileSystem * fileSystem)         { m_fileSystem = fileSystem; }
    IFileSystem         * GetFileSystem        () const                           { return m_fileSystem; }
    const std::wstring  & GetCurrentDirectory  () const                           { return m_currentDirectory; }
    void                  SetCurrentDirectory  (const std::wstring & directory)   { m_currentDirectory = directory; }
    std::wstring          ResolvePath          (const std::string & path) const;

    // The last S or SH results, reachable as @1, @2 and so on.
    const std::vector<Word> & GetSearchResults () const                          { return m_searchResults; }
    void                  SetSearchResults     (std::vector<Word> results)        { m_searchResults = std::move (results); }

    // Line-assembly mode: each following line is assembled at the address,
    // and a blank line ends it.
    void   BeginAssembly         (Word address);
    bool   IsAssembling          () const { return m_assemblyAddress.has_value(); }

    void   SetInstructionObserver (IInstructionObserver * observer) { m_instructionObserver = observer; }

    // BPV: stop when the video scanline enters the range, once.
    void   SetVideoBreak         (uint32_t first, uint32_t last);
    void   ClearVideoBreak       ();
    bool   HasVideoBreak         () const { return m_videoBreak.has_value(); }

    // DebugHook: the stop conditions consulted before each instruction.
    bool   ShouldStopBefore      (Word pc) override;
    bool   HasPendingStop        () const override;
    void   OnInstruction         (Word pc) override;

    // IRunObserver
    void   OnStopped             (const StopEvent & stop) override;

    // IDebugExpressionContext
    bool   TryGetRegister        (const std::string & name, Word & value) const override;
    bool   TryPeek               (Word address, Byte & value) const override;
    bool   TryResolveSymbol      (const std::string & name, Word & address) const override;

private:
    struct VideoBreak
    {
        uint32_t  first = 0;
        uint32_t  last  = 0;
    };

    //  Where a Monitor `G` returns to. The ROM's own G pushes the address of
    //  the code that re-enters the Monitor, so an RTS from the program lands
    //  back at the prompt; Casso pushes the same address and stops there.
    static constexpr Word  kMonitorReentry = 0xFF69;

    bool   TryExecuteEngineCommand (const DebugCommand & command, Reply & reply);
    void   ExecuteRun            (const DebugCommand & command, Reply & reply);
    void   ExecuteSource         (const DebugCommand & command, Reply & reply);
    void   ExecuteAssemblyLine   (const std::string & line, Reply & reply);
    Reply  ExecuteMonitorLine    (const std::string & text);
    Reply  ExecuteAppleWinLine   (const std::string & text);
    void   PushMonitorReturn     ();
    void   UpdateHookInstalled   ();
    bool   HasStopConditions     () const;
    bool   TryMatchBeforeWatchpoint (Word pc);
    bool   IsVideoBreakHit       () const;
    void   ClearTemporary        (const StopEvent & stop);

    static bool         TryGetRunKind  (DebugVerb verb, RunKind & kind);
    static void         SetError       (Reply & reply, CommandStatus status, const std::string & label, const std::string & detail);
    static std::string  Trim           (const std::string & text);

    IDebugTarget                        & m_target;
    IDebugNotificationSink              & m_sink;
    std::vector<IDebugCommandHandler *>   m_handlers;
    IInstructionObserver                * m_instructionObserver = nullptr;
    IFileSystem                         * m_fileSystem          = nullptr;
    std::wstring                          m_currentDirectory;

    int                                   m_nextId        = 0;
    BreakpointTable                       m_breakpoints   { m_nextId };
    WatchpointTable                       m_watchpoints   { m_nextId };
    WatchTable                            m_watches;
    WatchTable                            m_zeroPage;
    WatchTable                            m_bookmarks;
    DataBlockTable                        m_dataBlocks;
    SymbolTable                           m_symbols;
    DebugFile                             m_debugFile;
    std::wstring                          m_debugFilePath;
    LineTable                             m_lineTable;
    bool                                  m_stepBySource  = false;
    std::vector<Word>                     m_searchResults;

    RunState                              m_state         = RunState::Paused;
    CommandMode                           m_mode          = CommandMode::AppleWin;
    LogLevel                              m_logLevel      = LogLevel::Info;
    std::optional<uint64_t>               m_budget;
    bool                                  m_hookInstalled = false;
    std::optional<int>                    m_lastBreakpointId;
    std::optional<WatchHit>               m_beforeHit;
    std::optional<VideoBreak>             m_videoBreak;
    bool                                  m_videoBreakHit = false;

    std::optional<Word>                   m_assemblyAddress;
    std::unique_ptr<OpcodeTable>          m_assemblyOpcodes;

    MonitorState                          m_monitorState;
    std::optional<Word>                   m_monitorReturn;
};
