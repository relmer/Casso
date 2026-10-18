#pragma once

#include "I6502DebugInfo.h"
#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/Reverse/ReverseOutcome.h"
#include "Debugger/RoutineProfile.h"
#include "Disassembler.h"

class LineTable;
class StepFilter;





////////////////////////////////////////////////////////////////////////////////
//
//  Reply data kinds
//
//  One struct per data kind in the debug channel protocol. Text and JSON are
//  both rendered from these, never from each other.
//
////////////////////////////////////////////////////////////////////////////////

enum class CommandStatus
{
    Ok,
    Error,
    NotAvailable,
    Unknown,
};

enum class MemoryRegion
{
    MainRam,
    AuxRam,
    LcBank1,
    LcBank2,
    Rom,
    SlotRom,
    Io,
};

enum class BreakpointKind
{
    Address,
    Opcode,
    Register,
    Memory,
    Brk,
    Interrupt,
    MemoryValue,
    Disk,
    BasicLine,
    BasicVariable,
    BasicError,
};

enum class WatchAccess
{
    Read,
    Write,
    ReadWrite,
};

enum class WatchListKind
{
    Watch,
    ZeroPage,
    Bookmark,
};

// When a watchpoint stops: after the access has happened, reported by the
// bus, or before the instruction whose operand would make it.
enum class WatchMode
{
    After,
    Before,
};

enum class SymbolTableId
{
    Main,
    Basic,
    Asm,
    User,
    User2,
    Src,
    Src2,
    Dos33,
    ProDos,
};

// Lines with no other structure: ECHO, PRINT, a script's collected output.
struct MessageData
{
    std::vector<std::string>  lines;
};

struct RegistersData
{
    Cpu6502Registers  registers = {};

    //  One register to show, as WinDbg's `r a` asks for: A, X, Y, S, PC or P.
    //  Empty for all of them.
    std::string       only;
};

struct MemoryRow
{
    Word                              address = 0;
    std::vector<std::optional<Byte>>  bytes;
    MemoryRegion                      region  = MemoryRegion::MainRam;
};

struct MemoryData
{
    std::vector<MemoryRow>  rows;
};

//  `label` is the name at the line's own address (or the name of the data
//  block it starts); `operandSymbol` is the name of the address its operand
//  names. They are kept apart because a listing shows them in different
//  places: the label in its own column, the operand symbol in the operand.
struct DisassemblyLine
{
    DisassembledInstruction  instruction;
    std::string              label;
    std::string              operandSymbol;

    //  The operand as a listing shows it, with the symbol in place of the
    //  address it names when one is loaded.
    std::string  GetShownOperand() const
    {
        return instruction.hasOperandAddress
            ? Disassembler::SubstituteSymbol (instruction.operand, instruction.operandAddress, operandSymbol)
            : instruction.operand;
    }
};

struct DisassemblyData
{
    std::vector<DisassemblyLine>  lines;
};

struct BreakpointInfo
{
    int                  id        = 0;
    BreakpointKind       kind      = BreakpointKind::Address;
    Word                 address   = 0;
    Word                 last      = 0;
    Byte                 opcode    = 0;
    std::optional<Byte>  value;                     // kind MemoryValue
    std::string          condition;                 // a register predicate, or an IF expression
    WatchAccess          access    = WatchAccess::ReadWrite;
    WatchMode            mode      = WatchMode::After;
    bool                 enabled   = true;
    bool                 temporary = false;         // cleared when it fires
    bool                 stops     = true;          // false: counts hits only
    uint32_t             hits      = 0;
    std::string          symbol;                    // the symbol an address breakpoint was set by
    std::string          name;                      // BPNAME's; empty for the default

    // Kind Disk: what BPDISK was given, and the routine an RWTS or driver
    // breakpoint stops at now, absent while its vector has no target.
    std::optional<DiskBreakSpec>  disk;
    std::optional<Word>           resolved;

