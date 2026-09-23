#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinFormatter
//
//  Renders a reply's data as AppleWin-mode text into Reply::text, and a
//  non-ok reply as the two-line error. The JSON form is rendered from the
//  same data by ReplyJson, so the two cannot disagree.
//
////////////////////////////////////////////////////////////////////////////////

class AppleWinFormatter
{
public:
    static void         Format          (Reply & reply);

    // The line batch mode and the window print for a stop.
    static std::string  FormatStop      (const StopEvent & stop);

    static std::string  FormatFlags     (Byte p);
    static std::string  FormatAddress   (const BreakpointInfo & breakpoint);

    // One HISTORY line, as the trace pane and HISTORY SAVE show it too.
    static std::string  FormatTraceLine  (const TraceRecord & record);

    // An entry's opcode bytes, as many as its instruction has ("8D 00 04").
    static std::string  FormatTraceBytes (const TraceRecord & record);

    // A breakpoint's kind and place in words, as BPL lists it.
    static std::string  DescribeBreakpoint (const BreakpointInfo & breakpoint);

private:
    using Lines = std::vector<std::string>;

    static void  FormatError        (Reply & reply);
    static void  FormatData         (const ReplyData & data, Lines & lines);
    static void  FormatRegisters    (const RegistersData      & data, Lines & lines);
    static void  FormatMemory       (const MemoryData         & data, Lines & lines);
    static void  FormatDisassembly  (const DisassemblyData    & data, Lines & lines);
    static void  FormatBreakpointSet  (const BreakpointSetData  & data, Lines & lines);
    static void  FormatBreakpointList (const BreakpointListData & data, Lines & lines);
    static void  FormatWatchList    (const WatchListData      & data, Lines & lines);
    static void  FormatSearchHits   (const SearchHitsData     & data, Lines & lines);
    static void  FormatStack        (const StackData          & data, Lines & lines);
    static void  FormatSoftSwitches (const SoftSwitchData     & data, Lines & lines);
    static void  FormatSymbols      (const SymbolData         & data, Lines & lines);
    static void  FormatFileIo       (const FileIoData         & data, Lines & lines);
    static void  FormatCompare      (const CompareData        & data, Lines & lines);
    static void  FormatDataBlocks   (const DataBlockListData  & data, Lines & lines);
    static void  FormatProfile      (const ProfileData        & data, Lines & lines);
    static void  FormatCalc         (const CalcData           & data, Lines & lines);
    static void  FormatStepFilter   (const StepFilterData     & data, Lines & lines);
    static void  FormatCallStack    (const CallStackData      & data, Lines & lines);

    static std::string  FormatCallFrame (const CallStackFrame & frame);
    static void  FormatTrace        (const TraceData          & data, Lines & lines);

    static void  FormatProfileOpcodes   (const ProfileData & data, Lines & lines);
    static void  FormatProfileAddresses (const ProfileData & data, Lines & lines);
    static double  GetShare             (uint64_t part, uint64_t total);

    static std::string  FormatCondition    (const StopEvent & stop);

    static constexpr int   kBytesPerRow  = 8;
    static constexpr int   kDetailIndent = 7;
};
