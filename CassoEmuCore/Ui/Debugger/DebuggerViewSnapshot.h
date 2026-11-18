#pragma once

#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugFile.h"
#include "Debugger/DebugMemoryImage.h"
#include "Debugger/DisassemblyLineList.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Debugger/HeatMapOptions.h"
#include "Debugger/Reply.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Ui/Debugger/MemoryRegionKeys.h"

class HeatMapSymbols;
class Microcode;
struct HeatAccessHover;

class DebugSession;





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
    //  A code line's annotation in its parts; a view composes the parts its
    //  disassembly options turn on: the registers the instruction reads, the
    //  address it accesses and the byte there, the pointer an indirect mode
    //  reads, the flags, and the description of a location with a symbol.
    struct AnnotationParts
    {
        std::string  registers;
        std::string  operandValue;
        std::string  pointer;
        std::string  flags;
        std::string  about;
    };

    struct CodeLine
    {
        Word         address        = 0;
        std::string  bytes;
        std::string  instruction;
        std::string  label;
        bool         isCurrent      = false;
        bool         isBranchedFrom = false;
        bool         hasBreakpoint  = false;
        bool         isEnabled      = true;

        //  Where a branch or jump goes, and what the instruction acts on as
        //  the registers stand: its effective address and the byte there, or
        //  the flag a branch tests (FR-078).
        std::optional<Word>  target;
        std::string          annotation;
        AnnotationParts      annotationParts;

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

        //  Whether the line is in ROM, which an edit of the instruction does
        //  not change.
        bool         isRom         = false;

        //  Why the line is data rather than an instruction, where it is.
        DataOrigin   dataOrigin    = DataOrigin::None;
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

        //  A BASIC line breakpoint on a line the program in memory doesn't
        //  have, so it cannot stop.
        bool            isAbsent = false;
    };

    //  basicName is a BASIC watch's variable, empty for an address;
    //  expression is a typed watch's text. A register's or an expression's
    //  value is not edited in the watch pane.
    struct WatchLine
    {
        int          id              = 0;
        Word         address         = 0;
        std::string  value;
        bool         enabled         = true;
        std::string  basicName;
        std::string  expression;
        bool         isValueEditable = true;
        std::string  error;
        bool         isRegister      = false;
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

        //  The program in memory isn't the debug file's, so no line maps to
        //  an address: there is no line at PC, no breakpoint on a line, and
        //  the line maps are empty.
        bool                          isStale      = false;

        //  Every line at PC, outermost first: the invocation, each macro it
        //  expands, and the body line last. A level counts from the front.
        std::vector<std::pair<int, int>>  places;

        //  Each breakpoint on a line in any file: file, line, breakpoint id.
        std::vector<std::tuple<int, int, int>>  breakpointLines;

        //  Every line that produced code, to the first address it produced.
        //  The same map is shared until another debug file is loaded.
        std::shared_ptr<const std::map<std::pair<int, int>, Word>>  lineAddresses;

        //  Every line that produced code, to the first range of addresses it
        //  produced, first and last; the same map is shared as the first
        //  addresses' map is.
        std::shared_ptr<const std::map<std::pair<int, int>, std::pair<Word, Word>>>  lineRanges;

        //  While heat is shown: the lines whose bytes in memory are not the
        //  line's now, as other code at the same addresses, which take no heat.
        std::shared_ptr<const std::set<std::pair<int, int>>>                          unmatchedLines;

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
    uint64_t                     cycles = 0;     // the CPU's cycles since power-on
    //  Where the branch, jump or call that brought the machine to the PC is,
    //  by a step or a run that stopped there; none where the machine came to
    //  the PC otherwise, or while running.
    std::optional<Word>          branchedFrom;
    //  Where the instruction at the PC branches, jumps or calls to, and
    //  whether the current flags take it, at a stop; none while
    //  running. A view draws the PC's arrow from these whether or not the
    //  PC's line is among its lines.
    std::optional<Word>          pcTarget;
    bool                         isPcTargetTaken = true;
    //  The last instruction that sent PC elsewhere, at any stop, and where it
    //  sent it, when that is known.
    std::optional<Word>          lastBranchFrom;
    std::optional<Word>          lastBranchTo;
    //  Whether the machine was stopped when this was built. The command bar
    //  gates on it: stepping a running machine is not a command it can take.
    bool                         isPaused      = false;
    //  Whether the line assembler takes the command box's lines, which read no
    //  command, so that Return on an empty box ends it rather than acting as a key.
    bool                         isAssembling  = false;
    CommandMode                  mode          = CommandMode::AppleWin;

    //  Why the machine last stopped, and at which breakpoint, if one.
    StopReason                   stopReason    = StopReason::Pause;
    std::optional<int>           stopBreakpoint;

    //  DebugSession::GetRunNotice at the stop, for the status bar.
    std::string                  runNotice;
    std::string                  machine;

    //  The error or result of the last action run from the window, not the
    //  console, and where the window displays it: over a pane, or in a
    //  message box. The console has no line of it. The serial changes with
    //  each action, so an action with no reply clears an older one, and a
    //  second action's same text is displayed again.
    std::vector<std::string>     actionReply;
    uint32_t                     actionReplySerial  = 0;
    std::wstring                 actionReplyPane;
    bool                         isActionReplyBoxed = false;
    bool                         isActionReplyError = false;

    //  The cards' and ports' bus registers, for the operand tips.
    std::vector<DiagnosticsIoRegister>  ioRegisters;

    //  The card in each slot, which labels the slot's ranges, and the
    //  model's directory name, which labels the parts of its system ROM.
    std::array<std::string, 8>          slotCards;
    std::string                         machineId;

    //  The CPU's instruction set, which the code pane assembles an edited
    //  instruction with, and the copy that owns it when the snapshot was
    //  built from a capture.
    const Microcode                                * instructionSet   = nullptr;
    std::shared_ptr<const std::vector<Microcode>>    instructionTable;

    //  The 64K the CPU saw when the snapshot was built from a capture, for a
    //  dialog that reads memory at an address typed into it; empty otherwise.
    std::shared_ptr<const DebugMemoryImage>          memoryImage;

    //  While paused, every line of the line list, with the address, bytes,
    //  label and instruction a code view's find searches; empty while the
    //  machine runs.
    std::shared_ptr<const std::vector<CodeLine>>     codeFind;

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

    //  The instruction to select in each view, set by the heat map's
    //  last-writer and last-reader menu rows, and a count that rises with
    //  each request, so a view selects each one once, in the first snapshot
    //  that contains it.
    std::array<std::optional<Word>, 4>    codeSelect       = {};
    std::array<uint32_t, 4>               codeSelectSerial = {};
    std::vector<RegisterRow>              registers;
    std::string                           flags;
    std::optional<BeamState>              beam;
    std::vector<MemoryLine>               memory;
    std::vector<MemoryWindow>             memoryWindows;
    std::vector<StackLine>                stack;
    CallStackData                         callStack;
    std::vector<BreakpointLine>           breakpoints;
    int                                   nextBreakpointId = 0;   // the id the next breakpoint set takes

    //  The Disk II controller's slot, which a disk breakpoint's Data column
    //  has; absent on a machine with none.
    std::optional<int>                    diskSlot;

    //  Whether the breakpoints pane has a step to undo or redo (FR-120).
    bool                                  canUndoBreakpoints = false;
    std::wstring                          undoBreakpointsText;        // the step Undo would act on, as "delete breakpoint #3"
    std::wstring                          redoBreakpointsText;
    bool                                  canRedoBreakpoints = false;
    std::vector<WatchLine>                watches;

    //  The zero-page pointers, each the address its two bytes form and the
    //  first bytes there, as "$0300: A9 41 8D 00"; "--" for one disabled.
    std::vector<WatchLine>                zeroPagePointers;

    //  The bookmarks, each its id and address.
    std::vector<WatchLine>                bookmarks;

    //  The symbol tables, each its name, count and whether it is on, and
    //  every symbol in them; one object until the symbols change, so a pane
    //  rebuilds its rows only then.
    struct SymbolTableRow
    {
        std::string               name;
        size_t                    count   = 0;
        bool                      enabled = true;
        SymbolTableId             id      = SymbolTableId::Main;
        std::vector<std::string>  origins;   // the files the table was loaded from, by file name
        std::wstring              path;      // the full path of the file last loaded into it, or empty
    };

    struct SymbolsState
    {
        std::vector<SymbolTableRow>  tables;
        std::vector<SymbolInfo>      symbols;
    };

    std::shared_ptr<const SymbolsState>   symbols;

    //  The bytes at each symbol's address, for a symbol that is not a
    //  constant or an I/O switch: as many as its size, from one to
    //  kMaxSymbolBytes. Read for every snapshot, since they change as the
    //  machine runs, and sorted by address.
    static constexpr size_t  kMaxSymbolBytes = 8;

    struct SymbolBytes
    {
        Word                               address = 0;
        size_t                             count   = 0;
        std::array<Byte, kMaxSymbolBytes>  bytes   = {};
    };

    std::vector<SymbolBytes>              symbolBytes;

    //  The profile, read while the machine is paused; none while it runs.
    //  Whether profiling is on, read either way. The data blocks and the
    //  step filter's entries.
    std::optional<ProfileData>            profile;
    std::optional<ProfileData>            profileByAddress;

    //  How much of a profile built from history is done, 0 to 1, while one
    //  builds.
    std::optional<double>                 profileProgress;
    bool                                  isProfiling = false;
    std::vector<DataBlockEntry>           dataBlocks;
    std::vector<StepFilterEntry>          stepFilters;
    std::vector<AutoWatchLine>            autoWatches;
    std::optional<SourceState>            source;
    TraceState                            trace;

    //  The file names of the loaded debug and symbol files whose program
    //  isn't in memory; their source lines and labels are hidden.
    std::vector<std::string>              staleFileNames;

    //  The heat map pane's levels, one per address and kind, 0 for cold;
    //  empty while the pane is hidden and nothing is recorded. Level 255
    //  stands for top: accesses a second while fading, the busiest
    //  address's count while cumulative. While fading, after a move through
    //  history, isRebuilding until the heat at the landing is rebuilt. The
    //  levels are of one bank, of those the machine has; with the mouse over
    //  a cell, its last writer and reader in that bank. Beside the levels:
    //  which addresses were fetched as opcodes, the bank's bytes as they
    //  stand (-1 where one cannot be read without changing the machine), each
    //  address's MemoryRegionKeys key in the CPU's bank (empty in any other),
    //  each opcode's mnemonic with its operand's form ("LDA (..),Y"), and
    //  where in the bank the PC and the stack pointer are, when they are in it.
    using OpcodeForms = std::vector<std::string>;

    struct HeatMapState
    {
        std::vector<Byte>                       execute;
        std::vector<Byte>                       read;
        std::vector<Byte>                       write;
        std::vector<Byte>                       opcodes;
        std::vector<int16_t>                    values;
        std::vector<uint16_t>                   regions;
        std::shared_ptr<const OpcodeForms>      opcodeForms;
        std::optional<Word>                     pc;
        std::optional<Word>                     stack;
        double                                  top          = 0.0;
        bool                                    isRebuilding = false;
        HeatMapOptions::Bank                    bank         = HeatMapOptions::Bank::Cpu;
        std::vector<HeatMapOptions::Bank>       banks;
        bool                                    hasAux       = false;
        std::shared_ptr<const HeatAccessHover>  hover;

        //  A new number each time the map is read, so the window knows a
        //  snapshot holding the last reading again has nothing new in it.
        uint64_t                                serial       = 0;

        //  The reads of RAM nothing had written, but those in the ranges
        //  left out, and the writes that changed their byte, the same way;
        //  and the bytes the debugger edited, a mark each.
        std::vector<Byte>                       unwritten;
        std::vector<Byte>                       changed;
        std::vector<Byte>                       edited;
    };

    HeatMapState                          heatMap;

    //  The CPU's address space's levels for the heat columns, read while any
    //  is shown as the heat map's are; none while none is.
    struct HeatColumnLevels
    {
        std::vector<Byte>  execute;
        std::vector<Byte>  read;
        std::vector<Byte>  write;

        //  Applesoft statements begun at each address, for the BASIC program
        //  pane's lines.
        std::vector<Byte>  statement;

        //  Each address's count of runs, reads, writes and statements since
        //  counting began, for a heat cell's tip; a count past the largest
        //  32-bit value reads as that value.
        std::vector<uint32_t>  executeCount;
        std::vector<uint32_t>  readCount;
        std::vector<uint32_t>  writeCount;
        std::vector<uint32_t>  statementCount;
    };

    std::shared_ptr<const HeatColumnLevels>  heatColumns;

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

    //  The last S or SH's hits and the last MC's differences, each with a
    //  serial that moves when a new one replaces it.
    std::vector<Word>               searchHits;
    uint64_t                        searchSerial  = 0;
    std::vector<CompareDifference>  comparison;
    uint64_t                        compareSerial = 0;

    //  The Applesoft program while a BASIC pane is open: whether Applesoft
    //  is in the $D000 bank, its lines when the chain from TXTTAB ends as it
    //  should, the line being run (none in direct mode or at the ] prompt)
    //  and the byte after TXTPTR, which is the start of a statement about to
    //  run or a byte of the one running. The variables and frames are read
    //  only while the machine is paused, and hasValues is true then.
    struct BasicState
    {
        bool                         isApplesoft = false;
        bool                         hasProgram  = false;
        std::vector<BasicLine>       lines;
        std::optional<Word>          line;
        Word                         textPointer = 0;
        bool                         hasValues   = false;
        std::vector<BasicVariable>   variables;
        std::vector<BasicArray>      arrays;
        std::vector<BasicFrame>      frames;
    };

    std::optional<BasicState>  basic;
};