    // Kind BasicVariable: the variable or array element, as BPBASICVAR was
    // given it. Kind BasicLine has its line number in address.
    std::string                   basicName;
};

struct BreakpointSetData
{
    BreakpointInfo  breakpoint;
};

struct BreakpointListData
{
    std::vector<BreakpointInfo>                    breakpoints;

    //  A BPV scanline break, first and last line. The session has it, not a
    //  table, so it has no id.
    std::optional<std::pair<uint32_t, uint32_t>>  videoBreak;
};

//  A BASIC watch has its variable's name, and its value as PRINT writes it,
//  a string in quotes, while the machine is stopped in a running program.
struct WatchEntry
{
    int                         id      = 0;
    Word                        address = 0;
    bool                        enabled = true;
    std::optional<Word>         value;
    std::string                 basicName;
    std::optional<std::string>  basicValue;
};

struct WatchListData
{
    WatchListKind            kind = WatchListKind::Watch;
    std::vector<WatchEntry>  entries;
};

struct SearchHitsData
{
    std::vector<Word>  addresses;
};

struct StackEntry
{
    Word  address = 0;
    Byte  value   = 0;
};

struct StackData
{
    Byte                     sp = 0;
    std::vector<StackEntry>  entries;
};

struct SoftSwitch
{
    std::string  name;
    bool         value = false;
};

struct SoftSwitchData
{
    std::vector<SoftSwitch>  switches;
};

struct SymbolInfo
{
    std::string    name;
    Word           address    = 0;
    SymbolTableId  table      = SymbolTableId::Main;
    bool           isConstant = false;   // an equate: a value, not an address
    Word           size       = 0;       // the bytes it spans, where its file said
};

struct SymbolData
{
    std::vector<SymbolInfo>  symbols;
};

struct CyclesData
{
    uint64_t  count = 0;
};

struct ModeData
{
    CommandMode  mode = CommandMode::AppleWin;
};

// BLOAD, BSAVE, R and W. `echo` is DebugSession::GetPathEcho's text: the
// absolute path a relative one resolved to, or empty. `companion` is the
// line from the debug or symbol file BLOAD found beside the binary.
struct FileIoData
{
    std::string  path;
    std::string  echo;
    std::string  companion;
    uint32_t     requested   = 0;
    uint32_t     transferred = 0;
    Word         address     = 0;
    bool         isLoad      = false;
    bool         mismatch    = false;
};

// MC: each byte that differs between the source range and the destination.
struct CompareDifference
{
    Word  address    = 0;
    Byte  value      = 0;
    Word  other      = 0;
    Byte  otherValue = 0;
};

struct CompareData
{
    uint32_t                        compared = 0;
    std::vector<CompareDifference>  differences;
};

enum class DataBlockKind
{
    Bytes,
    Words,
    Address,
    Text,
    Float,
};

// A range the disassembler shows as data rather than code.
struct DataBlock
{
    std::string    name;
    Word           first = 0;
    Word           last  = 0;
    DataBlockKind  kind  = DataBlockKind::Bytes;
};

struct DataBlockListData
{
    std::vector<DataBlock>  blocks;
};

struct VideoInfoData
{
    uint32_t  scanline    = 0;
    uint32_t  cycleInLine = 0;
};

// LBR: the last instruction that transferred control, if one has.
struct BranchRecordData
{
    std::optional<Word>  address;
};

// One mnemonic in one addressing mode. The cycles are the base cycles only;
// the avoidable penalties are ProfileData's own rows.
struct ProfileEntry
{
    std::string  mnemonic;
    std::string  mode;
    uint64_t     count  = 0;
    uint64_t     cycles = 0;
};

struct ProfileAddressEntry
{
    Word         address = 0;
    std::string  symbol;
    uint64_t     cycles  = 0;
};

// One routine: where it starts and how it was entered, the cycles of its own
// instructions, the cycles of it and every routine it called, counted once
// per call chain, and how many times it was called.
struct ProfileRoutineEntry
{
    Word         address = 0;
    RoutineKind  kind    = RoutineKind::Top;
    std::string  symbol;
    uint64_t     self    = 0;
    uint64_t     total   = 0;
    uint64_t     calls   = 0;

