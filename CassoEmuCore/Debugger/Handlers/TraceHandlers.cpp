#include "Pch.h"

#include "Debugger/Handlers/TraceHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "Disassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool TraceHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::ShowHistory: Show   (session, command, reply); return true;
    case DebugVerb::SetHistory:  Switch (session, command, reply); return true;
    case DebugVerb::SaveHistory: Save   (session, command, reply); return true;
    default:                     return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::Describe
//
//  The instruction comes from the bytes the entry recorded, not from memory
//  now, which may have changed since the instruction ran. Its operand has
//  the symbol for the address it accesses, as the disassembly's does.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Describe (DebugSession & session, std::vector<TraceRecord> & entries)
{
    const SymbolTable        & symbols           = session.GetSymbols();
    const Microcode          * instructionSet    = session.GetTarget().GetInstructionSet();
    bool                       hasInstructionSet = instructionSet != nullptr;
    Disassembler               disassembler      (instructionSet);
    DisassemblyLine            line;
    SymbolTableId              table             = SymbolTableId::Main;
    HRESULT                    hr                = S_OK;



    for (TraceRecord & record : entries)
    {
        Byte  bytes[Disassembler::kMaxInstructionBytes] = { record.opcode, record.op1, record.op2 };



        if (!record.isAvailable)
        {
            continue;
        }

        if (hasInstructionSet)
        {
            line = DisassemblyLine();

            hr = disassembler.DisassembleOne (record.pc, bytes, line.instruction);
            IGNORE_RETURN_VALUE (hr, S_OK);

            if (line.instruction.hasOperandAddress)
            {
                DataDirectiveHandlers::ChooseOperandSymbol (session, line);
            }

            record.instruction = line.instruction.operand.empty() ? line.instruction.mnemonic
                                                                  : line.instruction.mnemonic + " " + line.GetShownOperand();
            record.length      = (Byte) line.instruction.bytes.size();
        }

        record.symbol.clear();
        record.accessSymbol.clear();
        symbols.TryFindName (record.pc, record.symbol, table);

        if (record.hasAccess)
        {
            symbols.TryFindName (record.accessAddress, record.accessSymbol, table);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::Show
//
//  A cycle past the newest entry returns no entries; a window running past
//  the newest is cut at it.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Show (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    size_t  total = session.GetTarget().GetTraceSize();
    size_t  count = (command.count != 0) ? command.count : kDefaultCount;
    size_t  first = 0;



    if (command.fromCycle.has_value())
    {
        first = FindCycle (session.GetTarget(), *command.fromCycle);
    }
    else
    {
        first = (total > count) ? total - count : 0;
    }

    reply.data = MakeWindow (session, first, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::FindCycle
//
//  The place in the trace of the first entry at or after cycle, or the
//  trace's size when every entry is before it. The cycle count rises from
//  one entry to the next, so the search halves the trace each step.
//
////////////////////////////////////////////////////////////////////////////////

size_t TraceHandlers::FindCycle (IDebugTarget & target, uint64_t cycle)
{
    size_t                     low  = 0;
    size_t                     high = target.GetTraceSize();
    size_t                     mid  = 0;
    std::vector<TraceRecord>   one;



    if (target.TryFindTraceCycle (cycle, low))
    {
        return low;
    }

    while (low < high)
    {
        mid = low + (high - low) / 2;
        target.GetTraceWindow (mid, 1, one);

        if (!one.empty() && one.front().cycles < cycle)
        {
            low = mid + 1;
        }
        else
        {
            high = mid;
        }
    }

    return low;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::MakeWindow
//
//  The count entries from first, cut at the end, each described.
//
////////////////////////////////////////////////////////////////////////////////

TraceData TraceHandlers::MakeWindow (DebugSession & session, size_t first, size_t count)
{
    IDebugTarget  & target = session.GetTarget();
    TraceData       data;



    data.isOn  = target.IsTraceOn();
    data.total = target.GetTraceSize();
    target.GetTraceWindow ((std::min) (first, (size_t) data.total), count, data.entries);
    Describe (session, data.entries);

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::Switch
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Switch (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IDebugTarget  & target = session.GetTarget();
    bool            isOn   = command.count != 0;



    target.SetTraceOn (isOn);

    reply.data = isOn ? MessageData { { "Trace on." } }
                      : MessageData { { std::format ("Trace off, {} {} retained.", target.GetTraceSize(), target.GetTraceSize() == 1 ? "entry" : "entries") } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers::Save
//
//  Every retained entry, oldest first, one HISTORY line each.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Save (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    HRESULT                    hr      = S_OK;
    IDebugTarget             & target  = session.GetTarget();
    IFileSystem              * files   = session.GetFileSystem();
    size_t                     total   = target.GetTraceSize();
    size_t                     first   = (total > kMaxSaved) ? total - kMaxSaved : 0;
    std::vector<TraceRecord>   entries;
    std::string                text;
    std::string                saved;



    CBRF (files != nullptr, reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files."));
    CBRF (total != 0,       reply.SetError (CommandStatus::Error, "no trace", "The trace is empty. Use HISTORY ON to start one."));

    target.GetTraceWindow (first, total - first, entries);
    Describe (session, entries);

    for (const TraceRecord & record : entries)
    {
        text += AppleWinFormatter::FormatTraceLine (record) + "\n";
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), text);
    CHRF (hr, reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text)));

    saved = (first > 0) ? std::format ("the newest {} of {}", entries.size(), total) : std::format ("{}", entries.size());

    reply.data = MessageData { { std::format ("Saved {} trace {}{}.", saved, entries.size() == 1 ? "entry" : "entries", session.GetPathEcho (command.text, "to")) } };

Error:
    return;
}
