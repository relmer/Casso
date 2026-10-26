#include "Pch.h"

#include "Ui/Debugger/BreakpointDefinitionCheck.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/Basic/ApplesoftProgram.h"
#include "Debugger/ConditionContext.h"
#include "Debugger/DiskValueSymbols.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Machines/Apple2/Common/VideoTiming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::GetError
//
//  The parse first, then the limits the command's handler checks, then the
//  condition, as the handler would evaluate it once, and last a duplicate.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDefinitionCheck::GetError (const std::string & definition, const DebuggerViewSnapshot & snapshot, std::optional<int> editedId)
{
    SnapshotContext      context (snapshot);
    AppleWinParseResult  parsed  = AppleWinParser::Parse (definition, context);
    const DebugCommand & command = parsed.command;
    bool                 isWatch = command.verb == DebugVerb::SetMemoryWatchpoint || command.verb == DebugVerb::SetReadWatchpoint ||
                                   command.verb == DebugVerb::SetWriteWatchpoint;
    bool                 isValue = command.verb == DebugVerb::SetValueBreakpoint;
    std::string          error;
    HRESULT              hr      = S_OK;



    if (parsed.status != ParseStatus::Ok)
    {
        return parsed.error.empty() ? std::string ("The breakpoint cannot be read.") : parsed.error;
    }

    error = GetRangeError (command, snapshot);

    if (!error.empty())
    {
        return error;
    }

    if (!command.expression.postfix.empty())
    {
        hr = ConditionContext::Validate (command.expression, context, isWatch || isValue, isValue || (isWatch && command.text != "BEFORE"), error);

        if (FAILED (hr))
        {
            return error;
        }
    }

    return GetDuplicateError (command, snapshot, editedId);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDefinitionCheck::ToUpper (const std::string & text)
{
    std::string  upper = text;



    std::ranges::transform (upper, upper.begin(), [] (char ch) { return (char) std::toupper ((unsigned char) ch); });
    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::GetRangeError
//
//  What the handlers check after the parse: a BASIC line number, a BASIC
//  variable's form, a scanline and a cycle inside the frame, and a Disk II
//  for a disk breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDefinitionCheck::GetRangeError (const DebugCommand & command, const DebuggerViewSnapshot & snapshot)
{
    Word            line      = 0;
    BasicReference  reference;
    bool            isVbl     = command.text == "VBL";



    switch (command.verb)
    {
    case DebugVerb::SetBasicLineBreakpoint:
        return BasicHandlers::TryParseLine (command.text, line) ? std::string() : std::string ("The line number must be 0 to 63999.");

    case DebugVerb::SetBasicVariableBreakpoint:
        return ApplesoftProgram::TryParseReference (command.text, reference) ? std::string()
             : std::string ("The variable must be written as a program writes it: A, B%, C$ or D(3,4).");

    case DebugVerb::BreakOnVideoLine:
        return (command.a1 < VideoTiming::kScanlinesPerFrame) ? std::string()
             : std::format ("Scanline {:X} is past the frame. Scanlines run from 0 to {:X}.", command.a1, VideoTiming::kScanlinesPerFrame - 1);

    case DebugVerb::BreakOnBeam:
        if (!isVbl && command.a1 >= VideoTiming::kScanlinesPerFrame)
        {
            return std::format ("Scanline {:X} is past the frame. Scanlines run from 0 to {:X}.", command.a1, VideoTiming::kScanlinesPerFrame - 1);
        }

        return (isVbl || command.a2 < VideoTiming::kCyclesPerScanline) ? std::string()
             : std::format ("Cycle {:X} is past the scanline. Cycles run from 0 to {:X}.", command.a2, VideoTiming::kCyclesPerScanline - 1);

    case DebugVerb::SetDiskBreakpoint:
        return snapshot.diskSlot.has_value() ? std::string() : std::string ("This machine has no Disk II controller.");

    default:
        return std::string();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::GetDuplicateError
//
//  An execution breakpoint over the same addresses with the same condition
//  as one already set, other than the one being edited.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointDefinitionCheck::GetDuplicateError (const DebugCommand & command, const DebuggerViewSnapshot & snapshot, std::optional<int> editedId)
{
    Word  last = command.hasA2 ? command.a2 : command.a1;



    if (command.verb != DebugVerb::SetBreakpoint)
    {
        return std::string();
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        bool  isSame = bp.info.kind == BreakpointKind::Address && bp.info.address == command.a1 && bp.info.last == last &&
                       bp.info.condition == command.expression.text && bp.id != editedId.value_or (-1);



        if (isSame)
        {
            return std::format ("Breakpoint #{} is already set at ${:04X}.", bp.id, command.a1);
        }
    }

    return std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::SnapshotContext::TryGetRegister
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointDefinitionCheck::SnapshotContext::TryGetRegister (const std::string & name, Word & value) const
{
    value = 0;
    return name == "A" || name == "X" || name == "Y" || name == "P" || name == "S" || name == "PC";
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::SnapshotContext::TryPeek
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointDefinitionCheck::SnapshotContext::TryPeek (Word address, Byte & value) const
{
    (void) address;

    value = 0;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointDefinitionCheck::SnapshotContext::TryResolveSymbol
//
//  The disk values on a machine with a Disk II, the flags P.C to P.N, a
//  search result @n, and the snapshot's symbols.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointDefinitionCheck::SnapshotContext::TryResolveSymbol (const std::string & name, Word & address) const
{
    std::string  upper  = ToUpper (name);
    bool         isFlag = upper.size() == 3 && upper.starts_with ("P.") && std::string_view ("CZIDBVN").find (upper[2]) != std::string_view::npos;
    bool         isHit  = upper.size() >= 2 && upper[0] == '@' && upper.find_first_not_of ("0123456789", 1) == std::string::npos;



    address = 0;

    if (DiskValueSymbols::IsDiskValue (name))
    {
        return m_snapshot.diskSlot.has_value();
    }

    if (isFlag || isHit)
    {
        return true;
    }

    if (m_snapshot.symbols == nullptr)
    {
        return false;
    }

    for (const SymbolInfo & symbol : m_snapshot.symbols->symbols)
    {
        if (ToUpper (symbol.name) == upper)
        {
            address = symbol.address;
            return true;
        }
    }

    return false;
}