    //  Top level, the symbol or the address, with BRK or Interrupt ahead of
    //  a routine entered that way.
    std::string  GetName () const;
};

// One node of the call tree: its routine, an index in ProfileData::routines,
// and its depth under Top level. The nodes are in depth-first order, each
// node's callees after it by their totals.
struct ProfileTreeEntry
{
    size_t       routine = 0;
    int          depth   = 0;
    uint64_t     self    = 0;
    uint64_t     total   = 0;
    uint64_t     calls   = 0;
};

// One video frame's cycles by routine, an index in ProfileData::routines, and
// the cycles of the routines it had no room for.
struct ProfileFrameEntry
{
    uint64_t                                  index = 0;
    std::vector<std::pair<size_t, uint64_t>>  shares;
    uint64_t                                  other = 0;
};

// The table PROFILE LIST prints: by opcode, by address (ADDR), by routine
// (ROUTINES) or as the call tree (TREE).
enum class ProfileList
{
    Opcodes,
    Addresses,
    Routines,
    Tree,
};

// PROFILE LIST and its tables. The cycles total includes the penalties. The
// routines, the call tree and the frames are there whatever the list.
struct ProfileData
{
    bool                              isOn         = false;
    ProfileList                       list         = ProfileList::Opcodes;
    uint64_t                          instructions = 0;
    uint64_t                          cycles       = 0;
    uint64_t                          pageCross    = 0;
    uint64_t                          branchTaken  = 0;
    uint64_t                          branchCross  = 0;
    std::vector<ProfileEntry>         opcodes;
    std::vector<ProfileAddressEntry>  addresses;
    std::vector<ProfileRoutineEntry>  routines;
    std::vector<ProfileTreeEntry>     tree;
    std::vector<ProfileFrameEntry>    frames;
};

struct CalcData
{
    Word  value = 0;
};

// SKIP: a routine a step into runs as a step over. name is the symbol it was
// given by, empty when it was given as an address or a range.
struct StepFilterEntry
{
    std::string  name;
    Word         first = 0;
    Word         last  = 0;
};

struct StepFilterData
{
    std::vector<StepFilterEntry>  entries;
};

// CALLS: the chain of calls to PC (FR-067 to FR-069).
enum class CallStackMechanism
{
    Recorded,
    Walk,
    Hybrid,
};

enum class CallFrameKind
{
    Call,
    Brk,
    Irq,
    Nmi,
};

enum class CallProvenance
{
    Recorded,
    Guessed,
};

enum class CallBreakKind
{
    Txs,
    PulledReturn,
    EndedByJump,
    ReturnMismatch,
    StackWrap,
    Reset,
    TrackingBegan,
    PowerOn,
    HistoryBegan,
    TrackingRestarted,
};

// callSite is the JSR or BRK, or the interrupted instruction; target is the
// routine entered or the handler the vector took. stackLevel is SP before
// the push, which is what ends the frame (R-033). A frame below a break is
// unverified. isRewritten is set once a store changes a byte of its return
// address, so where it returns to is the program's choice, not a mismatch.
// hasRisen is set once the stack pointer goes above where the call left it,
// as pulling the return address does; step back out passes over such a
// frame. note is empty unless something about the frame is worth saying.
struct CallStackFrame
{
    Word            callSite    = 0;
    Word            target      = 0;
    CallFrameKind   kind        = CallFrameKind::Call;
    CallProvenance  provenance  = CallProvenance::Recorded;
    Byte            stackLevel  = 0;
    uint64_t        cycle       = 0;        // the cycle count the call began at; recorded frames only
    bool            isVerified  = true;
    bool            isRewritten = false;
    bool            hasRisen    = false;    // the stack pointer has since been above where the call left it; recorded frames only
    std::string     symbol;
    std::string     note;
};

