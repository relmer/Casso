#include "Pch.h"

#include "Debugger/Handlers/TraceHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/DebugSession.h"
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
//  now, which may have changed since the instruction ran.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Describe (DebugSession & session, std::vector<TraceRecord> & entries)
{
    const SymbolTable        & symbols           = session.GetSymbols();
    const Microcode          * instructionSet    = session.GetTarget().GetInstructionSet();
    bool                       hasInstructionSet = instructionSet != nullptr;
    Disassembler               disassembler      (instructionSet);
    DisassembledInstruction    instruction;
    SymbolTableId              table             = SymbolTableId::Main;
    HRESULT                    hr                = S_OK;



    for (TraceRecord & record : entries)
    {
        Byte  bytes[Disassembler::kMaxInstructionBytes] = { record.opcode, record.op1, record.op2 };



        if (hasInstructionSet)
        {
            hr = disassembler.DisassembleOne (record.pc, bytes, instruction);
            IGNORE_RETURN_VALUE (hr, S_OK);

            record.instruction = instruction.operand.empty() ? instruction.mnemonic
                                                             : instruction.mnemonic + " " + instruction.operand;
            record.length      = (Byte) instruction.bytes.size();
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
//  A first past the end gives no entries; a window running past the end is
//  cut at it.
//
////////////////////////////////////////////////////////////////////////////////

void TraceHandlers::Show (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    size_t  total = session.GetTarget().GetTraceSize();
    size_t  count = (command.count != 0) ? command.count : kDefaultCount;
    size_t  first = 0;



    if (command.first.has_value())
    {
        first = (size_t) std::min<uint64_t> (*command.first, total);
    }
    else
    {
        first = (total > count) ? total - count : 0;
    }

    reply.data = MakeWindow (session, first, count);
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
    std::vector<TraceRecord>   entries;
    std::string                text;



    CBRF (files != nullptr, reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files."));
    CBRF (total != 0,       reply.SetError (CommandStatus::Error, "no trace", "The trace is empty. Use HISTORY ON to start one."));

    target.GetTraceWindow (0, total, entries);
    Describe (session, entries);

    for (const TraceRecord & record : entries)
    {
        text += AppleWinFormatter::FormatTraceLine (record) + "\n";
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), text);
    CHRF (hr, reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text)));

    reply.data = MessageData { { std::format ("Saved {} trace {}{}.", entries.size(), entries.size() == 1 ? "entry" : "entries", session.GetPathEcho (command.text, "to")) } };

Error:
    return;
}
