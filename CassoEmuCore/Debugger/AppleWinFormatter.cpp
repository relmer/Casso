#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"

#include "Debugger/CommandModeNames.h"
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
//  before-mode stop has no value yet. Other stops give the reason and PC.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::FormatStop (const StopEvent & stop)
{
    static constexpr const char * kReasons[] = { "Breakpoint", "Watchpoint", "Step", "Run to", "Budget", "Pause", "BRK", "Invalid opcode", "Reset" };
    std::string                   text;



    if (stop.reason == StopReason::Watchpoint && stop.watch.has_value())
    {
        const WatchHit & hit = *stop.watch;



        if (hit.mode == WatchMode::Before)
        {
            text = std::format ("Watchpoint #{}: {} of ${:04X} by ${:04X}, before the access",
                                hit.id, hit.access == WatchAccess::Read ? "read" : "write", hit.address, hit.accessPc);
        }
        else
        {
            text = std::format ("Watchpoint #{}: {} ${:02X} {} ${:04X} by ${:04X}",
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

        return text + FormatCondition (stop);
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

    return std::format (", IF {} is ${:X}", stop.condition, (uint32_t) *stop.conditionValue);
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
    else if (auto * v = std::get_if<CalcData>           (&data)) { FormatCalc           (*v, lines); }
    else if (auto * v = std::get_if<StepFilterData>     (&data)) { FormatStepFilter     (*v, lines); }
    else if (auto * v = std::get_if<MessageData>        (&data)) { lines.insert (lines.end(), v->lines.begin(), v->lines.end()); }
    else if (auto * v = std::get_if<CyclesData>         (&data)) { lines.push_back (std::format ("Cycles: {}", v->count)); }
    else if (auto * v = std::get_if<ModeData>           (&data)) { lines.push_back ("Mode: " + CommandModeNames::GetUpperName (v->mode)); }
    else if (auto * v = std::get_if<VideoInfoData>      (&data)) { lines.push_back (std::format ("Scanline {}, cycle {}", v->scanline, v->cycleInLine)); }
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
    lines.push_back (std::format ("Breakpoint #{} set {}", data.breakpoint.id, DescribeBreakpoint (data.breakpoint)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatBreakpointList
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatBreakpointList (const BreakpointListData & data, Lines & lines)
{
    if (data.breakpoints.empty())
    {
        lines.push_back ("No breakpoints.");
        return;
    }

    for (const BreakpointInfo & breakpoint : data.breakpoints)
    {
        lines.push_back (std::format ("#{} {} {}, hits {}",
                                      breakpoint.id,
                                      breakpoint.enabled ? "enabled " : "disabled",
                                      DescribeBreakpoint (breakpoint),
                                      breakpoint.hits));
    }
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
        std::string  text = std::format ("#{} ${:04X}", entry.id, entry.address);



        if (entry.value.has_value() && data.kind == WatchListKind::ZeroPage)
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
        lines.push_back (std::format ("${:04X} {} ({})", symbol.address, symbol.name, ReplyJson::GetSymbolTableName (symbol.table)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatFileIo
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatFileIo (const FileIoData & data, Lines & lines)
{
    std::string  text = std::format ("{}: {} of {} bytes", data.path, data.transferred, data.requested);



    if (data.mismatch)
    {
        text += "; the file and the range differ in size";
    }

    lines.push_back (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter::FormatCompare
//
////////////////////////////////////////////////////////////////////////////////

void AppleWinFormatter::FormatCompare (const CompareData & data, Lines & lines)
{
    lines.push_back (std::format ("Compared {} bytes, {} differ.", data.compared, data.differences.size()));

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

    if (data.isByAddress)
    {
        FormatProfileAddresses (data, lines);
        return;
    }

    FormatProfileOpcodes (data, lines);
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
    case BreakpointKind::Io:        text = "on I/O at " + FormatAddress (breakpoint);                                             break;
    case BreakpointKind::Brk:       text = "on BRK";                                                                              break;
    case BreakpointKind::Interrupt: text = "on interrupt";                                                                        break;
    case BreakpointKind::MemoryValue:
        text = std::format ("when ${:04X} becomes ${:02X}", breakpoint.address, breakpoint.value.value_or (0));
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
