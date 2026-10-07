#pragma once

#include "Debugger/DebugFile.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Debugger/HeatAccessJump.h"
#include "Debugger/HeatMapOptions.h"
#include "Debugger/Reply.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Ui/Debugger/BreakpointHistory.h"
#include "Ui/Debugger/BreakpointImport.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/InstructionTouches.h"

class SymbolTable;
class HeatMapSymbols;
class AccessHeatMap;
class IDebugTarget;
class Microcode;

class DebugSession;
class IDiagnosticsProvider;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewSnapshot
//
//  Everything the debugger window shows, as plain data.
//
//  A COPY, NOT A VIEW OF THE SESSION. The session belongs to the CPU thread and
//  the window to the UI thread, so the window never reads the session: the CPU
//  thread builds one of these and hands it across, and the window paints
//  whatever it last received.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerViewSnapshot
{
    struct CodeLine
    {
        Word         address       = 0;
        std::string  bytes;
        std::string  instruction;
        std::string  label;
        bool         isCurrent     = false;
        bool         hasBreakpoint = false;
        bool         isEnabled     = true;

        //  Where a branch or jump goes, and what the instruction acts on as
        //  the registers stand: its effective address and the byte there, or
        //  the flag a branch tests (FR-078).
        std::optional<Word>  target;
        std::string          annotation;

        //  False for a branch on the PC's line that the flags as they stand
        //  will not take; true wherever that is not known.
        bool                 isTargetTaken = true;

        //  What executing this instruction would leave behind, on the PC's
        //  line alone: no other line's register values are known (FR-107).
        std::string          effect;

        //  The operand as written in hex, which Go to resolves, and as shown,
        //  with a symbol for its address; empty for an operand that names no
        //  memory (immediate, implied, the accumulator).
        std::string          memoryOperand;
        std::string          shownOperand;

        //  The outermost source line that produced this address, where a
        //  debug file is loaded and one did.
        int          sourceFileId  = -1;
        int          sourceLine    = 0;
    };

    struct RegisterRow
    {
        std::string  name;
        std::string  value;
    };

    //  Where the video beam is: the scanline, and the cycle within it.
    struct BeamState
    {
        uint32_t  scanline = 0;
        uint32_t  cycle    = 0;
    };

    struct MemoryLine
    {
        Word         address = 0;
        std::string  bytes;
        std::string  characters;
        std::string  region;
    };

    //  One memory window's bytes from `first`, a region for each; an I/O byte
    //  is empty, since reading one would change the machine.
    struct MemoryWindow
    {
        int                               id    = 0;
        Word                              first = 0;
        std::vector<std::optional<Byte>>  bytes;
        std::vector<MemoryRegion>         regions;
    };

    struct StackLine
    {
        Word  address = 0;
        Byte  value   = 0;
    };

    struct BreakpointLine
    {
        int             id      = 0;
        Word            address = 0;
        std::string     text;
        bool            enabled = true;
        BreakpointInfo  info;

        //  The symbol at the breakpoint's address, or empty.
        std::string     label;
    };

    struct WatchLine
    {
        int          id      = 0;
        Word         address = 0;
        std::string  value;
        bool         enabled = true;
    };

    //  One thing the instruction at the PC, or the one just executed, reads or
    //  writes (FR-095): what it is, and what it holds now. `key` is stable
    //  across stops, for the changed highlight.
    struct AutoWatchLine
    {
        std::string  key;
        std::string  label;
        std::string  value;
        bool         isRead  = false;
        bool         isWrite = false;

        //  Only the instruction that ran before this stop touched it.
        bool         isPrevious = false;
    };

    //  Each source line's operand, then its result: file and line to both.
    using LineOperands = std::map<std::pair<int, int>, std::pair<std::string, std::string>>;

    //  The loaded debug file, as the source pane needs it.
    struct SourceState
    {
        std::wstring                  debugFilePath;
        std::string                   programKey;
        std::vector<DebugSourceFile>  files;

        //  The line at PC: the outermost, which the pane shows, and the
        //  innermost, which is the macro body line when depth is above 0.
        int                           fileId       = -1;
        int                           line         = 0;
        int                           bodyFileId   = -1;
        int                           bodyLine     = 0;
        int                           depth        = 0;
        bool                          stepBySource = false;

        //  Every line at PC, outermost first: the invocation, each macro it
        //  expands, and the body line last. A level counts from the front.
        std::vector<std::pair<int, int>>  places;

        //  Each breakpoint on a line in any file: file, line, breakpoint id.
        std::vector<std::tuple<int, int, int>>  breakpointLines;

        //  Every line that produced code, to the first address it produced.
        //  The same map is shared until another debug file is loaded.
        std::shared_ptr<const std::map<std::pair<int, int>, Word>>  lineAddresses;

        //  What each line in the PC's files reads, as the disassembly pane's
        //  operand column has it, and for the line at PC what it leaves too:
        //  the operand first, the result second. Built while the machine is
        //  stopped; the same map is shared while they do not change.
        std::shared_ptr<const LineOperands>                                lineOperands;

        //  Where the instruction at PC branches, jumps or calls to, and
        //  whether the flags as they stand take it.
        std::optional<Word>           pcTarget;
        bool                          isPcTargetTaken = true;
    };

    //  A window of the instruction trace: the entries from first, of the
    //  total it retains. While the machine is stopped, `next` holds the
    //  instructions it will run from the PC, with only their address, bytes,
    //  symbol and instruction filled.
    struct TraceState
    {
        bool                      isOn  = false;
        uint64_t                  total = 0;
        uint64_t                  first = 0;
        std::vector<TraceRecord>  entries;
        std::vector<TraceRecord>  next;
    };

    Word                         pc     = 0;
    //  Whether the machine was stopped when this was built. The command bar
    //  gates on it: stepping a running machine is not a command it can take.
    bool                         isPaused      = false;
    //  Whether the line assembler takes the command box's lines, which read no
    //  command, so that Return on an empty box ends it rather than acting as a key.
    bool                         isAssembling  = false;
    CommandMode                  mode          = CommandMode::AppleWin;
    std::string                  machine;

    //  Where the machine stands in its recorded history; the host fills it in.
    HistoryStatus                history;

    //  Where the symbols came from, one line a source: each built-in table
    //  and each file loaded; see DebuggerViewState::DescribeSymbolSources.
    std::vector<std::string>     symbolSources;
    std::vector<CodeLine>        code;

    //  Every disassembly view's lines, the first repeated in `code`; which
    //  are open, and which follows the PC.
    std::array<std::vector<CodeLine>, 4>  codeViews;
    std::array<bool, 4>                   codeOpen      = {};
    int                                   followView    = 0;
    std::vector<RegisterRow>              registers;
    std::string                           flags;
    std::optional<BeamState>              beam;
    std::vector<MemoryLine>               memory;
    std::vector<MemoryWindow>             memoryWindows;
    std::vector<StackLine>                stack;
    CallStackData                         callStack;
    std::vector<BreakpointLine>           breakpoints;

    //  Whether the breakpoints pane has a step to undo or redo (FR-120).
    bool                                  canUndoBreakpoints = false;
    bool                                  canRedoBreakpoints = false;
    std::vector<WatchLine>                watches;
    std::vector<AutoWatchLine>            autoWatches;
    std::optional<SourceState>            source;
    TraceState                            trace;

    //  The heat map pane's levels, one per address and kind, 0 for cold;
    //  empty while the pane is hidden and nothing is recorded. Level 255
    //  stands for top: accesses a second while fading, the busiest
    //  address's count while cumulative. While fading, after a move through
    //  history, isRebuilding until the heat at the landing is rebuilt. The
    //  levels are of one bank, of those the machine has; with the mouse over
    //  a cell, its last writer and reader in that bank. Beside the levels:
    //  which addresses were fetched as opcodes, the bank's bytes as they
    //  stand (-1 where one cannot be read without changing the machine), each
    //  opcode's mnemonic with its operand's form ("LDA (..),Y"), and where in
    //  the bank the PC and the stack pointer are, when they are in it.
    using OpcodeForms = std::vector<std::string>;

    struct HeatMapState
    {
        std::vector<Byte>                       execute;
        std::vector<Byte>                       read;
        std::vector<Byte>                       write;
        std::vector<Byte>                       opcodes;
        std::vector<int16_t>                    values;
        std::shared_ptr<const OpcodeForms>      opcodeForms;
        std::optional<Word>                     pc;
        std::optional<Word>                     stack;
        double                                  top          = 0.0;
        bool                                    isRebuilding = false;
        HeatMapOptions::Bank                    bank         = HeatMapOptions::Bank::Cpu;
        std::vector<HeatMapOptions::Bank>       banks;
        bool                                    hasAux       = false;
        std::shared_ptr<const HeatAccessHover>  hover;

        //  The reads of RAM nothing had written, but those in the ranges
        //  left out, and the writes that changed their byte, the same way;
        //  and what the CPU's space counted of the reads before written.
        std::vector<Byte>                       unwritten;
        std::vector<Byte>                       changed;
        HeatUnwrittenStatus                     unwrittenStatus;
    };

    HeatMapState                          heatMap;

    //  The symbols the heat map's ranges are read against. The same copy is
    //  shared from snapshot to snapshot until the symbols change.
    std::shared_ptr<const HeatMapSymbols> heatMapSymbols;

    //  The pane CODE, DATA or CONSOLE last asked to bring forward. The
    //  window acts on it once, when the serial changes.
    std::wstring                          showPane;
    uint32_t                              showPaneSerial = 0;

    //  The line the last console line's reply offers instead of it (FR-127),
    //  empty for none. The window offers it once, when the serial changes.
    std::string                           suggestion;
    uint32_t                              suggestionSerial = 0;

    //  Every device of the machine that publishes a panel, whether its panel
    //  is open, and the rows of each open one.
    struct PanelInfo
    {
        std::string  id;
        std::string  title;
        bool         open = false;
    };

    std::vector<PanelInfo>            panels;
    std::vector<DiagnosticsSnapshot>  diagnostics;

    //  The last Go to resolved on the CPU thread: which memory window, where
    //  (nothing when the text did not resolve), what was typed, and a serial
    //  that tells a new one from one already acted on.
    struct GoTo
    {
        int                  window = 0;
        std::optional<Word>  address;
        std::string          text;
        std::string          error;
        uint32_t             serial = 0;
    };

    std::optional<GoTo>  goTo;
};





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
    static constexpr int       kCodeLines       = 20;
    static constexpr int       kMemoryRows      = 16;
    static constexpr uint64_t  kBuildIntervalMs = 16;   // one frame at 60 Hz

    //  The registers pane's last row: the CPU cycle count, read only.
    static constexpr const char  * kCyclesRegister = "Cycles";

    //  Memory windows: the first is always open, and up to three more. Each
    //  reads this many bytes from a sixteen-byte boundary, enough for the rows a
    //  window shows at its widest columns and some to scroll into.
    static constexpr int       kMaxMemoryWindows  = 4;
    static constexpr int       kMemoryWindowBytes = 4096;
    static constexpr int       kMemoryRowBytes    = 16;

    //  Whether the CPU thread should rebuild the snapshot now: every frame while
    //  the machine runs, and at once after an action or a stop or start.
    static bool  IsBuildDue (bool isDirty, bool isPaused, bool wasPaused, uint64_t nowMs, uint64_t builtAtMs);

    //  Up to four disassembly views; the first is always open. Each call
    //  names a view, the first when it does not.
    static constexpr int  kMaxCodeViews = 4;

    //  Where a code view starts. The one following the PC follows it unless
    //  the user moved it; the memory pane starts at the zero page.
    void  SetCodeAddress   (std::optional<Word> address, int view = 0) { CodeView & v = m_code[(size_t) view]; v.address = address; v.centerOn.reset(); v.scrollLines = 0; }

    //  Moves a code view so `address` is on its middle line, or as near the
    //  middle as the top of memory allows. Every navigation goes through
    //  here, so what was asked for is never on an edge.
    void  CenterCodeOn     (Word address, int view = 0) { m_code[(size_t) view].centerOn = address; m_code[(size_t) view].scrollLines = 0; }

    //  Scrolls a code view by instructions, down when positive, up when
    //  negative, through the whole address space.
    void  ScrollCode       (int lines, int view = 0)    { m_code[(size_t) view].scrollLines += lines; }

    //  Show next statement: the view follows the PC again AND brings it to
    //  the middle, where following alone would leave a PC already on screen
    //  wherever it sat.
    void  ShowPcIn         (int view)                   { CodeView & v = m_code[(size_t) view]; v.address.reset(); v.pinnedAtPc.reset(); v.scrollLines = 0; v.centerOnPc = true; }
    void  SetMemoryAddress (Word address)                { m_memoryAddress = address; }

    //  How many lines a code view has room for. The window measures it and
    //  says; the default is what fits the smallest pane worth having.
    void  SetCodeLines     (int lines, int view = 0);
    int   GetCodeLines     (int view = 0) const { return m_code[(size_t) view].lines; }

    std::optional<Word>  GetCodeAddress   (int view = 0) const { return m_code[(size_t) view].address; }

    //  Views 2 to 4 (indexes 1 to 3) open centered on an address, and close;
    //  a view closing while it follows the PC hands following to the first.
    void  OpenCodeView     (int view, Word address);

    //  A view reopened as it was left: `top` on its first line, where
    //  OpenCodeView would put the address in the middle.
    void  OpenCodeViewAt   (int view, Word top);
    void  CloseCodeView    (int view);
    bool  IsCodeViewOpen   (int view) const { return view == 0 || m_code[(size_t) view].open; }

    //  Which open view follows the PC; exactly one does. The one giving it up
    //  stays where it was.
    void  SetFollowView    (int view);
    int   GetFollowView    () const { return m_follow; }
    Word                 GetMemoryAddress () const { return m_memoryAddress; }

    //  Windows 2 to 4 open at an address or close; window 1 is the memory pane
    //  above and moves with SetMemoryAddress. Other ids are ignored.
    void                 OpenMemoryWindow       (int id, Word address);
    void                 CloseMemoryWindow      (int id);
    std::optional<Word>  GetMemoryWindowAddress (int id) const;

    //  The trace pane reads kTraceRows entries from the entry the user
    //  scrolled to, or the newest when it follows the end (no entry).
    static constexpr int     kTraceRows     = 128;
    //  How many instructions from the PC the stopped trace shows below it.
    static constexpr size_t  kTraceNextRows = 8;

    void                     SetTraceTop  (std::optional<uint64_t> first) { m_traceTop = first; }
    std::optional<uint64_t>  GetTraceTop  () const                        { return m_traceTop; }

    //  The first entry of the window read for a pane scrolled to top, or at
    //  the end when top is empty: the window ends at the newest entry at the
    //  latest, so a pane at the end never reads past it.
    static uint64_t     GetTraceWindowFirst (uint64_t total, std::optional<uint64_t> top, int rows);
    static std::string  GetHistoryLine      (uint64_t first, int rows);
    static std::string  GetTraceToggleLine  (bool isOn) { return isOn ? "HISTORY OFF" : "HISTORY ON"; }

    //  Behind live, the trace pane lists the instructions that led to the
    //  current position, at most kHistoryTraceRows of them, in place of the
    //  live trace.
    static constexpr size_t  kHistoryTraceRows = 1000;

    static void         ApplyHistoryTrace   (DebugSession & session, std::vector<TraceRecord> entries, DebuggerViewSnapshot & snapshot);

    //  The heat map pane: while it is shown the machine's accesses are
    //  counted and each snapshot carries their levels; hidden, the machine
    //  records nothing.
    void                 SetHeatMapShown (bool shown) { m_isHeatMapShown = shown; }
    bool                 IsHeatMapShown  () const     { return m_isHeatMapShown; }

    //  Whether the levels come from the fading heat or the totals, and how
    //  long the heat takes to fade.
    void                    SetHeatMapOptions (const HeatMapOptions & options) { m_heatMapOptions = options; }
    const HeatMapOptions &  GetHeatMapOptions () const                         { return m_heatMapOptions; }

    //  The cell the mouse is over on the heat map, whose last writer and
    //  reader each snapshot carries; none when it is over none.
    void                    SetHeatMapHover   (std::optional<Word> address)    { m_heatMapHover = address; }

    //  An opcode's mnemonic with its operand's form, each operand byte a
    //  dot: "LDA (..),Y", "JMP ....", "INX"; empty for an opcode the
    //  instruction set does not define.
    static std::string      GetOpcodeForm     (const Microcode * instructionSet, Byte opcode);

    //  Device panels by provider id. A panel stays open until closed or until
    //  its device leaves the machine; only open panels cost the devices
    //  anything, since a closed one is never asked for its rows.
    void                 OpenPanel   (const std::string & id) { m_openPanels.insert (id); }
    void                 ClosePanel  (const std::string & id) { m_openPanels.erase (id); }
    bool                 IsPanelOpen (const std::string & id) const { return m_openPanels.contains (id); }

    //  Runs the pane commands against the session. CPU thread only.
    //
    //  isPaused is the CPU manager's run state, which the session does not
    //  hold. It decides whether the code pane's annotations are built at all
    //  (FR-110): while the machine runs they would cost an effective-address
    //  prediction and several peeks per shown line per snapshot, to show a
    //  byte read at an arbitrary moment that no one can read at speed.
    DebuggerViewSnapshot  Build (DebugSession & session, bool isPaused = true) const;

    //  The command a control stands for.
    static std::string  GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address);
    static std::string  GetPokeLine             (Word address, Byte value);
    static bool         IsCodeBreakpointAt      (const DebuggerViewSnapshot::BreakpointLine & bp, Word address);

    //  The actions a finished watch edit takes (FR-096), echoed in `mode`.
    //  `watchId` picks a manual watch and `autoIndex` an automatic one;
    //  exactly one is set. Column 0 is the expression, 1 the value. Empty for
    //  text that says nothing writable, or an edit that is not allowed -- an
    //  automatic watch's expression is what the instruction touches, not the
    //  user's. What was typed is evaluated in the session's mode when the
    //  action runs.
    static std::vector<DebuggerAction>  GetWatchEditActions (const DebuggerViewSnapshot & snapshot,
                                                             std::optional<int> watchId, std::optional<int> autoIndex,
                                                             int column, const std::string & typed, CommandMode mode);

    //  What puts a watch edit back (FR-097), from the snapshot as it stood
    //  BEFORE the edit. Moving a manual watch cannot be undone by an action
    //  alone: the watch the edit made gets its id from the engine only once
    //  the edit runs, so the undo holds the watch it replaced and the caller
    //  finds the new one when the time comes.
    struct WatchUndo
    {
        std::vector<DebuggerAction>  actions;

        //  For a moved watch: the id before the edit, and the address to put
        //  back. `movedFromIds` is every watch id there was, so the one the
        //  edit made is the one not among them.
        std::optional<Word>          restoreAddress;
        std::vector<int>             movedFromIds;

        //  The watch the move made, once a snapshot shows it.
        std::optional<int>           movedToId;
    };

    static std::optional<WatchUndo>  GetWatchUndo (const DebuggerViewSnapshot & before,
                                                   std::optional<int> watchId, std::optional<int> autoIndex, int column,
                                                   CommandMode mode);

    //  Notes the watch a move made from a snapshot taken after it ran, and
    //  gives the actions that put the edit back: none yet while no snapshot
    //  shows the moved watch, and empty once it has been removed.
    static void                                        NoteMovedWatch      (const DebuggerViewSnapshot & now, WatchUndo & undo);
    static std::optional<std::vector<DebuggerAction>>  GetWatchUndoActions (const DebuggerViewSnapshot & now, WatchUndo & undo, CommandMode mode);

    //  Which optional views are open, as the text the preferences keep, so a
    //  restart brings them back: `code2=E000 follow=2 memory3=0300 panel=mmu`.
    //  A disassembly view keeps the address at its top, and a memory window
    //  its first byte. The first disassembly view and the first memory window
    //  are always open and are not written.
    struct OpenViews
    {
        std::array<std::optional<Word>, kMaxCodeViews>      code;
        int                                                 follow  = 0;
        std::array<std::optional<Word>, kMaxMemoryWindows>  memory;
        std::vector<std::string>                            panels;
    };

    static std::string  FormatOpenViews (const DebuggerViewSnapshot & snapshot);
    static OpenViews    ParseOpenViews  (const std::string & text);
    static std::string  GetStepLine             ()             { return "T"; }
    static std::string  GetStepOverLine         ()             { return "P"; }
    static std::string  GetStepOutLine          ()             { return "RTS"; }
    static std::string  GetRunLine              ()             { return "G"; }
    static std::string  GetRunToCursorLine      (Word address);

    //  One line for each table holding symbols: a file by its name and the
    //  table it went into, a built-in table by its name and its count.
    static std::vector<std::string>  DescribeSymbolSources (const SymbolTable & symbols);

    //  The mode run to cursor runs in, whatever the console's: GSSquared's g
    //  takes no address, so the window sends Casso's own G in every dialect.
    static constexpr CommandMode  kRunToCursorMode = CommandMode::Casso;

    //  PANEL to open or close a device panel, as an AppleWin line, which
    //  GetModeLine gives in the words of the current mode.
    static std::string  GetPanelLine            (const std::string & id, bool open);

    //  The command line a keyboard-scheme action sends, which is the line its
    //  button sends. The cursor actions use the selected code line (toggling
    //  falls back to the PC's line); Pause has no line, since it is the
    //  channel's pause rather than a command.
    static std::optional<std::string>  GetActionLine (DebuggerKeySchemes::Action   action,
                                                      const DebuggerViewSnapshot * snapshot,
                                                      int                          selectedRow);

    //  A control's AppleWin line in the words of the mode the session is in,
    //  where the mode has them: GSSquared's `s`, `o`, `r`, `g`, `bp`, `nobp`
    //  and `addr:` deposit; WinDbg's `t`, `p`, `gu`, `g`, `bp`, `bc` and `eb`.
    //  Any other line, and any line in another mode, is returned as it is.
    static std::string  GetModeLine   (const std::string & line, CommandMode mode);
    static std::string  GetWinDbgLine (const std::string & name, const std::string & rest, const std::string & line);
    static bool         IsHexBytes    (const std::string & text);

    //  SRC ON or SRC OFF, which the window sends when focus moves between the
    //  source and the disassembly, in the words of the given mode.
    static std::string  GetSourceStepLine (bool isSource, CommandMode mode);

    //  In GSSquared mode with the command line empty, Space and F10 step and
    //  Return resumes, as GSSquared's own window does, whatever the key
    //  scheme; otherwise nothing, and the key goes where it would have.
    static std::optional<DebuggerKeySchemes::Action>  GetConsoleKeyAction (CommandMode mode,
                                                                           WPARAM      vk,
                                                                           bool        ctrl,
                                                                           bool        alt,
                                                                           bool        shift,
                                                                           bool        isLineEmpty);

    //  While the line assembler is taking lines, Return and Space go to the
    //  command box whatever the scheme, so an empty line can end it.
    static bool  DoesAssemblerKeepKey (const DebuggerViewSnapshot * snapshot, WPARAM vk, bool ctrl, bool alt);

    //  In Monitor mode Return on an empty command line is a line of its own:
    //  it shows the next row of bytes, so the command line keeps it from the
    //  key scheme.
    static bool  DoesConsoleKeepKey (CommandMode mode, WPARAM vk, bool ctrl, bool alt);

    //  A line from the window's command box, run and formatted exactly as batch
    //  mode runs and formats it. CPU thread only.
    static Reply  ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The same, except that the AppleWin names batch and the pipe report as
    //  needing the window are carried out on the panes here. CPU thread only.
    Reply  ExecuteWindowLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The lines the window's console shows for a line: the line behind the
    //  prompt of the mode it runs in, then the reply. It runs in `lineMode`
    //  when given, otherwise in the session's mode. CPU thread only.
    std::vector<std::string>  ExecuteConsoleLine (DebugSession & session, const std::string & line, std::optional<CommandMode> lineMode = std::nullopt);

    //  The lines the console shows for a control's action: its echo behind
    //  the prompt, then the reply. The command runs directly, never through a
    //  parser (FR-135). CPU thread only.
    std::vector<std::string>  ExecuteAction (DebugSession & session, const DebuggerAction & action);

    //  A gap ahead of a command's lines, which sets the command and its output
    //  apart from the one before. No lines get none. The gap is a line of its
    //  own, kCommandGap, which the console draws kCommandGapHeight of a line
    //  high.
    static void  AddCommandGap (std::vector<std::string> & lines);
    static bool  IsCommandGap  (const std::string & line) { return line == kCommandGap; }

    static constexpr const char * kCommandGap       = "\f";
    static constexpr float        kCommandGapHeight = 0.75f;

    //  A breakpoints pane action as one undo step, or its undo or redo:
    //  each action's echo behind the prompt, then that action's reply.
    //  An import ends with how many of the file's lines were skipped.
    //  CPU thread only.
    std::vector<std::string>  ExecuteBreakpointStep (DebugSession & session, const BreakpointStep & step);

    //  The R or W a line holds with no file name, which the window asks for,
    //  and the line with the chosen name added.
    static std::optional<DebugVerb>  GetMissingFileVerb  (const std::string & line, CommandMode mode, bool isAssembling);
    static std::string               GetLineWithFileName (const std::string & line, const std::string & path);

    static std::string  GetRegionLabel (MemoryRegion region);

    //  A memory address, from the memory address box or the console's MD: any
    //  expression the console evaluates, alone or inside a 6502 operand form.
    //  `peek` reads the pointer of an indirect form; empty for I/O.
    using GoToPeek = std::function<std::optional<Byte> (Word address)>;
    static bool  TryResolveGoTo (const std::string & text, const IDebugExpressionContext & context, const GoToPeek & peek, Word & address, std::string & error);

    //  Resolves a Go to on the CPU thread, for the window to act on when the
    //  next snapshot carries it.
    void  RequestGoTo (DebugSession & session, int window, const std::string & text);

