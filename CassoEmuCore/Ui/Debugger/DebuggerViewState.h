#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/AutoWatchBuilder.h"
#include "Ui/Debugger/CodeViews.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewCommands.h"
#include "Ui/Debugger/DevicePanelViews.h"
#include "Ui/Debugger/HeatMapReader.h"
#include "Ui/Debugger/MemoryWindowViews.h"

class DebugSession;
class IParallelRunner;
class SymbolTable;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState
//
//  What the window shows and what its controls do, with no window in it.
//
//  EVERY PANE IS A COMMAND'S REPLY. The code pane is `U`, the registers `R`,
//  memory `D`, the stack `STACK`, the call stack `CALLS`, watches `WL`,
//  breakpoints `BPL` -- run through the session and read back from their
//  typed data. The window therefore
//  cannot show anything batch mode or a channel client would not be told, and
//  a fix to a command is a fix to its pane.
//
//  EVERY CONTROL IS A COMMAND LINE. Clicking a line, stepping, poking a byte:
//  each produces the line a person could have typed, which the window then runs
//  exactly as it runs the command line. There is one path from an action to the
//  machine, and it is the one the rest of the debugger already tests.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerViewState
{
public:
    static constexpr int       kCodeLines       = CodeViews::kCodeLines;
    static constexpr int       kMemoryRows      = MemoryWindowViews::kMemoryRows;
    static constexpr uint64_t  kBuildIntervalMs = 16;   // one frame at 60 Hz
    static constexpr uint64_t  kHeatIntervalMs  = HeatMapReader::kHeatIntervalMs;
    static constexpr uint64_t  kScrollHoldMs    = CodeViews::kScrollHoldMs;

    //  The registers pane's last rows, read only: the CPU cycle count, the
    //  cycles since RCC, and the last branch taken.
    static constexpr const char  * kCyclesRegister     = "Cycles";
    static constexpr const char  * kTripRegister       = "Trip";
    static constexpr const char  * kLastBranchRegister = "Last branch";

    static bool  IsReadOnlyRegister (const std::string & name)
    {
        return name == kCyclesRegister || name == kTripRegister || name == kLastBranchRegister;
    }

    static constexpr int  kMaxMemoryWindows  = MemoryWindowViews::kMaxMemoryWindows;
    static constexpr int  kHeatMapGoTo       = 0;   // a Go to's window for the heat map's address box
    static constexpr int  kMemoryWindowBytes = MemoryWindowViews::kMemoryWindowBytes;
    static constexpr int  kMemoryRowBytes    = MemoryWindowViews::kMemoryRowBytes;
    static constexpr int  kMaxCodeViews      = CodeViews::kMaxCodeViews;

    //  Whether the CPU thread should rebuild the snapshot now: every frame while
    //  the machine runs, once the window has taken the last one, and at once
    //  after an action or a stop or start.
    static bool  IsBuildDue (bool isDirty, bool isPaused, bool wasPaused, uint64_t nowMs, uint64_t builtAtMs);
    static bool  IsBuildDue (bool isDirty, bool isPaused, bool wasPaused, bool isTaken, uint64_t nowMs, uint64_t builtAtMs);

    //  Whether a step under way, such as a step over a call, is still too new
    //  for a view: the views see the stop's PC alone, unless the step runs
    //  past kStepHoldMs and is drawn as a run is.
    static bool  IsStepHeld (bool isStepping, uint64_t elapsedMs) { return isStepping && elapsedMs < kStepHoldMs; }

    static constexpr uint64_t  kStepHoldMs     = 500;
    static constexpr uint64_t  kFindTextIdleMs = CodeViews::kFindTextIdleMs;

    //  The parts of the view: the disassembly views, the memory windows, the
    //  device panels, the heat map, and the commands the window runs.
    CodeViews                  & GetCodeViews     ()       { return m_codeViews; }
    const CodeViews            & GetCodeViews     () const { return m_codeViews; }
    MemoryWindowViews          & GetMemoryWindows ()       { return m_memoryWindows; }
    const MemoryWindowViews    & GetMemoryWindows () const { return m_memoryWindows; }
    DevicePanelViews           & GetPanels        ()       { return m_panels; }
    const DevicePanelViews     & GetPanels        () const { return m_panels; }
    HeatMapReader              & GetHeatMap       ()       { return m_heatMap; }
    const HeatMapReader        & GetHeatMap       () const { return m_heatMap; }
    DebuggerViewCommands       & GetCommands      ()       { return m_commands; }
    const DebuggerViewCommands & GetCommands      () const { return m_commands; }

    //  The milliseconds the code views' scroll hold and the heat map's
    //  reading interval are timed by, which a test sets to a clock of its own.
    using Clock = uint64_t (*)();
    void  SetClock (Clock clock) { m_codeViews.SetClock (clock); m_heatMap.SetClock (clock); }

    //  The trace pane reads kTraceRows entries from the entry the user
    //  scrolled to, or the newest when it follows the end (no entry).
    static constexpr int     kTraceRows     = 512;
    //  How many instructions from the PC the stopped trace shows below it.
    static constexpr size_t  kTraceNextRows = 8;

    void                     SetTraceTop  (std::optional<uint64_t> first) { m_traceTop = first; }
    std::optional<uint64_t>  GetTraceTop  () const                        { return m_traceTop; }

    //  Whether the trace pane is on screen; hidden, its rows are not read.
    void                     SetTraceShown (bool shown)                   { m_isTraceShown = shown; }
    bool                     IsTraceShown  () const                       { return m_isTraceShown; }

    //  The first entry of the window read for a pane scrolled to top, or at
    //  the end when top is empty: the window ends at the newest entry at the
    //  latest, so a pane at the end never reads past it.
    static uint64_t  GetTraceWindowFirst (uint64_t total, std::optional<uint64_t> top, int rows);

    //  The BASIC panes: while one is visible each snapshot includes the
    //  Applesoft program; with none visible the program is not read.
    void                 SetBasicShown   (bool isVisible) { m_isBasicShown = isVisible; }
    bool                 IsBasicShown    () const     { return m_isBasicShown; }

    //  The Applesoft program and, while paused, its values, for the BASIC
    //  panes.
    static DebuggerViewSnapshot::BasicState  ReadBasic (DebugSession & session, bool isPaused);

    //  Marks each BASIC line breakpoint on a line the program read doesn't
    //  have (BreakpointLine::isAbsent).
    static void          MarkAbsentBasicLines (DebuggerViewSnapshot & snapshot);

    //  Builds every pane from the session: BuildCaptured, then BuildLive.
    //
    //  isPaused is the CPU manager's run state, which the session does not
    //  have. It decides whether the code pane's annotations are built at all
    //  (FR-110): while the machine runs they would cost an effective-address
    //  prediction and several peeks per shown line per snapshot, to show a
    //  byte read at an arbitrary moment that no one can read at speed.
    DebuggerViewSnapshot  Build         (DebugSession & session, bool isPaused = true) const;

    //  The panes that only read the machine and the session, which can be
    //  built from a session over a capture, on any thread.
    DebuggerViewSnapshot  BuildCaptured (DebugSession & session, bool isPaused, IParallelRunner * runner = nullptr) const;

    //  The panes that drive the live machine: the device panels, which ask
    //  the devices themselves, and the heat map, which is switched on and
    //  folded. The machine's thread only.
    void                  BuildLive     (DebugSession & session, DebuggerViewSnapshot & snapshot, bool isPaused) const;

    //  One line for each table with symbols: a file by its name and the
    //  table it went into, a built-in table by its name and its count.
    static std::vector<std::string>  DescribeSymbolSources (const SymbolTable & symbols);

private:
    void  BuildSource    (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildTrace     (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildTraceNext (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildSymbols   (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildWatches   (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

    static void  AddSessionStops  (DebugSession & session, DebuggerViewSnapshot & snapshot);
    static void  BuildSymbolBytes (DebugSession & session, DebuggerViewSnapshot & snapshot);

    CodeViews             m_codeViews;
    MemoryWindowViews     m_memoryWindows;
    DevicePanelViews      m_panels;
    HeatMapReader         m_heatMap;
    AutoWatchBuilder      m_autoWatches;
    DebuggerViewCommands  m_commands { m_codeViews, m_memoryWindows, m_panels };

    std::optional<uint64_t>  m_traceTop;
    bool                     m_isTraceShown = true;
    bool                     m_isBasicShown = false;

    //  The symbols as last read, and the symbol revision they were read at.
    mutable std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>  m_symbols;
    mutable uint64_t                                                   m_symbolsRevision = 0;

    //  The line-to-address map for the loaded debug file, built once per load.
    mutable std::string                                                                  m_lineAddressesKey;
    mutable std::shared_ptr<const std::map<std::pair<int, int>, Word>>                   m_lineAddresses;

    //  The first range each line produced, built with m_lineAddresses.
    mutable std::shared_ptr<const std::map<std::pair<int, int>, std::pair<Word, Word>>>  m_lineRanges;
    mutable std::shared_ptr<const DebuggerViewSnapshot::LineOperands>                    m_lineOperands;
};
