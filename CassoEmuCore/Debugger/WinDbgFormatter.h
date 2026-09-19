#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinDbgFormatter
//
//  Renders a reply's data in WinDbg's layouts, into Reply::text.
//
//  One data kind has several WinDbg layouts -- a dump is db, dw, dd or da,
//  and an evaluation is ? or .formats -- so the command the reply echoes
//  picks the layout. A kind WinDbg has no layout for keeps the AppleWin
//  text, as does an error.
//
////////////////////////////////////////////////////////////////////////////////

class WinDbgFormatter
{
public:
    static void         Format          (Reply & reply);

    //  The stop line AppleWin mode prints, then r's line.
    static std::string  FormatStop      (const StopEvent & stop);

    static std::string  FormatRegisters (const Cpu6502Registers & registers);
    static std::string  FormatFlags     (Byte p);

private:
    using Lines = std::vector<std::string>;

    static bool         TryFormatData       (const std::string & name, const ReplyData & data, Lines & lines);
    static void         FormatBytes         (const MemoryData         & data, Lines & lines);
    static void         FormatUnits         (const MemoryData         & data, size_t unit, Lines & lines);
    static void         FormatString        (const MemoryData         & data, Lines & lines);
    static void         FormatDisassembly   (const DisassemblyData    & data, Lines & lines);
    static void         FormatBreakpoints   (const BreakpointListData & data, Lines & lines);
    static void         FormatCallStack     (const CallStackData      & data, Lines & lines);
    static void         FormatEvaluation    (const CalcData           & data, Lines & lines);
    static void         FormatFormats       (const CalcData           & data, Lines & lines);
    static std::string  FormatCallFrame     (const CallStackFrame & frame);
    static std::string  GetCommandName      (const std::string & command);

    //  The dump's bytes in order, from the first row's address.
    static std::vector<std::optional<Byte>>  GetBytes (const MemoryData & data);

    static constexpr size_t  kBytesPerLine = 16;
};
