#include "Pch.h"

#include "Debugger/ReplyJson.h"

#include "Core/JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplyJson::WriteReply
//
//  data is omitted on error and unknown, error on ok.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReplyJson::WriteReply (const Reply & reply, std::optional<int64_t> id)
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
        members.emplace_back ("watch", JsonValue (Members { { "id",       MakeNumber (stop.watch->id) },
                                                            { "address",  MakeNumber (stop.watch->address) },
                                                            { "value",    MakeNumber (stop.watch->value) },
                                                            { "access",   MakeString (GetAccessName (stop.watch->access)) },
                                                            { "accessPc", MakeNumber (stop.watch->accessPc) } }));
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
    static constexpr const char * kNames[] = { "address", "opcode", "register", "memory", "io", "brk", "interrupt" };



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
    return mode == CommandMode::Monitor ? "monitor" : "applewin";
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

    return JsonValue (Members { { "kind", MakeString ("message") } });
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

        lines.push_back (JsonValue (Members { { "address",    MakeNumber (line.instruction.address) },
                                              { "bytes",      JsonValue (std::move (bytes)) },
                                              { "mnemonic",   MakeString (line.instruction.mnemonic) },
                                              { "operand",    MakeString (line.instruction.operand) },
                                              { "target",     line.instruction.hasTarget ? MakeNumber (line.instruction.target) : JsonValue (nullptr) },
                                              { "symbol",     line.symbol.empty() ? JsonValue (nullptr) : MakeString (line.symbol) },
                                              { "documented", JsonValue (line.instruction.documented) } }));
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
    bool     hasRange = breakpoint.kind == BreakpointKind::Address || breakpoint.kind == BreakpointKind::Memory || breakpoint.kind == BreakpointKind::Io;



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

    if (breakpoint.kind == BreakpointKind::Register)
    {
        members.emplace_back ("condition", MakeString (breakpoint.condition));
    }

    if (breakpoint.kind == BreakpointKind::Memory)
    {
        members.emplace_back ("access", MakeString (GetAccessName (breakpoint.access)));
    }

    members.emplace_back ("enabled", JsonValue (breakpoint.enabled));
    members.emplace_back ("hits",    MakeNumber (breakpoint.hits));
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
