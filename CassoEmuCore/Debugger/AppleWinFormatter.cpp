#include "Pch.h"

#include "Debugger/AppleWinFormatter.h"

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
    else if (auto * v = std::get_if<CyclesData>         (&data)) { lines.push_back (std::format ("Cycles: {}", v->count)); }
    else if (auto * v = std::get_if<ModeData>           (&data)) { lines.push_back (v->mode == CommandMode::Monitor ? "Mode: MONITOR" : "Mode: APPLEWIN"); }
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



    for (const DisassemblyLine & line : data.lines)
    {
        std::string  bytes;
        std::string  text;



        for (Byte b : line.instruction.bytes)
        {
            bytes += std::format ("{:02X} ", b);
        }

        bytes.resize (std::max (bytes.size(), kBytesColumn), ' ');
        text = std::format ("{:04X}: {}{:<4} {}", line.instruction.address, bytes, line.instruction.mnemonic, line.instruction.operand);

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        if (!line.symbol.empty())
        {
            text += "  ; " + line.symbol;
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
            text += std::format (" = ${:02X}", *entry.value);
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
//  AppleWinFormatter::DescribeBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinFormatter::DescribeBreakpoint (const BreakpointInfo & breakpoint)
{
    static constexpr const char * kAccess[] = { "read", "write", "read or write" };



    switch (breakpoint.kind)
    {
    case BreakpointKind::Opcode:    return std::format ("on opcode ${:02X}", breakpoint.opcode);
    case BreakpointKind::Register:  return "when " + breakpoint.condition;
    case BreakpointKind::Memory:    return std::format ("on {} of {}", kAccess[(int) breakpoint.access], FormatAddress (breakpoint));
    case BreakpointKind::Io:        return "on I/O at " + FormatAddress (breakpoint);
    case BreakpointKind::Brk:       return "on BRK";
    case BreakpointKind::Interrupt: return "on interrupt";
    default:                        return "at " + FormatAddress (breakpoint);
    }
}
