#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GSSquaredFormatter
//
//  Renders a reply's data as GSSquared's debugger prints it, into
//  Reply::text. The layouts follow GSSquared's own console output: a
//  bank-qualified address (`00/0300`), a dump of sixteen bytes a line with
//  the characters after them and a blank line to close it, `[id] kind addr`
//  breakpoint and watch lists, and `Breakpoint id=N set`.
//
//  A reply GSSquared has no layout for keeps the AppleWin text, as the
//  Monitor formatter does: errors, registers, stops, and every engine
//  command. GSSquared prints no stop line of its own; its window shows the
//  stop.
//
//  Lines carry no trailing spaces, where GSSquared pads its disassembly to a
//  fixed width, so a golden file does not hinge on invisible characters.
//
////////////////////////////////////////////////////////////////////////////////

class GSSquaredFormatter
{
public:
    static void         Format        (Reply & reply);

    //  `00/0300`, the form every GSSquared address takes.
    static std::string  FormatAddress (Word address);

private:
    using Lines = std::vector<std::string>;

    //  False when GSSquared has no layout for this reply, which sends it to
    //  the AppleWin formatter instead.
    static bool  TryFormatData        (const Reply & reply, Lines & lines);

    static void  FormatMemory         (const MemoryData         & data, Lines & lines);
    static void  FormatDisassembly    (const DisassemblyData    & data, Lines & lines);
    static void  FormatBreakpointList (const BreakpointListData & data, Lines & lines);
    static void  FormatWatchList      (const WatchListData      & data, bool isAdd, Lines & lines);
    static void  FormatSymbols        (const SymbolData         & data, Lines & lines);
    static void  TrimTrailingSpaces   (Lines & lines);

    static constexpr size_t  kBytesPerLine    = 16;
    static constexpr size_t  kOperandColumn   = 23;
    static constexpr size_t  kMnemonicColumn  = 18;
};
