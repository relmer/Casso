#pragma once

#include "Debugger/DebugFile.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Debugger/Reply.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/InstructionTouches.h"

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
    };

    struct WatchLine
    {
        int          id      = 0;
        Word         address = 0;
        std::string  value;
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

        //  Each breakpoint on a line in any file: file, line, breakpoint id.
        std::vector<std::tuple<int, int, int>>  breakpointLines;

        //  Every line that produced code, to the first address it produced.
        //  The same map is shared until another debug file is loaded.
        std::shared_ptr<const std::map<std::pair<int, int>, Word>>  lineAddresses;
    };

    //  A window of the instruction trace: the entries from first, of the
    //  total it retains.
    struct TraceState
    {
        bool                      isOn  = false;
        uint64_t                  total = 0;
        uint64_t                  first = 0;
        std::vector<TraceRecord>  entries;
    };

    Word                         pc     = 0;
    //  Whether the machine was stopped when this was built. The command bar
    //  gates on it: stepping a running machine is not a command it can take.
    bool                         isPaused      = false;
    CommandMode                  mode          = CommandMode::AppleWin;
    std::string                  machine;
    std::vector<CodeLine>        code;

    //  Every disassembly view's lines, the first repeated in `code`; which
    //  are open, and which follows the PC.
    std::array<std::vector<CodeLine>, 4>  codeViews;
    std::array<bool, 4>                   codeOpen      = {};
    int                                   followView    = 0;
    std::vector<RegisterRow>              registers;
    std::string                           flags;
    std::vector<MemoryLine>               memory;
    std::vector<MemoryWindow>             memoryWindows;
    std::vector<StackLine>                stack;
    CallStackData                         callStack;
    std::vector<BreakpointLine>           breakpoints;
    std::vector<WatchLine>                watches;
    std::vector<AutoWatchLine>            autoWatches;
    std::optional<SourceState>            source;
    TraceState                            trace;

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

    //  Memory windows: the first is always open, and up to three more. Each
    //  reads this many bytes from a row boundary, enough for the rows a window
    //  shows and some to scroll into.
    static constexpr int       kMaxMemoryWindows  = 4;
    static constexpr int       kMemoryWindowBytes = 512;
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
    static constexpr int     kTraceRows = 128;

    void                     SetTraceTop  (std::optional<uint64_t> first) { m_traceTop = first; }
    std::optional<uint64_t>  GetTraceTop  () const                        { return m_traceTop; }

    //  The first entry of the window read for a pane scrolled to top, or at
    //  the end when top is empty: the window ends at the newest entry at the
    //  latest, so a pane at the end never reads past it.
    static uint64_t     GetTraceWindowFirst (uint64_t total, std::optional<uint64_t> top, int rows);
    static std::string  GetHistoryLine      (uint64_t first, int rows);
    static std::string  GetTraceToggleLine  (bool isOn) { return isOn ? "HISTORY OFF" : "HISTORY ON"; }

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

    //  The commands a finished watch edit sends (FR-096). `watchId` picks a
    //  manual watch and `autoIndex` an automatic one; exactly one is set.
    //  Column 0 is the expression, 1 the value. Empty for text that says
    //  nothing writable, or an edit that is not allowed -- an automatic
    //  watch's expression is what the instruction touches, not the user's.
    static std::vector<std::string>  GetWatchEditLines (const DebuggerViewSnapshot & snapshot,
                                                        std::optional<int> watchId, std::optional<int> autoIndex,
                                                        int column, const std::string & typed);
    static std::string  GetStepLine             ()             { return "T"; }
    static std::string  GetStepOverLine         ()             { return "P"; }
    static std::string  GetStepOutLine          ()             { return "RTS"; }
    static std::string  GetRunLine              ()             { return "G"; }
    static std::string  GetRunToCursorLine      (Word address);

    //  PANEL to open or close a device panel, as an AppleWin line; RunCommand
    //  marks it for the current mode.
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

    //  In GSSquared mode with the command line empty, Space and F10 step and
    //  Return resumes, as GSSquared's own window does, whatever the key
    //  scheme; otherwise nothing, and the key goes where it would have.
    static std::optional<DebuggerKeySchemes::Action>  GetConsoleKeyAction (CommandMode mode,
                                                                           WPARAM      vk,
                                                                           bool        ctrl,
                                                                           bool        alt,
                                                                           bool        shift,
                                                                           bool        isLineEmpty);

    //  A line from the window's command box, run and formatted exactly as batch
    //  mode runs and formats it. CPU thread only.
    static Reply  ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The same, except that the AppleWin names batch and the pipe report as
    //  needing the window are carried out on the panes here. CPU thread only.
    Reply  ExecuteWindowLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The R or W a line holds with no file name, which the window asks for,
    //  and the line with the chosen name added.
    static std::optional<DebugVerb>  GetMissingFileVerb  (const std::string & line, CommandMode mode);
    static std::string               GetLineWithFileName (const std::string & line, const std::string & path);

    static std::string  GetRegionLabel (MemoryRegion region);

    //  A memory pane's Go to (FR-090): a hex address; PC, A, X, Y or S; or a
    //  6502 operand -- zp, abs, zp,X, zp,Y, abs,X, abs,Y, (zp,X), (zp),Y or
    //  (abs) -- resolved against the registers and the bytes `peek` returns.
    //  Two hex digits or fewer is a zero-page operand. Nothing when the text
    //  is none of these, or a pointer would be read from I/O (peek empty).
    using GoToPeek = std::function<std::optional<Byte> (Word address)>;
    static std::optional<Word>  ResolveGoTo (const std::string & text, const Cpu6502Registers & registers, const GoToPeek & peek);

    //  Resolves a Go to on the CPU thread, for the window to act on when the
    //  next snapshot carries it.
    void  RequestGoTo (DebugSession & session, int window, const std::string & text);