// The instruction that broke the chain, and its opcode.
struct CallStackBreak
{
    CallBreakKind  kind   = CallBreakKind::Txs;
    Word           pc     = 0;
    Byte           opcode = 0;
};

// One row of the chain: a frame, or a break between frames.
struct CallStackRow
{
    std::optional<CallStackFrame>  frame;
    std::optional<CallStackBreak>  chainBreak;
};

// Innermost first. lastReturn is the most recent frame the recorder saw
// return with a note, such as one that returned past inline parameters.
// rebuildProgress is set while the record is being rebuilt from history,
// with how much of it has been replayed, 0 to 1.
struct CallStackData
{
    CallStackMechanism             mechanism = CallStackMechanism::Hybrid;
    std::vector<CallStackRow>      rows;
    std::optional<CallStackFrame>  lastReturn;
    std::optional<float>           rebuildProgress;
};

// CALLS MODE, with or without a mechanism: the one now in use.
struct CallStackModeData
{
    CallStackMechanism  mechanism = CallStackMechanism::Hybrid;
};

// One executed instruction in the trace: the cycle count and registers
// before it, and the last memory access it made when it made one. index is
// its place among the retained entries, oldest first. The instruction text
// and the symbols are filled by the command that reports it.
struct TraceRecord
{
    uint64_t     cycles        = 0;
    Word         pc            = 0;
    Byte         opcode        = 0;
    Byte         op1           = 0;
    Byte         op2           = 0;
    Byte         length        = 0;       // the instruction's bytes; 0 until it is disassembled, which shows all three
    Byte         a             = 0;
    Byte         x             = 0;
    Byte         y             = 0;
    Byte         sp            = 0;
    Byte         p             = 0;
    bool         isInterrupt   = false;   // the first instruction of an interrupt handler
    bool         isAvailable   = true;    // false when history could not be replayed there, or not yet
    bool         isRebuilding  = false;   // history's rebuild of it is running
    bool         hasAccess     = false;
    bool         accessIsWrite = false;
    Byte         accessData    = 0;
    Word         accessAddress = 0;
    std::string  instruction;
    std::string  symbol;
    std::string  accessSymbol;
};

// HISTORY: whether the trace is on, how many entries it retains, and the
// window of them the command asked for.
struct TraceData
{
    bool                      isOn  = false;
    uint64_t                  total = 0;
    std::vector<TraceRecord>  entries;
};

// PROJECT: the debug project open: what it is for, its file, and whether
// that is its scope's automatic file.
struct ProjectData
{
    std::string   scope;
    std::string   key;
    std::string   label;
    std::string   file;
    bool          isAutomatic = true;
};

// BASICVARS: the Applesoft program's simple variables and arrays.
struct BasicVariablesData
{
    std::vector<BasicVariable>  variables;
    std::vector<BasicArray>     arrays;
};

// BASICSTACK: its GOSUB and FOR frames, newest first.
struct BasicStackData
{
    std::vector<BasicFrame>  frames;
};

using ReplyData = std::variant<MessageData,
                               RegistersData,
                               MemoryData,
                               DisassemblyData,
                               BreakpointSetData,
                               BreakpointListData,
                               WatchListData,
                               SearchHitsData,
                               StackData,
                               SoftSwitchData,
                               SymbolData,
                               CyclesData,
                               ModeData,
                               FileIoData,
                               CompareData,
                               DataBlockListData,
                               VideoInfoData,
                               BranchRecordData,
                               ProfileData,
                               CalcData,
                               StepFilterData,
                               CallStackData,
                               CallStackModeData,
                               TraceData,
                               BasicVariablesData,
                               BasicStackData,
                               ProjectData>;





////////////////////////////////////////////////////////////////////////////////
//
//  Reply
//
//  error holds the two-line error shape for Error and NotAvailable.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplyError
{
    std::string  label;
    std::string  detail;
    std::string  usage;     // the command's syntax line, for wrong arguments
};

