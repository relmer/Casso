#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns
//
//  The breakpoints pane's columns (FR-117): those Visual Studio's Breakpoints
//  window lists that apply to Casso, and Kind. The text of each column for a
//  breakpoint is built from the snapshot alone so it is testable without a
//  window, as is the order a click on a heading sorts the rows into.
//
//  Name gives what the breakpoint is: an address or range with the symbol
//  there, the source file and line it was set from, an opcode, a register
//  condition, an I/O range, BRK or an interrupt. The other columns give one
//  fact each: Kind the sort of breakpoint, Symbol the symbol at the address,
//  Function the symbol an execution breakpoint stops at, Data what a data
//  breakpoint watches.
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
        Symbol,
        WhenHit,
        Function,
        File,
        Address,
        Data,
        Count,
    };

    //  What sort of breakpoint a row is, in the order a sort by Kind puts
    //  them. The engine keeps an execution breakpoint's address and not how
    //  it was given, so one set from a source line is SourceLine, one at a
    //  single address with a symbol there is Function, and any other is
    //  Address.
    enum class RowKind
    {
        Address,
        SourceLine,
        Function,
        DataRead,
        DataWrite,
        DataReadOrWrite,
        DataValue,
        RegisterCondition,
        Opcode,
        Io,
        Brk,
        Interrupt,
        Count,
    };

    static constexpr size_t  kCount = (size_t) Column::Count;

    using Cells = std::array<std::string, kCount>;

    static std::wstring         GetHeading  (Column column);
    static Cells                GetCells    (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp);
    static RowKind              GetRowKind  (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp);
    static std::string          GetKindText (RowKind kind);

    //  The snapshot's breakpoint indexes in the order the column sorts them,
    //  ties kept in the engine's order.
    static std::vector<size_t>  GetOrder    (const DebuggerViewSnapshot & snapshot, Column column, bool descending);

    //  Which columns show (FR-118): Name always, Condition, Hit count and Kind
    //  by default. Kept in the saved open views as a token, written only for
    //  a choice other than the default, and read back from them; a missing
    //  or unreadable token is the default.
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
    //  Not "bpcols": that token's bits followed the columns' order before
    //  Kind, with Labels where Symbol is and Filter after Hit count. A mask
    //  saved under it is read through that order instead (MigrateShown).
    static constexpr const char *  kpszToken    = "bpcols2";
    static constexpr const char *  kpszOldToken = "bpcols";

    static std::string  GetName          (const DebuggerViewSnapshot::BreakpointLine & bp, const std::string & sourceLine);
    static std::string  GetData          (const BreakpointInfo & info);
    static std::string  GetRange         (const BreakpointInfo & info);
    static std::string  GetWhenHit       (const BreakpointInfo & info);
    static bool         TryGetSourceLine (const DebuggerViewSnapshot & snapshot, int id, std::string & file, int & line);
    static bool         TryReadMask      (const std::string & token, const char * pszToken, unsigned & mask);
    static Shown        MakeShown        (unsigned mask);
    static Shown        MigrateShown     (unsigned mask);
};
