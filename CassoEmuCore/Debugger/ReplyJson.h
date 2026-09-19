#pragma once

#include "Core/JsonValue.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson
//
//  Replies and notifications as debug channel records: one JSON object per
//  line, with no raw newline. Addresses and bytes are integers, never hex
//  strings, and an unreadable byte is null. Batch mode's --json prints the
//  same records.
//
////////////////////////////////////////////////////////////////////////////////

class ReplyJson
{
public:
    static std::string  WriteReply          (const Reply & reply, std::optional<int64_t> id);

    //  The same, marked `"running":true` when the command left a run going, so
    //  a client knows a `stopped` naming this command is still to come.
    static std::string  WriteReply          (const Reply & reply, std::optional<int64_t> id, bool isRunning);
    static std::string  WriteStopped        (const StopEvent & stop, std::optional<int64_t> causeId);
    static std::string  WriteResumed        ();
    static std::string  WriteReset          (bool isPowerCycle);
    static std::string  WriteMachineChanged (const std::string & machineName);
    static std::string  WriteModeChanged    (CommandMode mode);
    static std::string  WriteClosing        ();

    static const char * GetStatusName       (CommandStatus status);
    static const char * GetRegionName       (MemoryRegion region);
    static const char * GetBreakpointKindName (BreakpointKind kind);
    static const char * GetAccessName       (WatchAccess access);
    static const char * GetStopReasonName   (StopReason reason);
    static const char * GetSymbolTableName  (SymbolTableId table);
    static const char * GetModeName         (CommandMode mode);
    static const char * GetWatchModeName    (WatchMode mode);
    static const char * GetDataBlockKindName (DataBlockKind kind);

private:
    using Members = std::vector<std::pair<std::string, JsonValue>>;

    static std::string  WriteLine           (Members && members);
    static JsonValue    MakeNumber          (int64_t value);
    static JsonValue    MakeString          (const std::string & value);
    static JsonValue    MakeRegisters       (const Cpu6502Registers & registers);
    static JsonValue    MakeData            (const ReplyData & data);
    static JsonValue    MakeMemory          (const MemoryData & data);
    static JsonValue    MakeDisassembly     (const DisassemblyData & data);
    static JsonValue    MakeBreakpoint      (const BreakpointInfo & breakpoint);
    static JsonValue    MakeWatchList       (const WatchListData & data);
    static JsonValue    MakeStack           (const StackData & data);
    static JsonValue    MakeSoftSwitches    (const SoftSwitchData & data);
    static JsonValue    MakeSymbols         (const SymbolData & data);
    static JsonValue    MakeCompare         (const CompareData & data);
    static JsonValue    MakeDataBlocks      (const DataBlockListData & data);
    static JsonValue    MakeStepFilter      (const StepFilterData & data);
    static JsonValue    MakeCallStack       (const CallStackData & data);
    static JsonValue    MakeCallFrame       (const CallStackFrame & frame);
    static JsonValue    MakeProfile         (const ProfileData & data);
    static JsonValue    MakeTextArray       (const std::vector<std::string> & lines);
};