private:
    void  MoveCodePane   (DebugSession & session, const std::string & name, Reply & reply);

    static DebuggerViewSnapshot::MemoryWindow  ReadMemoryWindow (DebugSession & session, int id, Word address);
    void  MoveMemoryPane (const std::string & name, const std::string & argument, Reply & reply);

    void  BuildSource    (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildTrace     (DebugSession & session, DebuggerViewSnapshot & snapshot) const;
    void  BuildPanels    (DebugSession & session, DebuggerViewSnapshot & snapshot) const;

    Reply  ExecutePanelLine (DebugSession & session, const std::string & text, const std::string & line, CommandMode mode);
    void   RunPanelCommand  (DebugSession & session, const DebugCommand & command, Reply & reply);

    static const IDiagnosticsProvider *  FindProvider (const std::vector<const IDiagnosticsProvider *> & providers, const std::string & name);
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
    static void  AddAutoWatches   (DebugSession & session, const InstructionTouches::Result & touches,
                                   const Cpu6502Registers & now, bool isPrevious,
                                   std::vector<DebuggerViewSnapshot::AutoWatchLine> & lines);

    //  An address to disassemble from so that `pc` lands `before` lines in,
    //  or `pc` itself when no such address is found. The 6502 cannot be
    //  disassembled backwards -- an instruction is only where the one before
    //  it ended -- so this walks forward from far enough back and takes the
    //  alignment that reaches `pc` exactly.
    static Word  FindStartAbove  (DebugSession & session, Word pc, int before);

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

    mutable std::array<CodeView, kMaxCodeViews>  m_code;
    int                                          m_follow        = 0;
    Word                                         m_memoryAddress = 0x0000;
    std::optional<uint64_t>                      m_traceTop;
    std::optional<DebuggerViewSnapshot::GoTo>  m_goTo;

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

    //  Mutable so a build can close the panel of a device that has left.
    mutable std::set<std::string>  m_openPanels;

    //  The line-to-address map for the loaded debug file, built once per load.
    mutable std::string                                                  m_lineAddressesKey;
    mutable std::shared_ptr<const std::map<std::pair<int, int>, Word>>   m_lineAddresses;
};