//  verb is the operation the reply answers, for a format that writes two
//  operations with the same data differently: GSSquared prints nothing for a
//  deposit and a dump for an examine, and both reply with memory rows.
struct Reply
{
    CommandStatus             status = CommandStatus::Ok;
    DebugVerb                 verb   = DebugVerb::None;
    std::string               command;
    CommandMode               mode   = CommandMode::AppleWin;   // the dialect command is written in
    ReplyData                 data;
    std::vector<std::string>  text;
    ReplyError                error;

    //  A line to offer in the command box for a word the mode does not have:
    //  this mode's equivalent or the closest command, with the arguments
    //  typed. Empty for none.
    std::string               suggestion;

    //  The text is final, as a line of several commands has it: each was
    //  formatted as it ran, errors included, so none is formatted again.
    bool                      isFormatted = false;

    void SetError (CommandStatus errorStatus, const std::string & label, const std::string & detail)
    {
        status       = errorStatus;
        error.label  = label;
        error.detail = detail;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  StopEvent
//
////////////////////////////////////////////////////////////////////////////////

enum class StopReason
{
    Breakpoint,
    Watchpoint,
    Step,
    RunTo,
    Budget,
    Pause,
    Brk,
    InvalidOpcode,
    Reset,
    Disk,
    BasicStep,
    BasicError,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldEvent
//
//  A field the disk breakpoints' tracker followed to its last nibble: an
//  address field to its checksum, a data field to its checksum nibble, or a
//  data prologue written after an address field was read. The instructions
//  are those that read the address prologue's first nibble, the data
//  prologue's first nibble and the event's last nibble; isPassed is the read
//  test's answer, the first and the last being one instruction.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskFieldEventKind
{
    AddressField,
    DataChecksum,
    WritePrologue,
};

struct DiskFieldEvent
{
    DiskFieldEventKind  kind                 = DiskFieldEventKind::AddressField;
    DiskFieldKind       format               = DiskFieldKind::Sixteen;
    Byte                track                = 0;
    Byte                sector               = 0;
    Byte                volume               = 0;
    Byte                storedChecksum       = 0;
    Byte                expectedChecksum     = 0;
    bool                isChecksumRead       = true;      // both of the address checksum's nibbles were read
    Byte                storedDataChecksum   = 0;
    Byte                expectedDataChecksum = 0;
    bool                isDataChecksumGood   = false;
    bool                isWrongTrack         = false;
    Word                addressInstruction   = 0;
    Word                dataInstruction      = 0;
    Word                lastInstruction      = 0;
    bool                isPassed             = false;
    int                 drive                = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallRequest
//
//  The request of an RWTS or driver call, read at the routine's entry: the
//  IOB for RWTS, $42-$47 for ProDOS's driver.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskCallRequest
{
    bool  isDriver = false;
    int   slot     = 0;
    int   drive    = 0;
    Byte  volume   = 0;
    Byte  track    = 0;
    Byte  sector   = 0;
    Byte  command  = 0;
    Byte  unit     = 0;
    Word  buffer   = 0;
    Word  block    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReport
//
//  The stop of a disk breakpoint: the lowest disk id that matched, the
//  event, the controller's state at it, the instruction responsible and what
//  the event has of its own.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskSectorOrder
{
    Dos33,
    ProDos,
};

struct DiskReport
{
    int                             breakpointId         = 0;
    DiskEvent                       event                = DiskEvent::SectorRead;
    int                             slot                 = 0;
    int                             drive                = 0;
    int                             quarterTrack         = 0;
    int                             previousQuarterTrack = 0;
    bool                            isMotorOn            = false;
    bool                            isProtected          = false;
    bool                            isBump               = false;
    std::optional<DiskMotorEvent>   motorEvent;
    Byte                            phases               = 0;
    Word                            instruction          = 0;
    std::optional<DiskFieldEvent>   field;
    std::array<Byte, 3>             epilogue             = {};
    bool                            isEpilogueRandom     = false;
    std::optional<Byte>             logicalSector;
    std::optional<Word>             block;
    std::vector<Byte>               physicalSectors;
    DiskSectorOrder                 order                = DiskSectorOrder::Dos33;
    std::optional<DiskCallRequest>  request;
    Word                            vector               = 0;
    int                             previousDrive        = 0;
    std::string                     imageName;
};

struct WatchHit
{
    int                     id          = 0;
    Word                    address     = 0;
    Byte                    value       = 0;
    std::optional<Byte>     previous;             // a memory write's replaced byte
    WatchAccess             access      = WatchAccess::Read;
    Word                    accessPc    = 0;
    WatchMode               mode        = WatchMode::After;
    std::optional<int32_t>  conditionValue;       // the IF expression's value, when there is one
    bool                    isUnwritten = false;  // a read of RAM nothing had written, BRKUNINIT's stop
};

//  An Applesoft stop's report: the line being run, none in direct
//  mode; for a variable breakpoint the variable, the line of the statement
//  that changed it and its value before and after; for an error stop the
//  error's code and message, and whether ONERR was on to handle it.
struct BasicStopReport
{
    std::optional<Word>  line;
    std::string          variable;
    std::optional<Word>  changeLine;
    std::string          previous;
    std::string          value;
    std::optional<Byte>  errorCode;
    std::string          error;
    bool                 isHandled   = false;
    bool                 isBasicStop = false;     // the Applesoft entry or step made the stop, not another beside it
};

struct StopEvent
{
    StopReason               reason    = StopReason::Pause;
    Word                     pc        = 0;
    std::optional<int>       breakpointId;
    std::optional<WatchHit>  watch;
    uint64_t                 cycles    = 0;
    Cpu6502Registers         registers = {};

    //  The source line at pc when a debug file is loaded and a line produced
    //  it: the innermost, when macros nest. An empty file means none.
    std::string              sourceFile;
    int                      sourceLine = 0;

    //  The IF expression of the breakpoint or watchpoint that stopped the
    //  machine, and its value at the hit. An empty condition means none.
    std::string              condition;
    std::optional<int32_t>   conditionValue;

    //  Present when a reverse execution command moved the machine through its
    //  history: how the command ended. The reason is Step.
    std::optional<ReverseOutcome>  history;
    //  A disk breakpoint's report: present with the reason Disk, and with
    //  Breakpoint or Watchpoint when an entry of another kind matched at the
    //  same stop.
    std::optional<DiskReport>      disk;

    //  Present for an Applesoft breakpoint, step or error stop.
    std::optional<BasicStopReport> basic;
};





////////////////////////////////////////////////////////////////////////////////
//
//  RunRequest
//
//  A run with no budget is unbounded. StepOver completes when PC reaches the
//  instruction after the JSR with the stack pointer back where it was, so a
//  recursive subroutine is stepped over as one call. A Go or RunTo with a
//  skip range also ends when PC leaves that range.
//
////////////////////////////////////////////////////////////////////////////////

enum class RunKind
{
    Go,
    StepInto,
    StepOver,
    StepOut,
    RunTo,
    Trace,
};

struct RunRequest
{
    RunKind                  kind       = RunKind::Go;
    bool                     fullSpeed  = false;
    bool                     hasUntilPc = false;
    Word                     untilPc    = 0;
    bool                     hasSkip    = false;
    Word                     skipFirst  = 0;
    Word                     skipLast   = 0;
    uint32_t                 count      = 1;
    std::optional<uint64_t>  budget;

    //  Present when a step goes by source line rather than by instruction.
    const LineTable        * lineTable  = nullptr;

    //  The routines a step into runs as a step over, when there are any.
    const StepFilter       * stepFilter = nullptr;

    //  Present for a step out that began inside a recorded call or interrupt:
    //  asked before each instruction, with the PC and stack pointer it will
    //  run with, whether that frame has ended. Absent, a step out ends on the
    //  stack pointer alone.
    std::function<bool (Word pc, Byte sp)>  hasLeftFrame;
};
