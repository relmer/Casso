#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns
//
//  The breakpoints pane's columns (FR-117): the text of each column for a
//  breakpoint, built from the snapshot alone so it is testable without a
//  window, and the order a click on a column's heading sorts the rows into.
//
//  Name says what the breakpoint is: an address or range with the symbol
//  there, the source file and line it was set from, an opcode, a register
//  condition, an I/O range, BRK or an interrupt. The other columns give one
//  fact each.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointColumns
{
public:
    enum class Column
    {
        Name,
        Condition,
        HitCount,
        Kind,
        Address,
        Label,
        File,
        WhenHit,
        Count,
    };

    static constexpr size_t  kCount = (size_t) Column::Count;

    using Cells = std::array<std::string, kCount>;

    static std::wstring         GetHeading (Column column);
    static Cells                GetCells   (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp);

    //  The snapshot's breakpoint indexes in the order the column sorts them,
    //  ties kept in the engine's order.
    static std::vector<size_t>  GetOrder   (const DebuggerViewSnapshot & snapshot, Column column, bool descending);

    //  Which columns show (FR-118): Name always, Condition and Hit count by
    //  default. Kept in the saved open views as a token that says nothing
    //  for the default, and read back from them; a missing or unreadable
    //  token is the default.
    using Shown = std::array<bool, kCount>;

    static Shown        GetDefaultShown ();
    static std::string  FormatShown     (const Shown & shown);
    static Shown        ParseShown      (const std::string & text);

    //  Where Go to source code goes (FR-119): the line a breakpoint was set
    //  on, or the line that produced the code at its address.
    static bool  TryGetSourcePlace (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp,
                                    int & fileId, int & line);

    //  Whether a breakpoint stops at an address: an opcode, a register
    //  condition, BRK and an interrupt stop anywhere.
    static bool  HasAddress        (const BreakpointInfo & info);

private:
    static constexpr const char *  kpszToken = "bpcolumns";

    static std::string  GetName     (const DebuggerViewSnapshot::BreakpointLine & bp, const std::string & sourceLine);
    static std::string  GetKind     (const BreakpointInfo & info);
    static std::string  GetRange    (const BreakpointInfo & info);
    static std::string  GetWhenHit  (const BreakpointInfo & info);
    static bool         TryGetSourceLine (const DebuggerViewSnapshot & snapshot, int id, std::string & file, int & line);
};
