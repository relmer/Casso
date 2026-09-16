#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorFormatter
//
//  Renders a reply's data as the Apple II Monitor renders it, into
//  Reply::text.
//
//  THE SAME DATA, RENDERED TWICE. This and AppleWinFormatter read the same
//  Reply and write different text, which is what lets one engine serve both
//  modes. The JSON form is rendered from the data by ReplyJson rather than
//  from either of them, so no two of the three can drift into agreeing with
//  a mistake.
//
//  A reply the Monitor has no layout of its own for keeps the AppleWin text.
//  That is not a gap: the only way to reach such a command from Monitor mode
//  is to type `/`, and a reader who did that asked for the AppleWin command
//  and should get its output.
//
//  The layout does not vary by machine, so a batch script's expected output
//  is one file whichever Apple II it ran against.
//
////////////////////////////////////////////////////////////////////////////////

class MonitorFormatter
{
public:
    static void         Format          (Reply & reply);

    //  The line a stop prints. With the instruction line a step's reply
    //  carries, this is the original ]['s step display: the instruction,
    //  then the registers.
    static std::string  FormatStop      (const StopEvent & stop);

    static std::string  FormatRegisters (const Cpu6502Registers & registers);

private:
    using Lines = std::vector<std::string>;

    static void  FormatError       (Reply & reply);

    //  False when the Monitor has no layout for this kind, which is what
    //  sends the reply to the AppleWin formatter instead.
    static bool  TryFormatData     (const ReplyData & data, Lines & lines);

    static void  FormatMemory      (const MemoryData      & data, Lines & lines);
    static void  FormatDisassembly (const DisassemblyData & data, Lines & lines);
    static void  FormatCompare     (const CompareData     & data, Lines & lines);
    static void  FormatSearchHits  (const SearchHitsData  & data, Lines & lines);

    //  Examine rows arrive already broken on eight-byte boundaries, so
    //  `303.30F` is five bytes against $0303 and eight against $0308. The
    //  Monitor labels a row with the address that starts it rather than with
    //  wherever the reader began, and the command that built the rows is
    //  what knows the range.
    static constexpr size_t  kBytesColumn  = 12;
    static constexpr int     kDetailIndent = 7;
};
