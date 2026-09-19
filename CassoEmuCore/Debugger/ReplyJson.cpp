#include "Pch.h"

#include "Debugger/ReplyJson.h"

#include "Core/JsonWriter.h"
#include "Debugger/CallStack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteReply
//
//  data is omitted on error and unknown, error on ok.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteReply (const Reply & reply, std::optional<int64_t> id)
{
    return WriteReply (reply, id, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteReply
//
//  `running` is written only when true. Absent is the ordinary case -- the
//  command is finished -- and a client reading an older record treats a
//  missing field the same way.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteReply (const Reply & reply, std::optional<int64_t> id, bool isRunning)
{
    Members  members;



    members.emplace_back ("type", MakeString ("reply"));

    if (id.has_value())
    {
        members.emplace_back ("id", MakeNumber (*id));
    }

    members.emplace_back ("status",  MakeString (GetStatusName (reply.status)));
    members.emplace_back ("command", MakeString (reply.command));

    if (reply.status == CommandStatus::Ok)
    {
        members.emplace_back ("data", MakeData (reply.data));
    }

    members.emplace_back ("text", MakeTextArray (reply.text));

    if (isRunning)
    {
        members.emplace_back ("running", JsonValue (true));
    }

    if (reply.status != CommandStatus::Ok)
    {
        members.emplace_back ("error", JsonValue (Members { { "label",  MakeString (reply.error.label)  },
                                                            { "detail", MakeString (reply.error.detail) } }));
    }

    return WriteLine (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteStopped
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteStopped (const StopEvent & stop, std::optional<int64_t> causeId)
{
    Members  members;



    members.emplace_back ("type",   MakeString ("stopped"));
    members.emplace_back ("reason", MakeString (GetStopReasonName (stop.reason)));
    members.emplace_back ("pc",     MakeNumber (stop.pc));

    if (causeId.has_value())
    {
        members.emplace_back ("causeId", MakeNumber (*causeId));
    }

    if (stop.breakpointId.has_value())
    {
        members.emplace_back ("breakpointId", MakeNumber (*stop.breakpointId));
    }

    if (stop.watch.has_value())
    {
        Members  watch;



        watch.emplace_back ("id",      MakeNumber (stop.watch->id));
        watch.emplace_back ("address", MakeNumber (stop.watch->address));
        watch.emplace_back ("value",   MakeNumber (stop.watch->value));

        if (stop.watch->previous.has_value())
        {
            watch.emplace_back ("previous", MakeNumber (*stop.watch->previous));
        }

        watch.emplace_back ("access",   MakeString (GetAccessName (stop.watch->access)));
        watch.emplace_back ("accessPc", MakeNumber (stop.watch->accessPc));
        watch.emplace_back ("mode",     MakeString (GetWatchModeName (stop.watch->mode)));
        members.emplace_back ("watch", JsonValue (std::move (watch)));
    }

    if (!stop.condition.empty())
    {
        members.emplace_back ("condition", MakeString (stop.condition));
    }

    if (stop.conditionValue.has_value())
    {
        members.emplace_back ("conditionValue", MakeNumber (*stop.conditionValue));
    }

    if (stop.sourceLine > 0)
    {
        Members  source;



        source.emplace_back ("file", MakeString (stop.sourceFile));
        source.emplace_back ("line", MakeNumber (stop.sourceLine));
        members.emplace_back ("source", JsonValue (std::move (source)));
    }

    members.emplace_back ("cycles",    MakeNumber ((int64_t) stop.cycles));
    members.emplace_back ("registers", MakeRegisters (stop.registers));
    return WriteLine (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson notifications
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteResumed()
{
    return WriteLine (Members { { "type", MakeString ("resumed") } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteReset
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteReset (bool isPowerCycle)
{
    return WriteLine (Members { { "type", MakeString ("reset") },
                                { "kind", MakeString (isPowerCycle ? "power" : "soft") } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteMachineChanged
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteMachineChanged (const std::string & machineName)
{
    return WriteLine (Members { { "type",    MakeString ("machineChanged") },
                                { "machine", MakeString (machineName) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteModeChanged
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteModeChanged (CommandMode mode)
{
    return WriteLine (Members { { "type", MakeString ("modeChanged") },
                                { "mode", MakeString (GetModeName (mode)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteClosing
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteClosing()
{
    return WriteLine (Members { { "type", MakeString ("closing") } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetStatusName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetStatusName (CommandStatus status)
{
    static constexpr const char * kNames[] = { "ok", "error", "notAvailable", "unknown" };



    return kNames[(int) status];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetRegionName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetRegionName (MemoryRegion region)
{
    static constexpr const char * kNames[] = { "mainRam", "auxRam", "lcBank1", "lcBank2", "rom", "slotRom", "io" };



    return kNames[(int) region];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetBreakpointKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetBreakpointKindName (BreakpointKind kind)
{
    static constexpr const char * kNames[] = { "address", "opcode", "register", "memory", "io", "brk", "interrupt", "memoryValue" };



    return kNames[(int) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetAccessName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetAccessName (WatchAccess access)
{
    static constexpr const char * kNames[] = { "read", "write", "readWrite" };



    return kNames[(int) access];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetStopReasonName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetStopReasonName (StopReason reason)
{
    static constexpr const char * kNames[] = { "breakpoint", "watchpoint", "step", "runTo", "budget", "pause", "brk", "invalidOpcode", "reset" };



    return kNames[(int) reason];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetSymbolTableName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetSymbolTableName (SymbolTableId table)
{
    static constexpr const char * kNames[] = { "main", "basic", "asm", "user", "user2", "src", "src2", "dos33", "prodos" };



    return kNames[(int) table];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetModeName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetModeName (CommandMode mode)
{
    return mode == CommandMode::Monitor ? "monitor"
         : mode == CommandMode::WinDbg  ? "windbg"
         :                                "applewin";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetWatchModeName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetWatchModeName (WatchMode mode)
{
    return mode == WatchMode::Before ? "before" : "after";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::GetDataBlockKindName
//
////////////////////////////////////////////////////////////////////////////////

const char * ReplyJson::GetDataBlockKindName (DataBlockKind kind)
{
    static constexpr const char * kNames[] = { "bytes", "words", "address", "text", "float" };



    return kNames[(int) kind];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteLine
//
//  Compact JSON escapes every control character inside strings, so the record
//  holds no raw newline.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteLine (Members && members)
{
    JsonWriter::Options  options;
    std::string          text;
    HRESULT              hr = S_OK;



    options.fPretty = false;
    hr = JsonWriter::Write (JsonValue (std::move (members)), options, text);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeNumber / MakeString
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeNumber (int64_t value)
{
    return JsonValue ((double) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeString
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeString (const std::string & value)
{
    return JsonValue (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeRegisters
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeRegisters (const Cpu6502Registers & r)
{
    return JsonValue (Members { { "a",  MakeNumber (r.a)  },
                                { "x",  MakeNumber (r.x)  },
                                { "y",  MakeNumber (r.y)  },
                                { "p",  MakeNumber (r.p)  },
                                { "s",  MakeNumber (r.sp) },
                                { "pc", MakeNumber (r.pc) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeData
//
//  One object per data kind, identified by "kind".
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeData (const ReplyData & data)
{
    static constexpr const char * kFlagNames[] = { "n", "v", "r", "b", "d", "i", "z", "c" };
    Members  members;



    if (auto * v = std::get_if<RegistersData> (&data))
    {
        Members  flags;



        members = MakeRegisters (v->registers).GetObjectEntries();

        for (int i = 0; i < 8; ++i)
        {
            flags.emplace_back (kFlagNames[i], JsonValue ((v->registers.p & (0x80 >> i)) != 0));
        }

        members.insert (members.begin(), { "kind", MakeString ("registers") });
        members.emplace_back ("flags", JsonValue (std::move (flags)));
        return JsonValue (std::move (members));
    }

    if (auto * v = std::get_if<MemoryData>         (&data)) { return MakeMemory      (*v); }
    if (auto * v = std::get_if<DisassemblyData>    (&data)) { return MakeDisassembly (*v); }
    if (auto * v = std::get_if<WatchListData>      (&data)) { return MakeWatchList   (*v); }
    if (auto * v = std::get_if<StackData>          (&data)) { return MakeStack       (*v); }
    if (auto * v = std::get_if<SoftSwitchData>     (&data)) { return MakeSoftSwitches(*v); }
    if (auto * v = std::get_if<SymbolData>         (&data)) { return MakeSymbols     (*v); }

    if (auto * v = std::get_if<BreakpointSetData> (&data))
    {
        return JsonValue (Members { { "kind", MakeString ("breakpointSet") }, { "breakpoint", MakeBreakpoint (v->breakpoint) } });
    }

    if (auto * v = std::get_if<BreakpointListData> (&data))
    {
        std::vector<JsonValue>  list;



        for (const BreakpointInfo & breakpoint : v->breakpoints)
        {
            list.push_back (MakeBreakpoint (breakpoint));
        }

        return JsonValue (Members { { "kind", MakeString ("breakpointList") }, { "breakpoints", JsonValue (std::move (list)) } });
    }

    if (auto * v = std::get_if<SearchHitsData> (&data))
    {
        std::vector<JsonValue>  addresses;



        for (Word address : v->addresses)
        {
            addresses.push_back (MakeNumber (address));
        }

        return JsonValue (Members { { "kind", MakeString ("searchHits") }, { "addresses", JsonValue (std::move (addresses)) } });
    }

    if (auto * v = std::get_if<CyclesData> (&data))
    {
        return JsonValue (Members { { "kind", MakeString ("cycles") }, { "count", MakeNumber ((int64_t) v->count) } });
    }

    if (auto * v = std::get_if<ModeData> (&data))
    {
        return JsonValue (Members { { "kind", MakeString ("mode") }, { "mode", MakeString (GetModeName (v->mode)) } });
    }

    if (auto * v = std::get_if<FileIoData> (&data))
    {
        return JsonValue (Members { { "kind",        MakeString ("fileIo") },
                                    { "path",        MakeString (v->path) },
                                    { "requested",   MakeNumber (v->requested) },
                                    { "transferred", MakeNumber (v->transferred) },
                                    { "mismatch",    JsonValue (v->mismatch) } });
    }

    if (auto * v = std::get_if<CompareData>       (&data)) { return MakeCompare    (*v); }
    if (auto * v = std::get_if<DataBlockListData> (&data)) { return MakeDataBlocks (*v); }
    if (auto * v = std::get_if<StepFilterData>    (&data)) { return MakeStepFilter (*v); }
    if (auto * v = std::get_if<CallStackData>     (&data)) { return MakeCallStack  (*v); }

    if (auto * v = std::get_if<CallStackModeData> (&data))
    {
        return JsonValue (Members { { "kind", MakeString ("callStackMode") }, { "mechanism", MakeString (CallStack::GetMechanismName (v->mechanism)) } });
    }

    if (auto * v = std::get_if<ProfileData>       (&data)) { return MakeProfile    (*v); }
    if (auto * v = std::get_if<TraceData>         (&data)) { return MakeTrace      (*v); }

    if (auto * v = std::get_if<VideoInfoData> (&data))
    {
        return JsonValue (Members { { "kind",        MakeString ("videoInfo") },
                                    { "scanline",    MakeNumber (v->scanline) },
                                    { "cycleInLine", MakeNumber (v->cycleInLine) } });
    }

    if (auto * v = std::get_if<BranchRecordData> (&data))
    {
        return JsonValue (Members { { "kind",    MakeString ("branchRecord") },
                                    { "address", v->address.has_value() ? MakeNumber (*v->address) : JsonValue (nullptr) } });
    }

    if (auto * v = std::get_if<CalcData> (&data))
    {
        return JsonValue (Members { { "kind", MakeString ("calc") }, { "value", MakeNumber (v->value) } });
    }

    return JsonValue (Members { { "kind", MakeString ("message") } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeCompare
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeCompare (const CompareData & data)
{
    std::vector<JsonValue>  differences;



    for (const CompareDifference & difference : data.differences)
    {
        differences.push_back (JsonValue (Members { { "address",    MakeNumber (difference.address) },
                                                    { "value",      MakeNumber (difference.value) },
                                                    { "other",      MakeNumber (difference.other) },
                                                    { "otherValue", MakeNumber (difference.otherValue) } }));
    }

    return JsonValue (Members { { "kind",        MakeString ("compare") },
                                { "compared",    MakeNumber (data.compared) },
                                { "differences", JsonValue (std::move (differences)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDataBlocks
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDataBlocks (const DataBlockListData & data)
{
    std::vector<JsonValue>  blocks;



    for (const DataBlock & block : data.blocks)
    {
        blocks.push_back (JsonValue (Members { { "name",  MakeString (block.name) },
                                               { "first", MakeNumber (block.first) },
                                               { "last",  MakeNumber (block.last) },
                                               { "kind",  MakeString (GetDataBlockKindName (block.kind)) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("dataBlocks") }, { "blocks", JsonValue (std::move (blocks)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeStepFilter
//
//  An entry given as an address or a range has an empty name.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeStepFilter (const StepFilterData & data)
{
    std::vector<JsonValue>  entries;



    for (const StepFilterEntry & entry : data.entries)
    {
        entries.push_back (JsonValue (Members { { "name",  MakeString (entry.name) },
                                                { "first", MakeNumber (entry.first) },
                                                { "last",  MakeNumber (entry.last) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("stepFilter") }, { "entries", JsonValue (std::move (entries)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeCallStack
//
//  Rows innermost first, each a frame or a break; a symbol or a note that is
//  absent is null.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeCallStack (const CallStackData & data)
{
    std::vector<JsonValue>  rows;



    for (const CallStackRow & row : data.rows)
    {
        if (row.chainBreak.has_value())
        {
            rows.push_back (JsonValue (Members { { "break",  MakeString (CallStack::GetBreakKindName (row.chainBreak->kind)) },
                                                 { "pc",     MakeNumber (row.chainBreak->pc) },
                                                 { "opcode", MakeNumber (row.chainBreak->opcode) },
                                                 { "text",   MakeString (CallStack::DescribeBreak (*row.chainBreak)) } }));
        }
        else if (row.frame.has_value())
        {
            rows.push_back (MakeCallFrame (*row.frame));
        }
    }

    return JsonValue (Members { { "kind",       MakeString ("callStack") },
                                { "mechanism",  MakeString (CallStack::GetMechanismName (data.mechanism)) },
                                { "rows",       JsonValue (std::move (rows)) },
                                { "lastReturn", data.lastReturn.has_value() ? MakeCallFrame (*data.lastReturn) : JsonValue (nullptr) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeCallFrame
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeCallFrame (const CallStackFrame & frame)
{
    return JsonValue (Members { { "callSite",   MakeNumber (frame.callSite) },
                                { "target",     MakeNumber (frame.target) },
                                { "type",       MakeString (CallStack::GetKindName (frame.kind)) },
                                { "provenance", MakeString (frame.provenance == CallProvenance::Recorded ? "recorded" : "guessed") },
                                { "stackLevel", MakeNumber (frame.stackLevel) },
                                { "verified",   JsonValue (frame.isVerified) },
                                { "symbol",     frame.symbol.empty() ? JsonValue (nullptr) : MakeString (frame.symbol) },
                                { "note",       frame.note.empty() ? JsonValue (nullptr) : MakeString (frame.note) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeProfile//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeProfile (const ProfileData & data)
{
    std::vector<JsonValue>  opcodes;
    std::vector<JsonValue>  addresses;



    for (const ProfileEntry & entry : data.opcodes)
    {
        opcodes.push_back (JsonValue (Members { { "mnemonic", MakeString (entry.mnemonic) },
                                                { "mode",     MakeString (entry.mode) },
                                                { "count",    MakeNumber ((int64_t) entry.count) },
                                                { "cycles",   MakeNumber ((int64_t) entry.cycles) } }));
    }

    for (const ProfileAddressEntry & entry : data.addresses)
    {
        addresses.push_back (JsonValue (Members { { "address", MakeNumber ((int64_t) entry.address) },
                                                  { "symbol",  entry.symbol.empty() ? JsonValue (nullptr) : MakeString (entry.symbol) },
                                                  { "cycles",  MakeNumber ((int64_t) entry.cycles) } }));
    }

    return JsonValue (Members { { "kind",         MakeString ("profile") },
                                { "on",           JsonValue (data.isOn) },
                                { "instructions", MakeNumber ((int64_t) data.instructions) },
                                { "cycles",       MakeNumber ((int64_t) data.cycles) },
                                { "opcodes",      JsonValue (std::move (opcodes)) },
                                { "penalties",    JsonValue (Members { { "pageCross",   MakeNumber ((int64_t) data.pageCross) },
                                                                       { "branchTaken", MakeNumber ((int64_t) data.branchTaken) },
                                                                       { "branchCross", MakeNumber ((int64_t) data.branchCross) } }) },
                                { "addresses",    JsonValue (std::move (addresses)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeTrace
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeTrace (const TraceData & data)
{
    std::vector<JsonValue>  entries;



    for (const TraceRecord & record : data.entries)
    {
        entries.push_back (MakeTraceRecord (record));
    }

    return JsonValue (Members { { "kind",    MakeString ("trace") },
                                { "on",      JsonValue (data.isOn) },
                                { "total",   MakeNumber ((int64_t) data.total) },
                                { "entries", JsonValue (std::move (entries)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeTraceRecord
//
//  An entry with no memory access has a null access.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeTraceRecord (const TraceRecord & record)
{
    JsonValue  access (nullptr);



    if (record.hasAccess)
    {
        access = JsonValue (Members { { "address",   MakeNumber ((int64_t) record.accessAddress) },
                                      { "direction", MakeString (record.accessIsWrite ? "write" : "read") },
                                      { "data",      MakeNumber ((int64_t) record.accessData) },
                                      { "symbol",    record.accessSymbol.empty() ? JsonValue (nullptr) : MakeString (record.accessSymbol) } });
    }

    return JsonValue (Members { { "index",       MakeNumber ((int64_t) record.index) },
                                { "cycles",      MakeNumber ((int64_t) record.cycles) },
                                { "pc",          MakeNumber ((int64_t) record.pc) },
                                { "bytes",       JsonValue (std::vector<JsonValue> { MakeNumber (record.opcode),
                                                                                     MakeNumber (record.op1),
                                                                                     MakeNumber (record.op2) }) },
                                { "instruction", MakeString (record.instruction) },
                                { "symbol",      record.symbol.empty() ? JsonValue (nullptr) : MakeString (record.symbol) },
                                { "a",           MakeNumber (record.a) },
                                { "x",           MakeNumber (record.x) },
                                { "y",           MakeNumber (record.y) },
                                { "sp",          MakeNumber (record.sp) },
                                { "p",           MakeNumber (record.p) },
                                { "interrupt",   JsonValue (record.isInterrupt) },
                                { "access",      std::move (access) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeMemory
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeMemory (const MemoryData & data)
{
    std::vector<JsonValue>  rows;



    for (const MemoryRow & row : data.rows)
    {
        std::vector<JsonValue>  bytes;



        for (const std::optional<Byte> & cell : row.bytes)
        {
            bytes.push_back (cell.has_value() ? MakeNumber (*cell) : JsonValue (nullptr));
        }

        rows.push_back (JsonValue (Members { { "address", MakeNumber (row.address) },
                                             { "bytes",   JsonValue (std::move (bytes)) },
                                             { "region",  MakeString (GetRegionName (row.region)) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("memory") }, { "rows", JsonValue (std::move (rows)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeDisassembly
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeDisassembly (const DisassemblyData & data)
{
    std::vector<JsonValue>  lines;



    for (const DisassemblyLine & line : data.lines)
    {
        std::vector<JsonValue>  bytes;



        for (Byte b : line.instruction.bytes)
        {
            bytes.push_back (MakeNumber (b));
        }

        lines.push_back (JsonValue (Members { { "address",        MakeNumber (line.instruction.address) },
                                              { "bytes",          JsonValue (std::move (bytes)) },
                                              { "mnemonic",       MakeString (line.instruction.mnemonic) },
                                              { "operand",        MakeString (line.instruction.operand) },
                                              { "operandAddress", line.instruction.hasOperandAddress ? MakeNumber (line.instruction.operandAddress) : JsonValue (nullptr) },
                                              { "operandSymbol",  line.operandSymbol.empty() ? JsonValue (nullptr) : MakeString (line.operandSymbol) },
                                              { "target",         line.instruction.hasTarget ? MakeNumber (line.instruction.target) : JsonValue (nullptr) },
                                              { "label",          line.label.empty() ? JsonValue (nullptr) : MakeString (line.label) },
                                              { "documented",     JsonValue (line.instruction.documented) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("disassembly") }, { "lines", JsonValue (std::move (lines)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeBreakpoint
//
//  Fields that do not apply to the kind are omitted.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeBreakpoint (const BreakpointInfo & breakpoint)
{
    Members  members;
    bool     hasRange = breakpoint.kind == BreakpointKind::Address || breakpoint.kind == BreakpointKind::Memory ||
                        breakpoint.kind == BreakpointKind::Io      || breakpoint.kind == BreakpointKind::MemoryValue;



    members.emplace_back ("id",   MakeNumber (breakpoint.id));
    members.emplace_back ("kind", MakeString (GetBreakpointKindName (breakpoint.kind)));

    if (hasRange)
    {
        members.emplace_back ("address", MakeNumber (breakpoint.address));
        members.emplace_back ("last",    MakeNumber (breakpoint.last));
    }

    if (breakpoint.kind == BreakpointKind::Opcode)
    {
        members.emplace_back ("opcode", MakeNumber (breakpoint.opcode));
    }

    if (breakpoint.value.has_value())
    {
        members.emplace_back ("value", MakeNumber (*breakpoint.value));
    }

    if (!breakpoint.condition.empty())
    {
        members.emplace_back ("condition", MakeString (breakpoint.condition));
    }

    if (breakpoint.kind == BreakpointKind::Memory)
    {
        members.emplace_back ("access", MakeString (GetAccessName (breakpoint.access)));
        members.emplace_back ("mode",   MakeString (GetWatchModeName (breakpoint.mode)));
    }

    members.emplace_back ("enabled",   JsonValue (breakpoint.enabled));
    members.emplace_back ("temporary", JsonValue (breakpoint.temporary));
    members.emplace_back ("stops",     JsonValue (breakpoint.stops));
    members.emplace_back ("hits",      MakeNumber (breakpoint.hits));
    return JsonValue (std::move (members));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeWatchList
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeWatchList (const WatchListData & data)
{
    static constexpr const char * kKinds[] = { "watchList", "zeroPageList", "bookmarkList" };
    std::vector<JsonValue>  entries;



    for (const WatchEntry & entry : data.entries)
    {
        entries.push_back (JsonValue (Members { { "id",      MakeNumber (entry.id) },
                                                { "address", MakeNumber (entry.address) },
                                                { "enabled", JsonValue (entry.enabled) },
                                                { "value",   entry.value.has_value() ? MakeNumber (*entry.value) : JsonValue (nullptr) } }));
    }

    return JsonValue (Members { { "kind", MakeString (kKinds[(int) data.kind]) }, { "entries", JsonValue (std::move (entries)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeStack
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeStack (const StackData & data)
{
    std::vector<JsonValue>  entries;



    for (const StackEntry & entry : data.entries)
    {
        entries.push_back (JsonValue (Members { { "address", MakeNumber (entry.address) }, { "value", MakeNumber (entry.value) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("stack") }, { "sp", MakeNumber (data.sp) }, { "entries", JsonValue (std::move (entries)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeSoftSwitches
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeSoftSwitches (const SoftSwitchData & data)
{
    std::vector<JsonValue>  switches;



    for (const SoftSwitch & entry : data.switches)
    {
        switches.push_back (JsonValue (Members { { "name", MakeString (entry.name) }, { "value", JsonValue (entry.value) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("softSwitches") }, { "switches", JsonValue (std::move (switches)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeSymbols
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeSymbols (const SymbolData & data)
{
    std::vector<JsonValue>  symbols;



    for (const SymbolInfo & symbol : data.symbols)
    {
        symbols.push_back (JsonValue (Members { { "name",    MakeString (symbol.name) },
                                                { "address", MakeNumber (symbol.address) },
                                                { "table",   MakeString (GetSymbolTableName (symbol.table)) } }));
    }

    return JsonValue (Members { { "kind", MakeString ("symbols") }, { "symbols", JsonValue (std::move (symbols)) } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::MakeTextArray
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ReplyJson::MakeTextArray (const std::vector<std::string> & lines)
{
    std::vector<JsonValue>  values;



    for (const std::string & line : lines)
    {
        values.push_back (MakeString (line));
    }

    return JsonValue (std::move (values));
}
