#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"

#include "Debugger/CallStack.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DiskBreakSpecText.h"
#include "Debugger/ReplyJson.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::Format
//
//  Text a handler already wrote is kept; data adds its lines after it.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::Format (Reply & reply)
{
    if (reply.status != CommandStatus::Ok)
    {
        FormatError (reply);
        return;
    }

    FormatData (reply.data, reply.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatStop
//
//  A watchpoint stop says who touched what: the access, the value, the byte
//  a write replaced when it is known, and the instruction that did it. A
//  before-mode stop has no value yet. A read before written says so, with
//  the value read. A disk stop is its report, and any other stop with a disk
//  report has the report's lines after its own. Any other stop is its reason
//  and PC.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatStop (const StopEvent & stop)
{
    static constexpr const char * kReasons[] = { "Breakpoint", "Data breakpoint", "Step", "Stopped", "Budget", "Pause", "BRK", "Invalid opcode", "Reset", "Disk breakpoint",
                                                 "BASIC step", "BASIC error" };
    std::string                   text;



    if (stop.reason == StopReason::Watchpoint && stop.watch.has_value())
    {
        const WatchHit & hit = *stop.watch;



        if (hit.isUnwritten)
        {
            return std::format ("Read before written: ${:02X} from ${:04X} by ${:04X}", hit.value, hit.address, hit.accessPc);
        }

        if (hit.mode == WatchMode::Before)
        {
            text = std::format ("Data breakpoint #{}: {} of ${:04X} by ${:04X}, before the access",
                                hit.id, hit.access == WatchAccess::Read ? "read" : "write", hit.address, hit.accessPc);
        }
        else
        {
            text = std::format ("Data breakpoint #{}: {} ${:02X} {} ${:04X} by ${:04X}",
                                hit.id,
                                hit.access == WatchAccess::Read ? "Read" : "Write",
                                hit.value,
                                hit.access == WatchAccess::Read ? "from" : "to",
                                hit.address,
                                hit.accessPc);

            if (hit.previous.has_value())
            {
                text += std::format (" (was ${:02X})", *hit.previous);
            }
        }

        return text + FormatCondition (stop) + JoinDiskReport (stop);
    }

    if (stop.basic.has_value() && stop.basic->isBasicStop)
    {
        return DescribeBasicStop (stop) + JoinDiskReport (stop);
    }

    if (stop.reason == StopReason::Disk && stop.disk.has_value())
    {
        Lines  lines;



        FormatDiskReport (*stop.disk, lines);
        lines.front() += FormatCondition (stop);

        for (const std::string & line : lines)
        {
            text += (text.empty() ? "" : "\n") + line;
        }

        return text;
    }

    text = std::format ("{} at ${:04X}", kReasons[(int) stop.reason], stop.pc);

    if (stop.breakpointId.has_value())
    {
        text = std::format ("Breakpoint #{} at ${:04X}", *stop.breakpointId, stop.pc);
    }

    text += FormatCondition (stop);

    if (stop.sourceLine > 0)
    {
        text += std::format (", {} line {}", stop.sourceFile, stop.sourceLine);
    }

    if (stop.basic.has_value() && stop.basic->line.has_value())
    {
        text += std::format (", BASIC line {}", *stop.basic->line);
    }

    return text + JoinDiskReport (stop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::DescribeBasicStop
//
//  An error reads as Applesoft prints it, ?SYNTAX ERROR IN 20, with its code
//  and whether ONERR was on to take it. A string variable's values are in
//  quotes.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::DescribeBasicStop (const StopEvent & stop)
{
    const BasicStopReport  & report  = *stop.basic;
    std::string              at      = report.line.has_value() ? std::format ("line {}", *report.line) : std::string ("direct mode");
    bool                     isText  = report.variable.ends_with ('$');
    std::string              quote   = isText ? "\"" : "";



    if (stop.reason == StopReason::BasicStep)
    {
        return "BASIC step to " + at;
    }

    if (stop.reason == StopReason::BasicError)
    {
        return std::format ("BASIC error: ?{} ERROR{} (code ${:02X}{})",
                            report.error,
                            report.line.has_value() ? std::format (" IN {}", *report.line) : std::string(),
                            report.errorCode.value_or (0),
                            report.isHandled ? ", ONERR on" : "");
    }

    if (!report.variable.empty())
    {
        return std::format ("Breakpoint #{}: {} changed from {}{}{} to {}{}{}{}, stopped at {}",
                            stop.breakpointId.value_or (0), report.variable,
                            quote, report.previous, quote, quote, report.value, quote,
                            report.changeLine.has_value() ? std::format (" in line {}", *report.changeLine) : std::string(),
                            at);
    }

    return std::format ("Breakpoint #{} at BASIC {}", stop.breakpointId.value_or (0), at);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::JoinDiskReport
//
//  A disk report's lines, each after a newline, for a stop that came
//  another way; nothing when the stop has none.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::JoinDiskReport (const StopEvent & stop)
{
    Lines        lines;
    std::string  text;



    if (stop.disk.has_value())
    {
        FormatDiskReport (*stop.disk, lines);
    }

    for (const std::string & line : lines)
    {
        text += "\n" + line;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCondition
//
//  The IF expression that let the stop through and its value, or nothing.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatCondition (const StopEvent & stop)
{
    if (stop.condition.empty() || !stop.conditionValue.has_value())
    {
        return std::string();
    }

    int32_t  value = *stop.conditionValue;



    if (value < 0)
    {
        return std::format (", IF {} is -${:X}", stop.condition, (uint32_t) -(int64_t) value);
    }

    return std::format (", IF {} is ${:X}", stop.condition, (uint32_t) value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatFlags
//
//  NVRBDIZC, most significant first, with a letter for a set flag and a
//  period for a clear one.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatFlags (Byte p)
{
    static constexpr const char  kLetters[] = "NVRBDIZC";
    static constexpr int         kFlagCount = 8;
    std::string                  flags;



    for (int i = 0; i < kFlagCount; ++i)
    {
        flags += (p & (0x80 >> i)) ? kLetters[i] : '.';
    }

    return flags;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatAddress
//
//  A single address or an inclusive range.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatAddress (const BreakpointInfo & breakpoint)
{
    return (breakpoint.last > breakpoint.address)
         ? std::format ("${:04X}-${:04X}", breakpoint.address, breakpoint.last)
         : std::format ("${:04X}", breakpoint.address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatError
//
//  Line 1 is the label; line 2 states the detail, indented under the label's
//  text.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatError (Reply & reply)
{
    reply.text.push_back ("Error: " + reply.error.label);

    if (!reply.error.detail.empty())
    {
        reply.text.push_back (std::string (kDetailIndent, ' ') + reply.error.detail);
    }

    if (!reply.error.usage.empty())
    {
        reply.text.push_back (std::string (kDetailIndent, ' ') + "Usage: " + reply.error.usage);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatData
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatData (const ReplyData & data, Lines & lines)
{
    if      (auto * v = std::get_if<RegistersData>      (&data)) { FormatRegisters      (*v, lines); }
    else if (auto * v = std::get_if<MemoryData>         (&data)) { FormatMemory         (*v, lines); }
    else if (auto * v = std::get_if<DisassemblyData>    (&data)) { FormatDisassembly    (*v, lines); }
    else if (auto * v = std::get_if<BreakpointSetData>  (&data)) { FormatBreakpointSet  (*v, lines); }
    else if (auto * v = std::get_if<BreakpointListData> (&data)) { FormatBreakpointList (*v, lines); }
    else if (auto * v = std::get_if<WatchListData>      (&data)) { FormatWatchList      (*v, lines); }
    else if (auto * v = std::get_if<SearchHitsData>     (&data)) { FormatSearchHits     (*v, lines); }
    else if (auto * v = std::get_if<StackData>          (&data)) { FormatStack          (*v, lines); }
    else if (auto * v = std::get_if<SoftSwitchData>     (&data)) { FormatSoftSwitches   (*v, lines); }
    else if (auto * v = std::get_if<SymbolData>         (&data)) { FormatSymbols        (*v, lines); }
    else if (auto * v = std::get_if<FileIoData>         (&data)) { FormatFileIo         (*v, lines); }
    else if (auto * v = std::get_if<CompareData>        (&data)) { FormatCompare        (*v, lines); }
    else if (auto * v = std::get_if<DataBlockListData>  (&data)) { FormatDataBlocks     (*v, lines); }
    else if (auto * v = std::get_if<ProfileData>        (&data)) { FormatProfile        (*v, lines); }
    else if (auto * v = std::get_if<TraceData>          (&data)) { FormatTrace          (*v, lines); }
    else if (auto * v = std::get_if<CalcData>           (&data)) { FormatCalc           (*v, lines); }
    else if (auto * v = std::get_if<StepFilterData>     (&data)) { FormatStepFilter     (*v, lines); }
    else if (auto * v = std::get_if<CallStackData>      (&data)) { FormatCallStack      (*v, lines); }
    else if (auto * v = std::get_if<BasicVariablesData> (&data)) { FormatBasicVariables (*v, lines); }
    else if (auto * v = std::get_if<BasicStackData>     (&data)) { FormatBasicStack     (*v, lines); }
    else if (auto * v = std::get_if<ProjectData>        (&data))
    {
        lines.push_back (std::format ("Debug project for {}, {}", v->label, v->isAutomatic ? "saved automatically" : "saved where it was chosen"));
        lines.push_back ("File: " + v->file);
    }
    else if (auto * v = std::get_if<CallStackModeData>  (&data)) { lines.push_back (std::format ("Call stack: {}", CallStack::GetMechanismName (v->mechanism))); }
    else if (auto * v = std::get_if<MessageData>        (&data)) { lines.insert (lines.end(), v->lines.begin(), v->lines.end()); }
    else if (auto * v = std::get_if<CyclesData>         (&data)) { lines.push_back (std::format ("Cycles: {}", v->count)); }
    else if (auto * v = std::get_if<ModeData>           (&data)) { lines.push_back ("Mode: " + CommandModeNames::GetUpperName (v->mode)); }
    else if (auto * v = std::get_if<VideoInfoData>      (&data)) { lines.push_back (std::format ("Scanline ${:X}, cycle ${:X}", v->scanline, v->cycleInLine)); }
    else if (auto * v = std::get_if<BranchRecordData>   (&data)) { lines.push_back (v->address.has_value() ? std::format ("Last branch at ${:04X}", *v->address) : std::string ("No branch recorded.")); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatRegisters
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatRegisters (const RegistersData & data, Lines & lines)
{
    const Cpu6502Registers & r = data.registers;



    lines.push_back (std::format ("A:{:02X} X:{:02X} Y:{:02X} P:{:02X} S:{:02X} PC:{:04X}  {}",
                                  r.a, r.x, r.y, r.p, r.sp, r.pc, FormatFlags (r.p)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatMemory
//
//  Eight bytes per row and their characters with the high bit stripped. An
//  unreadable I/O byte shows as ?? and a period.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatMemory (const MemoryData & data, Lines & lines)
{
    static constexpr Byte  kLowBits    = 0x7F;
    static constexpr Byte  kFirstPrint = 0x20;
    static constexpr Byte  kLastPrint  = 0x7E;



    for (const MemoryRow & row : data.rows)
    {
        std::string  hex   = std::format ("{:04X}:", row.address);
        std::string  ascii;



        for (const std::optional<Byte> & cell : row.bytes)
        {
            Byte  ch = cell.has_value() ? (Byte) (*cell & kLowBits) : 0;



            hex   += cell.has_value() ? std::format (" {:02X}", *cell) : std::string (" ??");
            ascii += (cell.has_value() && ch >= kFirstPrint && ch <= kLastPrint) ? (char) ch : '.';
        }

        hex.append ((size_t) (kBytesPerRow - (int) row.bytes.size()) * 3, ' ');
        lines.push_back (hex + "  " + ascii);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDisassembly
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatDisassembly (const DisassemblyData & data, Lines & lines)
{
    static constexpr size_t  kBytesColumn = 9;
    size_t                   labelWidth   = 0;



    for (const DisassemblyLine & line : data.lines)
    {
        labelWidth = std::max (labelWidth, line.label.size());
    }

    for (const DisassemblyLine & line : data.lines)
    {
        std::string  bytes;
        std::string  label;
        std::string  text;



        for (Byte b : line.instruction.bytes)
        {
            bytes += std::format ("{:02X} ", b);
        }

        bytes.resize (std::max (bytes.size(), kBytesColumn), ' ');

        if (labelWidth > 0)
        {
            label = std::format ("{:<{}} ", line.label, labelWidth);
        }

        text = std::format ("{:04X}: {}{}{:<4} {}", line.instruction.address, bytes, label,
                            line.instruction.mnemonic, line.GetShownOperand());

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatBreakpointSet
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatBreakpointSet (const BreakpointSetData & data, Lines & lines)
{
    const BreakpointInfo  & info   = data.breakpoint;
    bool                    isCall = info.disk.has_value() && (info.disk->event == DiskEvent::RwtsCall || info.disk->event == DiskEvent::DriverCall);
    const char            * vector = (isCall && info.disk->event == DiskEvent::RwtsCall) ? "$03D9 has no JMP" : "the device vector has no routine";



    if (info.kind != BreakpointKind::Disk)
    {
        lines.push_back (std::format ("Breakpoint #{} set {}", info.id, DescribeBreakpoint (info)));
    }
    else
    {
        lines.push_back (std::format ("Set disk breakpoint #{} ({}){}", info.id, DiskBreakSpecText::GetDescription (*info.disk),
                                      (isCall && !info.resolved.has_value()) ? std::format (", unresolved: {} yet", vector) : std::string()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::AppendVideoBreak
//
//  The BPV scanline break, after the list, in every mode's list layout: it
//  has no id, so no layout has a column for it.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::AppendVideoBreak (const BreakpointListData & data, std::vector<std::string> & lines)
{
    if (data.videoBreak.has_value())
    {
        lines.push_back (std::format ("Video break on scanlines ${:X}-${:X}, cleared when it fires.", data.videoBreak->first, data.videoBreak->second));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatBreakpointList
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatBreakpointList (const BreakpointListData & data, Lines & lines)
{
    if (data.breakpoints.empty() && !data.videoBreak.has_value())
    {
        lines.push_back ("No breakpoints.");
        return;
    }

    for (const BreakpointInfo & breakpoint : data.breakpoints)
    {
        lines.push_back (std::format ("#{} {} {}{}, hits {}",
                                      breakpoint.id,
                                      breakpoint.enabled ? "enabled " : "disabled",
                                      DescribeBreakpoint (breakpoint),
                                      breakpoint.name.empty() ? std::string() : std::format (", \"{}\"", breakpoint.name),
                                      breakpoint.hits));
    }

    AppendVideoBreak (data, lines);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatWatchList
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatWatchList (const WatchListData & data, Lines & lines)
{
    static constexpr const char * kEmpty[] = { "No watches.", "No zero-page pointers.", "No bookmarks." };



    if (data.entries.empty())
    {
        lines.push_back (kEmpty[(int) data.kind]);
        return;
    }

    for (const WatchEntry & entry : data.entries)
    {
        std::string  text = !entry.basicName.empty()  ? std::format ("#{} {}", entry.id, entry.basicName)
                          : !entry.expression.empty() ? std::format ("#{} {}", entry.id, entry.expression)
                          :                             std::format ("#{} ${:04X}", entry.id, entry.address);



        if (!entry.basicName.empty())
        {
            text += " = " + entry.basicValue.value_or ("--");
        }
        else if (entry.valueText.has_value())
        {
            text += " = $" + *entry.valueText;
        }
        else if (!entry.expression.empty() && !entry.value.has_value())
        {
            text += " = --";
        }
        else if (entry.value.has_value() && data.kind == WatchListKind::ZeroPage)
        {
            text += std::format (" -> ${:04X}", *entry.value);
        }
        else if (entry.value.has_value())
        {
            text += std::format (" = ${:04X}", *entry.value);
        }

        if (!entry.enabled)
        {
            text += " (disabled)";
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatSearchHits
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatSearchHits (const SearchHitsData & data, Lines & lines)
{
    std::string  text = std::format ("Found {}:", data.addresses.size());



    if (data.addresses.empty())
    {
        lines.push_back ("Not found.");
        return;
    }

    for (Word address : data.addresses)
    {
        text += std::format (" ${:04X}", address);
    }

    lines.push_back (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatStack
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatStack (const StackData & data, Lines & lines)
{
    lines.push_back (std::format ("S:{:02X}", data.sp));

    for (const StackEntry & entry : data.entries)
    {
        lines.push_back (std::format ("{:04X}: {:02X}", entry.address, entry.value));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatSoftSwitches
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatSoftSwitches (const SoftSwitchData & data, Lines & lines)
{
    for (const SoftSwitch & entry : data.switches)
    {
        lines.push_back (std::format ("{:<10} {}", entry.name, entry.value ? "on" : "off"));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatSymbols
//
//  `$FDED COUT (main)`, with `, constant` after the table for an equate.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatSymbols (const SymbolData & data, Lines & lines)
{
    if (data.symbols.empty())
    {
        lines.push_back ("No symbols.");
        return;
    }

    for (const SymbolInfo & symbol : data.symbols)
    {
        lines.push_back (std::format ("${:04X} {} ({}{})", symbol.address, symbol.name, ReplyJson::GetSymbolTableName (symbol.table),
                                      symbol.isConstant ? ", constant" : ""));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatFileIo
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatFileIo (const FileIoData & data, Lines & lines)
{
    std::string  text = data.isLoad
                            ? std::format ("Loaded {} bytes at ${:04X}{}", data.transferred, data.address, data.echo)
                            : std::format ("Saved {} bytes from ${:04X}{}", data.transferred, data.address, data.echo);



    if (data.mismatch)
    {
        text += "; the file and the range differ in size";
    }

    lines.push_back (text);

    if (!data.companion.empty())
    {
        lines.push_back (data.companion);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCompare
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatCompare (const CompareData & data, Lines & lines)
{
    lines.push_back (std::format ("Compared {} byte{}, {} differ.", data.compared, data.compared == 1 ? "" : "s", data.differences.size()));

    for (const CompareDifference & difference : data.differences)
    {
        lines.push_back (std::format ("{:04X}: {:02X}  {:04X}: {:02X}", difference.address, difference.value, difference.other, difference.otherValue));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatDataBlocks
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatDataBlocks (const DataBlockListData & data, Lines & lines)
{
    static constexpr const char * kKinds[] = { "bytes", "words", "address", "text", "float" };



    if (data.blocks.empty())
    {
        lines.push_back ("No data blocks.");
        return;
    }

    for (const DataBlock & block : data.blocks)
    {
        lines.push_back (std::format ("{:<12} ${:04X}-${:04X}  {}", block.name, block.first, block.last, kKinds[(int) block.kind]));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatStepFilter
//
//  One routine a line: its name when it was given one, then its address or
//  range.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatStepFilter (const StepFilterData & data, Lines & lines)
{
    std::string  where;



    if (data.entries.empty())
    {
        lines.push_back ("The step filter is empty.");
        return;
    }

    for (const StepFilterEntry & entry : data.entries)
    {
        where = (entry.first == entry.last) ? std::format ("${:04X}", entry.first) : std::format ("${:04X}-${:04X}", entry.first, entry.last);
        lines.push_back (entry.name.empty() ? where : std::format ("{:<12} {}", entry.name, where));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatTrace
//
//  A line for the state and the count retained, then one per entry.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatTrace (const TraceData & data, Lines & lines)
{
    lines.push_back (std::format ("Trace {}, {} {} retained.", data.isOn ? "on" : "off", data.total, data.total == 1 ? "entry" : "entries"));

    for (const TraceRecord & record : data.entries)
    {
        lines.push_back (FormatTraceLine (record));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatTraceLine
//
//  Cycle count, address, the opcode bytes, the address's symbol, the
//  instruction, the registers before it, and the access: R or W, the
//  address, the byte, and the address's symbol. An interrupt handler's first
//  instruction is marked.
//
//         5678901  0300  AD 00 C0  START    LDA $C000      A=00 X=00 Y=00 SP=FF nv-BdIzc  R C000=8D KBD
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatTraceLine (const TraceRecord & record)
{
    std::string  line;



    if (!record.isAvailable)
    {
        return record.isRebuilding ? "  (being rebuilt from history)" : "  (not available: history could not be replayed here)";
    }

    line = std::format ("{:>12}  {:04X}  {:<8}  {:<8} {:<14} A={:02X} X={:02X} Y={:02X} SP={:02X} {}",
                        record.cycles, record.pc, FormatTraceBytes (record), record.symbol, record.instruction,
                        record.a, record.x, record.y, record.sp, FormatFlags (record.p));

    if (record.hasAccess)
    {
        line += std::format ("  {} {:04X}={:02X}", record.accessIsWrite ? 'W' : 'R', record.accessAddress, record.accessData);

        if (!record.accessSymbol.empty())
        {
            line += " " + record.accessSymbol;
        }
    }

    if (record.isInterrupt)
    {
        line += "  [interrupt]";
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatTraceBytes
//
//  An entry not yet disassembled has no length, and shows all three bytes the
//  trace keeps rather than guessing which of them the instruction used.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatTraceBytes (const TraceRecord & record)
{
    static constexpr Byte  kKept  = 3;
    const Byte             held[] = { record.opcode, record.op1, record.op2 };
    Byte                   count  = (record.length == 0 || record.length > kKept) ? kKept : record.length;
    std::string            text;



    for (Byte i = 0; i < count; i++)
    {
        text += std::format ("{}{:02X}", i == 0 ? "" : " ", held[i]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCallStack
//
//  A line a frame, innermost first: the call site, how it was entered, the
//  routine and its symbol, and which mechanism found it. A break is a line
//  of its own between dashes.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatCallStack (const CallStackData & data, Lines & lines)
{
    if (data.rows.empty())
    {
        lines.push_back ("No calls are on the stack.");
    }

    for (const CallStackRow & row : data.rows)
    {
        if (row.chainBreak.has_value())
        {
            lines.push_back (std::format ("-- {} --", CallStack::DescribeBreak (*row.chainBreak)));
        }
        else if (row.frame.has_value())
        {
            lines.push_back (FormatCallFrame (*row.frame));
        }
    }

    if (data.lastReturn.has_value())
    {
        lines.push_back ("Last return: " + FormatCallFrame (*data.lastReturn));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCallFrame
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatCallFrame (const CallStackFrame & frame)
{
    std::string  line = std::format ("${:04X}  {} ${:04X} {:<12} {}",
                                     frame.callSite, CallStack::GetKindName (frame.kind), frame.target, frame.symbol,
                                     frame.provenance == CallProvenance::Recorded ? "recorded" : "guessed");



    if (!frame.isVerified)
    {
        line += ", unverified";
    }

    if (!frame.note.empty())
    {
        line += ", " + frame.note;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatBasicVariables
//
//  A variable a line, as a program would assign it. An array is its name
//  with its DIM subscripts, then its first elements, the first
//  subscript varying fastest, and how many more there are.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatBasicVariables (const BasicVariablesData & data, Lines & lines)
{
    static constexpr size_t  kShown = 16;



    if (data.variables.empty() && data.arrays.empty())
    {
        lines.push_back ("No BASIC variables.");
        return;
    }

    for (const BasicVariable & variable : data.variables)
    {
        lines.push_back (std::format ("{} = {}", variable.name, (variable.type == BasicType::String) ? "\"" + variable.value + "\"" : variable.value));
    }

    for (const BasicArray & array : data.arrays)
    {
        std::string  dims;
        std::string  values;



        for (int size : array.sizes)
        {
            dims += std::format ("{}{}", dims.empty() ? "" : ",", size - 1);
        }

        for (size_t i = 0; i < array.values.size() && i < kShown; i++)
        {
            values += (i == 0 ? "" : ", ") + ((array.type == BasicType::String) ? "\"" + array.values[i] + "\"" : array.values[i]);
        }

        if (array.values.size() > kShown)
        {
            values += std::format (", and {} more", array.values.size() - kShown);
        }

        lines.push_back (std::format ("{}({}) = {}", array.name, dims, values));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatBasicStack
//
//  Newest first: a GOSUB with the line it was made on, a FOR loop with its
//  variable, limit, step and line.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatBasicStack (const BasicStackData & data, Lines & lines)
{
    if (data.frames.empty())
    {
        lines.push_back ("No GOSUB or FOR is active.");
        return;
    }

    for (const BasicFrame & frame : data.frames)
    {
        lines.push_back ((frame.kind == BasicFrameKind::Gosub)
                         ? std::format ("GOSUB in line {}", frame.line)
                         : std::format ("FOR {} TO {} STEP {} in line {}", frame.variable, frame.limit, frame.step, frame.line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatProfile
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatProfile (const ProfileData & data, Lines & lines)
{
    lines.push_back (std::format ("Instructions: {}, cycles: {}", data.instructions, data.cycles));

    if (data.instructions == 0)
    {
        return;
    }

    switch (data.list)
    {
    case ProfileList::Addresses: FormatProfileAddresses (data, lines); break;
    case ProfileList::Routines:  FormatProfileRoutines  (data, lines); break;
    case ProfileList::Tree:      FormatProfileTree      (data, lines); break;
    default:                     FormatProfileOpcodes   (data, lines); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatProfileOpcodes
//
//  Count, base cycles and share per mnemonic and addressing mode, then the
//  avoidable cycles by kind. Each share is of the total, penalties included,
//  so the rows add up to 100%.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatProfileOpcodes (const ProfileData & data, Lines & lines)
{
    lines.push_back (std::format ("{:<6} {:<20} {:>10} {:>10} {:>7}", "Opcode", "Mode", "Count", "Cycles", "Percent"));

    for (const ProfileEntry & entry : data.opcodes)
    {
        lines.push_back (std::format ("{:<6} {:<20} {:>10} {:>10} {:>6.1f}%", entry.mnemonic, entry.mode, entry.count, entry.cycles, GetShare (entry.cycles, data.cycles)));
    }

    lines.push_back (std::format ("{:<38} {:>10} {:>7}", "Penalty", "Cycles", "Percent"));
    lines.push_back (std::format ("{:<38} {:>10} {:>6.1f}%", "Page crossing",            data.pageCross,   GetShare (data.pageCross,   data.cycles)));
    lines.push_back (std::format ("{:<38} {:>10} {:>6.1f}%", "Taken branches",           data.branchTaken, GetShare (data.branchTaken, data.cycles)));
    lines.push_back (std::format ("{:<38} {:>10} {:>6.1f}%", "Branches crossing a page", data.branchCross, GetShare (data.branchCross, data.cycles)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatProfileAddresses
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatProfileAddresses (const ProfileData & data, Lines & lines)
{
    lines.push_back (std::format ("{:<7} {:<20} {:>10} {:>7}", "Address", "Symbol", "Cycles", "Percent"));

    for (const ProfileAddressEntry & entry : data.addresses)
    {
        lines.push_back (std::format ("{:<7} {:<20} {:>10} {:>6.1f}%", std::format ("${:04X}", entry.address), entry.symbol, entry.cycles, GetShare (entry.cycles, data.cycles)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatProfileRoutines
//
//  Each routine's own cycles and its total, each with its share of every
//  cycle counted, its calls and its total per call. Top level is never
//  called, so it has no cycles per call.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatProfileRoutines (const ProfileData & data, Lines & lines)
{
    lines.push_back (std::format ("{:<24} {:>10} {:>7} {:>10} {:>7} {:>8} {:>8}", "Routine", "Self", "Percent", "Total", "Percent", "Calls", "Per call"));

    for (const ProfileRoutineEntry & entry : data.routines)
    {
        std::string  perCall = (entry.calls != 0) ? std::to_string (entry.total / entry.calls) : std::string ("-");



        lines.push_back (std::format ("{:<24} {:>10} {:>6.1f}% {:>10} {:>6.1f}% {:>8} {:>8}", entry.GetName(), entry.self, GetShare (entry.self, data.cycles),
                                      entry.total, GetShare (entry.total, data.cycles), entry.calls, perCall));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatProfileTree
//
//  Each routine under its caller, two spaces in for each level.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatProfileTree (const ProfileData & data, Lines & lines)
{
    lines.push_back (std::format ("{:<40} {:>10} {:>10} {:>7} {:>8}", "Routine", "Self", "Total", "Percent", "Calls"));

    for (const ProfileTreeEntry & entry : data.tree)
    {
        std::string  name = std::string ((size_t) entry.depth * 2, ' ') + data.routines[entry.routine].GetName();



        lines.push_back (std::format ("{:<40} {:>10} {:>10} {:>6.1f}% {:>8}", name, entry.self, entry.total, GetShare (entry.total, data.cycles), entry.calls));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::GetShare
//
////////////////////////////////////////////////////////////////////////////////

double AppleWinFormatter::GetShare (uint64_t part, uint64_t total)
{
    static constexpr double  kPercent = 100.0;



    return total == 0 ? 0.0 : (double) part * kPercent / (double) total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCalc
//
//  Hex, binary of the low byte, decimal and the character, with a note where
//  the byte has its high bit set or is a control character.
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatCalc (const CalcData & data, Lines & lines)
{
    static constexpr Byte  kLowBits    = 0x7F;
    static constexpr Byte  kHighBit    = 0x80;
    static constexpr Byte  kFirstPrint = 0x20;
    static constexpr Byte  kDelete     = 0x7F;
    Byte                   low         = (Byte) data.value;
    Byte                   ch          = (Byte) (low & kLowBits);
    bool                   isHigh      = (low & kHighBit) != 0;
    bool                   isControl   = ch < kFirstPrint || ch == kDelete;
    std::string            text;



    text = std::format ("${:04X}  0z{:08b}  {:5}  '{}'", data.value, low, data.value, isControl ? ' ' : (char) ch);

    if (isHigh && isControl)      { text += " (High Ctrl)"; }
    else if (isHigh)              { text += " (High)"; }
    else if (isControl)           { text += " (Ctrl)"; }

    lines.push_back (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::DescribeBreakpoint
//
//  The kind and place, then any flag that is off its default.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::DescribeBreakpoint (const BreakpointInfo & breakpoint)
{
    static constexpr const char * kAccess[] = { "read", "write", "read or write" };
    std::string                   text;



    switch (breakpoint.kind)
    {
    case BreakpointKind::Opcode:    text = std::format ("on opcode ${:02X}", breakpoint.opcode);                                  break;
    case BreakpointKind::Register:  text = "when " + breakpoint.condition;                                                       break;
    case BreakpointKind::Memory:    text = std::format ("on {} of {}{}", kAccess[(int) breakpoint.access], FormatAddress (breakpoint),
                                                        breakpoint.mode == WatchMode::Before ? ", before the access" : "");     break;
    case BreakpointKind::Brk:       text = "on BRK";                                                                              break;
    case BreakpointKind::Interrupt: text = "on interrupt";                                                                        break;
    case BreakpointKind::MemoryValue:
        text = std::format ("when ${:04X} becomes ${:02X}", breakpoint.address, breakpoint.value.value_or (0));
        break;
    case BreakpointKind::Disk:
        text = "on disk " + DescribeDisk (breakpoint);
        break;
    case BreakpointKind::BasicLine:
        text = std::format ("on BASIC line {}", breakpoint.address);
        break;
    case BreakpointKind::BasicVariable:
        text = "on BASIC variable " + breakpoint.basicName + " changing";
        break;
    case BreakpointKind::BasicError:
        text = "on BASIC error";
        break;
    default:                        text = "at " + FormatAddress (breakpoint);                                                    break;
    }

    if (breakpoint.kind != BreakpointKind::Register && !breakpoint.condition.empty())
    {
        text += " if " + breakpoint.condition;
    }

    if (breakpoint.temporary)
    {
        text += ", temporary";
    }

    if (!breakpoint.stops)
    {
        text += ", counts only";
    }

    return text;
}
