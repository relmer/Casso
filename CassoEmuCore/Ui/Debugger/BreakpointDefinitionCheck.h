#pragma once

#include "Pch.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck
//
//  Why a definition the breakpoint dialog made cannot be set, found from a
//  snapshot before the dialog closes: a line that does not parse, a number
//  out of its range, a condition with an unknown name or an I/O read, a disk
//  breakpoint on a machine with no Disk II, or an execution breakpoint that
//  is already set. The registers and memory read as zero.
//
////////////////////////////////////////////////////////////////////////////////

class BreakpointDefinitionCheck
{
public:
    //  Nothing when the definition can be set; editedId is the breakpoint
    //  being edited, which is no duplicate of itself.
    static std::string  GetError (const std::string & definition, const DebuggerViewSnapshot & snapshot, std::optional<int> editedId);

private:
    //  The snapshot as an expression context: the registers and memory as
    //  zero, the flags and the disk values as zero, and the snapshot's
    //  symbols, compared without case.
    class SnapshotContext : public IDebugExpressionContext
    {
    public:
        explicit SnapshotContext (const DebuggerViewSnapshot & snapshot) : m_snapshot (snapshot) {}

        bool  TryGetRegister   (const std::string & name, Word & value) const override;
        bool  TryPeek          (Word address, Byte & value) const override;
        bool  TryResolveSymbol (const std::string & name, Word & address) const override;

    private:
        const DebuggerViewSnapshot  & m_snapshot;
    };

    static std::string  ToUpper           (const std::string & text);
    static std::string  GetRangeError     (const DebugCommand & command, const DebuggerViewSnapshot & snapshot);
    static std::string  GetDuplicateError (const DebugCommand & command, const DebuggerViewSnapshot & snapshot, std::optional<int> editedId);
};
