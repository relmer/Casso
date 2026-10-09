#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointColumns
//
//  The breakpoints pane's columns: those Visual Studio's Breakpoints window
//  lists that apply to Casso, and Kind and Trigger. The text of each column
//  for a breakpoint is built from the snapshot alone so it is testable
//  without a window, as is the order a click on a heading sorts the rows
//  into.
//
//  Name gives what the breakpoint is: an address or range with the symbol
//  there, the source file and line at its address, an opcode, a register
//  condition, an I/O range, BRK or an interrupt. The other columns give one
//  fact each: Kind what the breakpoint watches, Trigger what about it stops
//  the machine, Symbol the symbol at the address, Function the symbol an
//  execution breakpoint stops at, Data what a data breakpoint watches.
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
        Trigger,
        Symbol,
        WhenHit,
        Function,
        File,
        Address,
        Data,
        Count,
    };

    //  What sort of breakpoint a row is, in the order a sort by Kind puts
    //  them. Each gives the row's Kind and Trigger: the memory kinds share
    //  the Kind "Memory" and differ in Trigger. An execution breakpoint is
    //  Execution whether or not a source line produced the code at its
    //  address; its File column shows that.
    enum class RowKind
    {
        Execution,
        DataRead,
        DataWrite,
        DataReadOrWrite,
        DataValue,
        Register,
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
    static RowKind              GetRowKind     (const BreakpointInfo & info);
    static std::string          GetKindText    (RowKind kind);
    static std::string          GetTriggerText (RowKind kind);

    //  The snapshot's breakpoint indexes in the order the column sorts them,
    //  ties kept in the engine's order.
    static std::vector<size_t>  GetOrder    (const DebuggerViewSnapshot & snapshot, Column column, bool descending);

    //  Which columns show: Name always, Condition, Hit count, Kind and
    //  Trigger by default. Kept in the saved open views as a token, written only for a
    //  choice other than the default, and read back from them; a missing or
    //  unreadable token is the default.
    using Shown = std::array<bool, kCount>;

    static Shown        GetDefaultShown ();
    static std::string  FormatShown     (const Shown & shown);
    static Shown        ParseShown      (const std::string & text);

    //  Where Go to source code goes: the line a breakpoint was set on, or the
    //  line that produced the code at its address.
    static bool  TryGetSourcePlace (const DebuggerViewSnapshot & snapshot, const DebuggerViewSnapshot::BreakpointLine & bp,
                                    int & fileId, int & line);

    //  Whether a breakpoint stops at an address: an opcode, a register
    //  condition, BRK and an interrupt stop anywhere.
    static bool  HasAddress        (const BreakpointInfo & info);

private:
    //  Not "bpcols" or "bpcols2": their bits followed the columns' earlier
    //  orders. "bpcols" had Labels between Condition and Hit count and
    //  Filter after Hit count, and no Kind (MigrateShown); "bpcols2" had
    //  Kind but no Trigger (MigrateKindShown).
    static constexpr const char *  kpszToken     = "bpcols3";
    static constexpr const char *  kpszKindToken = "bpcols2";
    static constexpr const char *  kpszOldToken  = "bpcols";

    static std::string  GetName          (const DebuggerViewSnapshot::BreakpointLine & bp, const std::string & sourceLine);
    static std::string  GetData          (const BreakpointInfo & info);
    static std::string  GetRange         (const BreakpointInfo & info);
    static std::string  GetWhenHit       (const BreakpointInfo & info);
    static bool         TryGetSourceLine (const DebuggerViewSnapshot & snapshot, int id, std::string & file, int & line);
    static bool         TryReadMask      (const std::string & token, const char * pszToken, unsigned & mask);
    static Shown        MakeShown        (unsigned mask);
    static Shown        MigrateShown     (unsigned mask);
    static Shown        MigrateKindShown (unsigned mask);
};
