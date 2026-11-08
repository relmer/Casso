#include "Pch.h"

#include "Debugger/WinDbgFormatter.h"

#include "Debugger/AppleWinFormatter.h"
#include "Debugger/CallStack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::Format
//
//  Text a handler already wrote is kept, and data adds its lines after it,
//  as in AppleWin mode.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::Format (Reply & reply)
{
    std::string  name = GetCommandName (reply.command);



    if (reply.status != CommandStatus::Ok || !TryFormatData (reply.command, name, reply.data, reply.text))
    {
        AppleWinFormatter::Format (reply);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatStop
//
//  WinDbg's "Break instruction exception" means nothing here; the reason is
//  AppleWin mode's, and the registers follow as r prints them.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::FormatStop (const StopEvent & stop)
{
    return AppleWinFormatter::FormatStop (stop) + "\n" + FormatRegisters (stop.registers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatRegisters
//
//  `a=41 x=00 y=00 sp=f9 pc=0300 nv-bdizc`.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::FormatRegisters (const Cpu6502Registers & registers)
{
    return std::format ("a={:02x} x={:02x} y={:02x} sp={:02x} pc={:04x} {}",
                        registers.a, registers.x, registers.y, registers.sp, registers.pc, FormatFlags (registers.p));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatRegister
//
//  `r a` alone: `a=41`, under the name it was asked for, which may be
//  written @a. The flags show as a byte, `fl=34`. A word that is not one
//  register gets the whole line.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::FormatRegister (const std::string & word, const Cpu6502Registers & registers)
{
    std::string  name = ToLower (word.starts_with ('@') ? word.substr (1) : word);



    if (name == "a")                 { return std::format ("a={:02x}",     registers.a);  }
    if (name == "x")                 { return std::format ("x={:02x}",     registers.x);  }
    if (name == "y")                 { return std::format ("y={:02x}",     registers.y);  }
    if (name == "sp")                { return std::format ("sp={:02x}",    registers.sp); }
    if (name == "pc")                { return std::format ("pc={:04x}",    registers.pc); }
    if (name == "fl" || name == "p") { return std::format ("{}={:02x}", name, registers.p); }

    return FormatRegisters (registers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatFlags
//
//  N V - B D I Z C, lowercase where clear and uppercase where set, as WinDbg
//  shows its flags. The unused bit is always a dash.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::FormatFlags (Byte p)
{
    static constexpr const char  kLetters[]  = "nv-bdizc";
    static constexpr int         kFlagCount  = 8;
    static constexpr Byte        kHighBit    = 0x80;
    std::string                  flags;



    for (int i = 0; i < kFlagCount; ++i)
    {
        bool  isSet = (p & (kHighBit >> i)) != 0;



        flags += (isSet && kLetters[i] != '-') ? (char) toupper ((unsigned char) kLetters[i]) : kLetters[i];
    }

    return flags;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::TryFormatData
//
//  False for a kind with no WinDbg layout. `r a` shows one register, and
//  `bl 1 3` the breakpoints it gives, from the whole set the reply holds.
//  bd and be show the entry they changed as bl lists it, so the e or d
//  reads back; bp and ba keep AppleWin's line, which gives the new id.
//
////////////////////////////////////////////////////////////////////////////////

bool WinDbgFormatter::TryFormatData (const std::string & command, const std::string & name, const ReplyData & data, Lines & lines)
{
    static constexpr size_t  kWord       = 2;
    static constexpr size_t  kDoubleWord = 4;
    std::string              words       = GetArguments (command);



    if (auto * v = std::get_if<RegistersData> (&data))
    {
        lines.push_back ((name == "r" && !words.empty()) ? FormatRegister (words, v->registers) : FormatRegisters (v->registers));
    }
    else if (auto * v = std::get_if<MemoryData> (&data))
    {
        if      (name == "dw") { FormatUnits  (*v, kWord,       lines); }
        else if (name == "dd") { FormatUnits  (*v, kDoubleWord, lines); }
        else if (name == "da") { FormatString (*v, lines); }
        else                   { FormatBytes  (*v, lines); }
    }
    else if (auto * v = std::get_if<BreakpointSetData> (&data); v != nullptr && (name == "bd" || name == "be"))
    {
        FormatBreakpoints (BreakpointListData { { v->breakpoint } }, std::string(), lines);
    }
    else if (auto * v = std::get_if<DisassemblyData>    (&data)) { FormatDisassembly (*v, lines); }
    else if (auto * v = std::get_if<BreakpointListData> (&data)) { FormatBreakpoints (*v, name == "bl" ? words : std::string(), lines); }
    else if (auto * v = std::get_if<CallStackData>      (&data)) { FormatCallStack   (*v, lines); }
    else if (auto * v = std::get_if<CalcData>           (&data))
    {
        if (name == ".formats") { FormatFormats    (*v, lines); }
        else                    { FormatEvaluation (*v, lines); }
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatBytes
//
//  db: sixteen bytes a line from the address asked for, a dash between the
//  eighth and ninth, then the characters. A byte outside printable ASCII is
//  a period, as WinDbg shows it; an unreadable one is ?? and a question mark.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatBytes (const MemoryData & data, Lines & lines)
{
    static constexpr size_t           kHalf       = 8;
    static constexpr size_t           kHexWidth   = kBytesPerLine * 3 - 1;
    static constexpr Byte             kFirstPrint = 0x20;
    static constexpr Byte             kLastPrint  = 0x7E;
    std::vector<std::optional<Byte>>  bytes       = GetBytes (data);
    Word                              address     = data.rows.empty() ? 0 : data.rows.front().address;



    for (size_t line = 0; line < bytes.size(); line += kBytesPerLine)
    {
        std::string  hex;
        std::string  ascii;



        for (size_t i = line; i < bytes.size() && i < line + kBytesPerLine; i++)
        {
            const std::optional<Byte> & cell = bytes[i];



            hex   += (i == line) ? "" : (i - line == kHalf) ? "-" : " ";
            hex   += cell.has_value() ? std::format ("{:02x}", *cell) : std::string ("??");
            ascii += !cell.has_value()                            ? '?'
                   : (*cell >= kFirstPrint && *cell <= kLastPrint) ? (char) *cell
                   :                                                 '.';
        }

        hex.resize (kHexWidth, ' ');
        lines.push_back (std::format ("{:04x}  {}  {}", (Word) (address + line), hex, ascii));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatUnits
//
//  dw and dd: little-endian words or double words, sixteen bytes a line. A
//  unit with an unreadable byte is question marks, and so is a last unit the
//  range ends inside, as at $FFFF.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatUnits (const MemoryData & data, size_t unit, Lines & lines)
{
    static constexpr int              kBitsPerByte = 8;
    std::vector<std::optional<Byte>>  bytes        = GetBytes (data);
    Word                              address      = data.rows.empty() ? 0 : data.rows.front().address;



    for (size_t line = 0; line < bytes.size(); line += kBytesPerLine)
    {
        std::string  text = std::format ("{:04x} ", (Word) (address + line));



        for (size_t at = line; at < line + kBytesPerLine && at < bytes.size(); at += unit)
        {
            uint32_t  value    = 0;
            bool      isKnown  = true;



            for (size_t i = 0; i < unit; i++)
            {
                bool  isPresent = at + i < bytes.size() && bytes[at + i].has_value();



                isKnown = isKnown && isPresent;
                value  |= isPresent ? (uint32_t) *bytes[at + i] << (i * kBitsPerByte) : 0;
            }

            text += " " + (isKnown ? std::format ("{:0{}x}", value, unit * 2) : std::string (unit * 2, '?'));
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatString
//
//  da: the characters up to the first zero byte, quoted. Apple II text sets
//  the high bit, so it is cleared before a character is judged printable.
//  An empty range shows nothing, having no address to show.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatString (const MemoryData & data, Lines & lines)
{
    static constexpr Byte             kLowBits    = 0x7F;
    static constexpr Byte             kFirstPrint = 0x20;
    static constexpr Byte             kLastPrint  = 0x7E;
    std::vector<std::optional<Byte>>  bytes       = GetBytes (data);
    Word                              address     = data.rows.empty() ? 0 : data.rows.front().address;
    std::string                       text;



    if (data.rows.empty())
    {
        return;
    }

    for (const std::optional<Byte> & cell : bytes)
    {
        Byte  ch = cell.has_value() ? (Byte) (*cell & kLowBits) : 0;



        if (!cell.has_value() || *cell == 0)
        {
            break;
        }

        text += (ch >= kFirstPrint && ch <= kLastPrint) ? (char) ch : '.';
    }

    lines.push_back (std::format ("{:04x}  \"{}\"", address, text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatDisassembly
//
//  u: a label on a line of its own, then address, bytes and instruction.
//  The byte column fits the widest line, which is a data line's when the
//  listing holds one. An operand is lowercased as WinDbg shows one, except a
//  symbol and quoted text, which keep their case.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatDisassembly (const DisassemblyData & data, Lines & lines)
{
    static constexpr size_t  kBytesWidth = 6;
    size_t                   width       = kBytesWidth;



    for (const DisassemblyLine & line : data.lines)
    {
        width = std::max (width, line.instruction.bytes.size() * 2);
    }

    for (const DisassemblyLine & line : data.lines)
    {
        std::string  bytes;
        std::string  mnemonic = line.instruction.mnemonic;
        std::string  operand  = line.GetShownOperand();
        std::string  text;
        char         quote    = '\0';



        if (!line.label.empty())
        {
            lines.push_back (line.label + ":");
        }

        for (Byte b : line.instruction.bytes)
        {
            bytes += std::format ("{:02x}", b);
        }

        for (char & ch : mnemonic)
        {
            ch = (char) tolower ((unsigned char) ch);
        }

        for (char & ch : operand)
        {
            if (quote == '\0' && (ch == '"' || ch == '\''))
            {
                quote = ch;
            }
            else if (ch == quote)
            {
                quote = '\0';
            }
            else if (quote == '\0' && line.operandSymbol.empty())
            {
                ch = (char) tolower ((unsigned char) ch);
            }
        }

        text = std::format ("{:04x} {:<{}}  {} {}", line.instruction.address, bytes, width, mnemonic, operand);

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatBreakpoints
//
//  bl: id, e or d, where the entry stops, and WinDbg's pass count, which is
//  always one: a Casso breakpoint stops on its first hit. Where is the
//  address, with an access watchpoint's kind and size after it, or for the
//  kinds with no address, what they stop on. A condition follows, then the
//  hits and any setting off its default, as BPL gives them. ids limits the
//  list to those entries; empty or * lists them all.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatBreakpoints (const BreakpointListData & data, const std::string & ids, Lines & lines)
{
    std::istringstream        stream (ids);
    std::set<std::string>     wanted;
    std::string               id;



    while (stream >> id)
    {
        id.erase (0, std::min (id.find_first_not_of ('0'), id.size() - 1));
        wanted.insert (id);
    }

    for (const BreakpointInfo & breakpoint : data.breakpoints)
    {
        std::string                text   = std::format ("{} {} {} 0001 (0001)", breakpoint.id, breakpoint.enabled ? "e" : "d", GetBreakpointPlace (breakpoint));
        std::vector<std::string>   extras;



        if (!wanted.empty() && !wanted.contains ("*") && !wanted.contains (std::to_string (breakpoint.id)))
        {
            continue;
        }

        if (!breakpoint.condition.empty() && breakpoint.kind != BreakpointKind::Register)
        {
            text += "  IF " + breakpoint.condition;
        }

        if (breakpoint.hits > 0)                     { extras.push_back (std::format ("hits {}", breakpoint.hits)); }
        if (breakpoint.temporary)                    { extras.push_back ("temporary");                             }
        if (!breakpoint.stops)                       { extras.push_back ("counts only");                           }
        if (breakpoint.mode == WatchMode::Before)    { extras.push_back ("before the access");                     }

        for (size_t i = 0; i < extras.size(); i++)
        {
            text += (i == 0 ? "  " : ", ") + extras[i];
        }

        lines.push_back (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::GetBreakpointPlace
//
//  An address or a range; an access watchpoint's with `r`, `w` or `rw` and
//  its size in bytes; and for the kinds with no address, the opcode, the
//  register comparison, BRK or an interrupt, and a value breakpoint's
//  address and the value it waits for.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::GetBreakpointPlace (const BreakpointInfo & breakpoint)
{
    std::string  address = (breakpoint.last > breakpoint.address)
                         ? std::format ("{:04x}-{:04x}", breakpoint.address, breakpoint.last)
                         : std::format ("{:04x}", breakpoint.address);
    int          size    = (int) (Word) (breakpoint.last - breakpoint.address) + 1;



    switch (breakpoint.kind)
    {
    case BreakpointKind::Memory:
    case BreakpointKind::Io:
        return std::format ("{} {}{}", address,
                            breakpoint.access == WatchAccess::Read  ? "r"
                          : breakpoint.access == WatchAccess::Write ? "w"
                          :                                           "rw",
                            size);

    case BreakpointKind::Opcode:      return std::format ("opcode {:02x}", breakpoint.opcode);
    case BreakpointKind::Register:    return "register " + breakpoint.condition;
    case BreakpointKind::Brk:         return "brk";
    case BreakpointKind::Interrupt:   return "interrupt";
    case BreakpointKind::MemoryValue: return std::format ("{:04x} value {:02x}", breakpoint.address, breakpoint.value.value_or (0));
    default:                          return address;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatCallStack
//
//  k: a header, then a frame a line innermost first, numbered as WinDbg
//  numbers them. A break in the chain is a line beginning with dashes.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatCallStack (const CallStackData & data, Lines & lines)
{
    size_t  index = 0;



    if (data.rows.empty())
    {
        lines.push_back ("No calls are on the stack.");
        return;
    }

    lines.push_back ("#  call site  target        how");

    for (const CallStackRow & row : data.rows)
    {
        if (row.chainBreak.has_value())
        {
            lines.push_back (std::format ("-- {} --", CallStack::DescribeBreak (*row.chainBreak)));
        }
        else if (row.frame.has_value())
        {
            lines.push_back (std::format ("{:<3}{}", std::format ("{:02x}", index), FormatCallFrame (*row.frame)));
            index++;
        }
    }

    if (data.lastReturn.has_value())
    {
        lines.push_back ("Last return: " + FormatCallFrame (*data.lastReturn));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatCallFrame
//
//  The call site, the target and its symbol, then how it was entered and
//  which mechanism found it. A symbol too long for its column keeps a space
//  before the how.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::FormatCallFrame (const CallStackFrame & frame)
{
    static constexpr size_t  kTargetWidth = 14;
    std::string              how          = std::format ("{} {}", CallStack::GetKindName (frame.kind),
                                                         frame.provenance == CallProvenance::Recorded ? "recorded" : "guessed");
    std::string              target       = std::format ("{:04x} {}", frame.target, frame.symbol);



    if (!frame.isVerified)
    {
        how += ", unverified";
    }

    if (!frame.note.empty())
    {
        how += ", " + frame.note;
    }

    if (target.size() >= kTargetWidth)
    {
        target += " ";
    }

    return std::format ("{:<11}{:<14}{}", std::format ("{:04x}", frame.callSite), target, how);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatEvaluation
//
//  ?: `Evaluate expression: 784 = 0310`.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatEvaluation (const CalcData & data, Lines & lines)
{
    lines.push_back (std::format ("Evaluate expression: {} = {:04x}", data.value, data.value));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::FormatFormats
//
//  .formats: the value in hex, decimal and binary, and its two bytes as
//  characters, high byte first, a period where one is not printable.
//
////////////////////////////////////////////////////////////////////////////////

void WinDbgFormatter::FormatFormats (const CalcData & data, Lines & lines)
{
    static constexpr int   kBitsPerByte = 8;
    static constexpr Byte  kFirstPrint  = 0x20;
    static constexpr Byte  kLastPrint   = 0x7E;
    Byte                   high         = (Byte) (data.value >> kBitsPerByte);
    Byte                   low          = (Byte) data.value;
    std::string            chars;



    for (Byte b : { high, low })
    {
        chars += (b >= kFirstPrint && b <= kLastPrint) ? (char) b : '.';
    }

    lines.push_back ("Evaluate expression:");
    lines.push_back (std::format ("  Hex:     {:04x}", data.value));
    lines.push_back (std::format ("  Decimal: {}", data.value));
    lines.push_back (std::format ("  Binary:  {:08b} {:08b}", high, low));
    lines.push_back (std::format ("  Chars:   {}", chars));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::GetCommandName
//
//  The first word of the line, lowercase; `?` stands alone whatever
//  follows it.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::GetCommandName (const std::string & command)
{
    size_t       first = command.find_first_not_of (" \t");
    size_t       end   = 0;
    std::string  name;



    if (first == std::string::npos)
    {
        return name;
    }

    if (command[first] == '?')
    {
        return "?";
    }

    end  = command.find_first_of (" \t", first);
    name = command.substr (first, end == std::string::npos ? std::string::npos : end - first);

    return ToLower (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::GetArguments
//
//  What follows the command's first word, trimmed.
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::GetArguments (const std::string & command)
{
    size_t  first = command.find_first_not_of (" \t");
    size_t  end   = (first == std::string::npos) ? std::string::npos : command.find_first_of (" \t", first);
    size_t  from  = (end == std::string::npos) ? std::string::npos : command.find_first_not_of (" \t", end);
    size_t  last  = command.find_last_not_of (" \t");



    return (from == std::string::npos) ? std::string() : command.substr (from, last - from + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::string WinDbgFormatter::ToLower (const std::string & text)
{
    std::string  result = text;



    for (char & ch : result)
    {
        ch = (char) tolower ((unsigned char) ch);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter::GetBytes
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::optional<Byte>> WinDbgFormatter::GetBytes (const MemoryData & data)
{
    std::vector<std::optional<Byte>>  bytes;



    for (const MemoryRow & row : data.rows)
    {
        bytes.insert (bytes.end(), row.bytes.begin(), row.bytes.end());
    }

    return bytes;
}





