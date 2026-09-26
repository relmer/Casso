#include "Pch.h"

#include "Debugger/GSSquaredFormatter.h"

#include "Debugger/AppleWinFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::Format
//
//  GSSquared's own layouts where it has one, and the AppleWin text
//  otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::Format (Reply & reply)
{
    if (reply.status != CommandStatus::Ok || !TryFormatData (reply, reply.text))
    {
        AppleWinFormatter::Format (reply);
        return;
    }

    TrimTrailingSpaces (reply.text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::TryFormatData
//
//  A deposit prints nothing, as GSSquared's does, though the reply carries
//  the rows written. A watch that was just set is reported by its id, and a
//  list by its entries.
//
////////////////////////////////////////////////////////////////////////////////

bool GSSquaredFormatter::TryFormatData (const Reply & reply, Lines & lines)
{
    const ReplyData  & data = reply.data;



    if (reply.verb == DebugVerb::EnterBytes)
    {
        return true;
    }

    if      (auto * v = std::get_if<MemoryData>         (&data)) { FormatMemory         (*v, lines); }
    else if (auto * v = std::get_if<DisassemblyData>    (&data)) { FormatDisassembly    (*v, lines); }
    else if (auto * v = std::get_if<BreakpointListData> (&data)) { FormatBreakpointList (*v, lines); }
    else if (auto * v = std::get_if<SymbolData>         (&data)) { FormatSymbols        (*v, lines); }
    else if (auto * v = std::get_if<BreakpointSetData>  (&data)) { lines.push_back (std::format ("Breakpoint id={} set", v->breakpoint.id)); }
    else if (auto * v = std::get_if<WatchListData>      (&data); v != nullptr && v->kind == WatchListKind::Watch)
    {
        FormatWatchList (*v, reply.verb == DebugVerb::AddWatch, lines);
    }
    else if (auto * v = std::get_if<FileIoData>         (&data); v != nullptr && reply.verb == DebugVerb::SaveBinary)
    {
        lines.push_back (std::format ("Saved {} bytes to {}", v->transferred, v->path));
    }
    else if (reply.verb == DebugVerb::ClearSymbols)
    {
        lines.push_back ("Cleared symbol table");
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatMemory
//
//  One byte is GSSquared's examine, `00/0300: A9`. More is its dump: sixteen
//  bytes a line from the first address, each byte followed by a space, then
//  a space and the characters with the high bit dropped and anything outside
//  printable ASCII as a period, then a blank line. A short last line is
//  padded so its characters line up with the lines above.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::FormatMemory (const MemoryData & data, Lines & lines)
{
    static constexpr Byte             kLowBits    = 0x7F;
    static constexpr Byte             kFirstPrint = 0x20;
    static constexpr Byte             kLastPrint  = 0x7E;
    std::vector<std::optional<Byte>>  bytes;
    Word                              first       = data.rows.empty() ? 0 : data.rows.front().address;



    for (const MemoryRow & row : data.rows)
    {
        bytes.insert (bytes.end(), row.bytes.begin(), row.bytes.end());
    }

    if (bytes.size() == 1)
    {
        lines.push_back (FormatAddress (first) + (bytes[0].has_value() ? std::format (": {:02X}", *bytes[0]) : std::string (": ??")));
        return;
    }

    for (size_t start = 0; start < bytes.size(); start += kBytesPerLine)
    {
        std::string  hex  = FormatAddress ((Word) (first + start)) + ": ";
        std::string  text;
        Byte         low  = 0;



        for (size_t i = start; i < start + kBytesPerLine; ++i)
        {
            if (i >= bytes.size())
            {
                hex  += "   ";
                text += ' ';
                continue;
            }

            low   = bytes[i].has_value() ? (Byte) (*bytes[i] & kLowBits) : 0;
            hex  += bytes[i].has_value() ? std::format ("{:02X} ", *bytes[i]) : std::string ("?? ");
            text += (bytes[i].has_value() && low >= kFirstPrint && low <= kLastPrint) ? (char) low : '.';
        }

        lines.push_back (hex + " " + text);
    }

    lines.push_back (std::string());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatDisassembly
//
//  GSSquared's `list` prints the register widths it assumed, which on a 6502
//  are always 8, then one line per instruction: `0300: A9 41` with the bytes
//  in a field wide enough for four, the mnemonic, and the operand. GSSquared
//  shows no labels in this listing.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::FormatDisassembly (const DisassemblyData & data, Lines & lines)
{
    lines.push_back ("M=8 X=8");

    for (const DisassemblyLine & line : data.lines)
    {
        std::string  text = std::format ("{:04X}: ", line.instruction.address);



        for (Byte value : line.instruction.bytes)
        {
            text += std::format ("{:02X} ", value);
        }

        text.resize (std::max (text.size(), kMnemonicColumn), ' ');
        text += line.instruction.mnemonic;
        text.resize (std::max (text.size(), kOperandColumn), ' ');
        text += line.instruction.operand;

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatBreakpointList
//
//  `[id] exec 00/0300`, `[id] exec 00/0300.030F`, `[id] data 00/C019 r`. A
//  kind GSSquared has no word for is described as AppleWin describes it.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::FormatBreakpointList (const BreakpointListData & data, Lines & lines)
{
    static constexpr const char * kAccess[] = { "r", "w", "rw" };



    lines.push_back ("Current breakpoints:");

    for (const BreakpointInfo & breakpoint : data.breakpoints)
    {
        std::string  place = FormatAddress (breakpoint.address);



        if (breakpoint.last > breakpoint.address)
        {
            place += std::format (".{:04X}", breakpoint.last);
        }

        switch (breakpoint.kind)
        {
        case BreakpointKind::Address:
            lines.push_back (std::format ("[{}] exec {}", breakpoint.id, place));
            break;

        case BreakpointKind::Memory:
            lines.push_back (std::format ("[{}] data {} {}", breakpoint.id, place, kAccess[(int) breakpoint.access]));
            break;

        case BreakpointKind::Io:
            lines.push_back (std::format ("[{}] io {} rw", breakpoint.id, place));
            break;

        default:
            lines.push_back (std::format ("[{}] {}", breakpoint.id, AppleWinFormatter::DescribeBreakpoint (breakpoint)));
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatWatchList
//
//  `Watch id=N set` for each watch just added, or the list: `[id] 00/0006`.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::FormatWatchList (const WatchListData & data, bool isAdd, Lines & lines)
{
    if (isAdd)
    {
        for (const WatchEntry & entry : data.entries)
        {
            lines.push_back (std::format ("Watch id={} set", entry.id));
        }

        return;
    }

    lines.push_back ("Current memory watches:");

    for (const WatchEntry & entry : data.entries)
    {
        lines.push_back (std::format ("[{}] {}", entry.id, FormatAddress (entry.address)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatSymbols
//
//  `slookup`: `00/0300: NAME`, one line per name at the address, with
//  `(constant)` after the name of an equate.
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::FormatSymbols (const SymbolData & data, Lines & lines)
{
    for (const SymbolInfo & symbol : data.symbols)
    {
        lines.push_back (std::format ("{}: {}{}", FormatAddress (symbol.address), symbol.name, symbol.isConstant ? " (constant)" : ""));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::FormatAddress
//
////////////////////////////////////////////////////////////////////////////////

std::string GSSquaredFormatter::FormatAddress (Word address)
{
    return std::format ("00/{:04X}", address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter::TrimTrailingSpaces
//
////////////////////////////////////////////////////////////////////////////////

void GSSquaredFormatter::TrimTrailingSpaces (Lines & lines)
{
    for (std::string & line : lines)
    {
        while (!line.empty() && line.back() == ' ')
        {
            line.pop_back();
        }
    }
}
