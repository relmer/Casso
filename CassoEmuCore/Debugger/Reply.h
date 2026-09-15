#pragma once

#include "I6502DebugInfo.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/Disassembler.h"





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
    Io,
    Brk,
    Interrupt,
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

struct MessageData
{
};

struct RegistersData
{
    Cpu6502Registers  registers = {};
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

struct DisassemblyLine
{
    DisassembledInstruction  instruction;
    std::string              symbol;
};

struct DisassemblyData
{
    std::vector<DisassemblyLine>  lines;
};

struct BreakpointInfo
{
    int             id        = 0;
    BreakpointKind  kind      = BreakpointKind::Address;
    Word            address   = 0;
    Word            last      = 0;
    Byte            opcode    = 0;
    std::string     condition;
    WatchAccess     access    = WatchAccess::ReadWrite;
    bool            enabled   = true;
    uint32_t        hits      = 0;
};

struct BreakpointSetData
{
    BreakpointInfo  breakpoint;
};

struct BreakpointListData
{
    std::vector<BreakpointInfo>  breakpoints;
};

struct WatchEntry
{
    int                   id      = 0;
    Word                  address = 0;
    bool                  enabled = true;
    std::optional<Word>   value;
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
    Word           address = 0;
    SymbolTableId  table   = SymbolTableId::Main;
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

struct FileIoData
{
    std::string  path;
    uint32_t     requested   = 0;
    uint32_t     transferred = 0;
    bool         mismatch    = false;
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
                               FileIoData>;





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
};

struct Reply
{
    CommandStatus             status = CommandStatus::Ok;
    std::string               command;
    ReplyData                 data;
    std::vector<std::string>  text;
    ReplyError                error;
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
};

struct WatchHit
{
    int          id       = 0;
    Word         address  = 0;
    Byte         value    = 0;
    WatchAccess  access   = WatchAccess::Read;
    Word         accessPc = 0;
};

struct StopEvent
{
    StopReason               reason    = StopReason::Pause;
    Word                     pc        = 0;
    std::optional<int>       breakpointId;
    std::optional<WatchHit>  watch;
    uint64_t                 cycles    = 0;
    Cpu6502Registers         registers = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  RunRequest
//
//  A run with no budget is unbounded. StepOver completes when PC reaches the
//  instruction after the JSR with the stack pointer back where it was, so a
//  recursive subroutine is stepped over as one call.
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
    uint32_t                 count      = 1;
    std::optional<uint64_t>  budget;
};
