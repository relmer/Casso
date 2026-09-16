#include "Pch.h"

#include "Debugger/MonitorFormatter.h"

#include "Debugger/AppleWinFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::Format
//
//  The Monitor's own layouts where it has one, and the AppleWin text
//  otherwise -- which is what a reader who typed `/` asked for.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::Format (Reply & reply)
{
    if (reply.status != CommandStatus::Ok)
    {
        FormatError (reply);
        return;
    }

    if (!TryFormatData (reply.data, reply.text))
    {
        AppleWinFormatter::Format (reply);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::TryFormatData
//
////////////////////////////////////////////////////////////////////////////////

bool MonitorFormatter::TryFormatData (const ReplyData & data, Lines & lines)
{
    if      (auto * v = std::get_if<MemoryData>      (&data)) { FormatMemory      (*v, lines); }
    else if (auto * v = std::get_if<DisassemblyData> (&data)) { FormatDisassembly (*v, lines); }
    else if (auto * v = std::get_if<CompareData>     (&data)) { FormatCompare     (*v, lines); }
    else if (auto * v = std::get_if<SearchHitsData>  (&data)) { FormatSearchHits  (*v, lines); }
    else if (auto * v = std::get_if<RegistersData>   (&data)) { lines.push_back (FormatRegisters (v->registers)); }
    else if (auto * v = std::get_if<CalcData>        (&data)) { lines.push_back (std::format ("={:02X}", v->value & 0xFF)); }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatMemory
//
//  `0300- A9 00 8D 00 03 60`, with no character column: the Monitor never
//  printed one.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::FormatMemory (const MemoryData & data, Lines & lines)
{
    for (const MemoryRow & row : data.rows)
    {
        std::string  text = std::format ("{:04X}-", row.address);



        for (const std::optional<Byte> & cell : row.bytes)
        {
            text += cell.has_value() ? std::format (" {:02X}", *cell) : std::string (" ??");
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatDisassembly
//
//  `0300-   A9 00       LDA   #$00`. The byte field is a fixed width, so a
//  one-byte instruction and a three-byte one keep the mnemonic in the same
//  column; an instruction with no operand loses its trailing spaces rather
//  than carrying them to the end of the line.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::FormatDisassembly (const DisassemblyData & data, Lines & lines)
{
    for (const DisassemblyLine & line : data.lines)
    {
        std::string  bytes;
        std::string  text;



        for (Byte value : line.instruction.bytes)
        {
            bytes += (bytes.empty() ? "" : " ") + std::format ("{:02X}", value);
        }

        bytes.resize (std::max (bytes.size(), kBytesColumn), ' ');

        text = std::format ("{:04X}-   {}{}   {}",
                            line.instruction.address, bytes, line.instruction.mnemonic, line.instruction.operand);

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatCompare
//
//  One line per difference, `0303-41 (42)`, the source byte in parentheses.
//  A verify that found nothing says nothing, which is how the Monitor
//  reports two ranges that match.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::FormatCompare (const CompareData & data, Lines & lines)
{
    for (const CompareDifference & difference : data.differences)
    {
        lines.push_back (std::format ("{:04X}-{:02X} ({:02X})",
                                      difference.address, difference.value, difference.otherValue));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatSearchHits
//
//  The addresses alone, one per line.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::FormatSearchHits (const SearchHitsData & data, Lines & lines)
{
    for (Word address : data.addresses)
    {
        lines.push_back (std::format ("{:04X}", address));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatRegisters
//
//  `A=00 X=00 Y=00 P=30 S=FF`, with no program counter: the Monitor's line
//  never carried one.
//
////////////////////////////////////////////////////////////////////////////////

std::string MonitorFormatter::FormatRegisters (const Cpu6502Registers & registers)
{
    return std::format ("A={:02X} X={:02X} Y={:02X} P={:02X} S={:02X}",
                        registers.a, registers.x, registers.y, registers.p, registers.sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatStop
//
////////////////////////////////////////////////////////////////////////////////

std::string MonitorFormatter::FormatStop (const StopEvent & stop)
{
    return FormatRegisters (stop.registers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter::FormatError
//
//  THE MONITOR'S `ERR` AND THE REASON BOTH.
//
//  A script reading Monitor output looks for ERR, which is all the real
//  Monitor ever gave it. A person needs to know which error it was, and the
//  ROM never told them. Printing both costs a line and leaves neither reader
//  worse off.
//
////////////////////////////////////////////////////////////////////////////////

void MonitorFormatter::FormatError (Reply & reply)
{
    reply.text.push_back ("ERR");
    reply.text.push_back ("Error: " + reply.error.label);

    if (!reply.error.detail.empty())
    {
        reply.text.push_back (std::string (kDetailIndent, ' ') + reply.error.detail);
    }
}