private:
    void  MoveCodePane   (DebugSession & session, const std::string & name, Reply & reply);

    static DebuggerViewSnapshot::MemoryWindow  ReadMemoryWindow (DebugSession & session, int id, Word address);
    void  MoveMemoryPane (DebugSession & session, const std::string & name, const std::string & argument, Reply & reply);

    struct GoToOperand
    {
        std::string  core;
        char         index         = 0;
        bool         isIndirect    = false;
        bool         isIndexInside = false;
    };

    static bool         TryResolveMemoryAddress (DebugSession & session, const std::string & text, Word & address, std::string & error);
    static GoToOperand  SplitGoToOperand        (const std::string & text);
    static size_t       FindClosingParen        (const std::string & text);
    static bool         IsWrittenAbsolute       (const std::string & core);
    static bool         TryReadGoToPointer      (const GoToOperand & operand, Word value, Word index, bool isZeroPage, const GoToPeek & peek, Word & address, std::string & error);
    void  GoToMemory     (int window, Word address);
    void  ShowWindowPane (DebugSession & session, const std::string & name, Reply & reply);
    void  ImportBreakpoints (DebugSession & session, const std::string & path, const BreakpointImport::LineRunner & run, std::vector<std::string> & lines);

    void  BuildSource    (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildTrace     (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildTraceNext (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildPanels    (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildHeatMap   (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    static HeatAccessInfo  GetHeatAccess (DebugSession & session, HeatSpace space, bool isWrite, Word address);
    static void            ReadHeatValues (IDebugTarget & target, const AccessHeatMap & map, HeatMapOptions::Bank bank, std::vector<int16_t> & values);
    static void            FindHeatMarks  (IDebugTarget & target, const AccessHeatMap & map, DebuggerViewSnapshot::HeatMapState & state);

    //  Every opcode's form for the instruction set, made again only when the
    //  machine brings another.
    std::shared_ptr<const DebuggerViewSnapshot::OpcodeForms>  GetOpcodeForms (const Microcode * instructionSet) const;

    //  The heat map ranges' copy of the symbols, taken again only when they
    //  have changed since the last.
    std::shared_ptr<const HeatMapSymbols>  GetHeatMapSymbols (const SymbolTable & symbols) const;

    Reply  ExecutePanelLine (DebugSession & session, const std::string & text, const std::string & line, CommandMode mode);
    Reply  ExecuteSessionLine (DebugSession & session, const std::string & line, CommandMode mode);
    void   RunPanelCommand  (DebugSession & session, const DebugCommand & command, Reply & reply);
    Reply  ExecuteActionCommand (DebugSession & session, const DebuggerAction & action);

    //  The action's command with its definition parsed and what the user
    //  typed evaluated, or false with why not.
    static bool  TryResolveCommand (DebugSession & session, const DebuggerAction & action, DebugCommand & command, std::string & error);
    static bool  TryParseHexWord   (const std::string & text, Word & value);

    static const IDiagnosticsProvider *  FindProvider (const std::vector<const IDiagnosticsProvider *> & providers, const std::string & name);

    //  The instruction that starts at an address, or none at an I/O address.
    static std::optional<DisassemblyLine>  GetInstructionAt (DebugSession & session, Word address);

    static Word                 GetInstructionLength   (DebugSession & session, Word address);
    static Word                 GetPreviousInstruction (DebugSession & session, Word address);
    static std::optional<Word>  GetReturnAddress       (DebugSession & session);
    static std::optional<Word>  GetOperandAddress      (DebugSession & session, Word address);
    static std::string          GetAnnotation          (DebugSession & session, const DisassemblyLine & line,
                                                       const Cpu6502Registers & registers, const InstructionTouches::Result & touches);
    static std::string          GetEffect              (DebugSession & session, const Cpu6502Registers & registers,
                                                       const InstructionTouches::Result & touches, Word next);

    //  Where the code pane starts this build: the pinned address, the anchor
    //  it already had while the PC is among the lines it produced, or a new
    //  anchor that puts the PC in the middle.
    Word         ChooseCodeStart (DebugSession & session, Word pc, int view) const;
    std::vector<DebuggerViewSnapshot::CodeLine>  BuildCode (DebugSession & session, const DebuggerViewSnapshot & snapshot, int view) const;
    void         BuildAutoWatches (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    static bool  IsSameRegisters  (const Cpu6502Registers & left, const Cpu6502Registers & right);
    static Byte  GetRegisterByte  (const std::string & name, const Cpu6502Registers & registers);
    static void  AddAutoWatches   (DebugSession & session, const InstructionTouches::Result & touches,
                                   const Cpu6502Registers & now, bool isPrevious,
                                   std::vector<DebuggerViewSnapshot::AutoWatchLine> & lines);

    //  An address to disassemble from so that `pc` lands `before` lines in,
    //  or `pc` itself when no such address is found. The 6502 cannot be
    //  disassembled backwards -- an instruction is only where the one before
    //  it ended -- so this walks forward from far enough back and takes the
    //  alignment that reaches `pc` exactly.
    static Word  FindStartAbove  (DebugSession & session, Word pc, int before);

    //  The top line of a pane of `lines` lines moved `count` instructions,
    //  kept within the ends of memory so the pane fills.
    static Word  ScrollCodeTop   (DebugSession & session, Word top, int lines, int count);

    //  One disassembly view. `address` pins it; `centerOn` and `scrollLines`
    //  are moves the next build makes. `followAnchor` and `shown` are where a
    //  view following the PC is anchored and the addresses it last showed: it
    //  re-anchors only when the PC walks out of those, since anchoring on the
    //  PC itself re-disassembled from a new address every snapshot, a window
    //  that never holds still. Mutable because a build is const and these are
    //  what it learned while running.
    struct CodeView
    {
        std::optional<Word>  address;
        std::optional<Word>  pinnedAtPc;
        bool                 centerOnPc   = false;
        std::optional<Word>  centerOn;
        int                  scrollLines  = 0;
        Word                 followAnchor = 0;
        std::vector<Word>    shown;
        int                  lines        = kCodeLines;
        bool                 open         = false;
    };

    mutable std::array<CodeView, kMaxCodeViews>    m_code;
    int                                            m_follow                 = 0;
    Word                                           m_memoryAddress          = 0x0000;
    std::optional<uint64_t>                        m_traceTop;
    bool                                           m_isHeatMapShown         = false;
    HeatMapOptions                                 m_heatMapOptions;
    mutable std::shared_ptr<const HeatMapSymbols>  m_heatMapSymbols;
    mutable uint64_t                               m_heatMapSymbolsRevision = 0;
    std::optional<Word>                            m_heatMapHover;

    //  Every opcode's form, and the instruction set they were made for.
    mutable std::shared_ptr<const DebuggerViewSnapshot::OpcodeForms>    m_opcodeForms;
    mutable const Microcode                                           * m_opcodeFormsSet = nullptr;

    std::optional<DebuggerViewSnapshot::GoTo>  m_goTo;
    std::wstring  m_showPane;
    uint32_t      m_showPaneSerial   = 0;
    std::string   m_suggestion;
    uint32_t      m_suggestionSerial = 0;

    std::array<std::optional<Word>, kMaxMemoryWindows - 1>  m_extraWindows;

    //  The instruction at the last stop and where it would leave the
    //  machine. At the next stop, if the registers are exactly that, it IS
    //  the instruction that just ran -- a step -- and its touches are shown
    //  as the previous instruction's. After a free run they are not, and it
    //  is left out rather than shown for an instruction that did not just run.
    //
    //  A paused machine rebuilds on every click, so the stop is recognized by
    //  its registers: the same registers are the same stop, and what was
    //  worked out on arriving there stands.
    struct LastStop
    {
        Cpu6502Registers                           at       = {};
        InstructionTouches::Result                 here;
        std::optional<InstructionTouches::Result>  previous;
        bool                                       isValid  = false;
    };

    mutable LastStop  m_lastStop;

    BreakpointHistory  m_breakpointHistory;

    //  Mutable so a build can close the panel of a device that has left.
    mutable std::set<std::string>  m_openPanels;

    //  The line-to-address map for the loaded debug file, built once per load.
    mutable std::string                                                        m_lineAddressesKey;
    mutable std::shared_ptr<const std::map<std::pair<int, int>, Word>>         m_lineAddresses;
    mutable std::shared_ptr<const DebuggerViewSnapshot::LineOperands>          m_lineOperands;
};
