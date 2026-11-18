#include "Pch.h"

#include "Ui/Debugger/BreakpointActions.h"
#include "Ui/Debugger/DebuggerCommandLines.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"

#include "Debugger/DiskBreakSpecText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetToggleBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetToggleBreakpoint (const DebuggerViewSnapshot & snapshot, Word address, CommandMode mode)
{
    DebuggerAction  action;



    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (DebuggerCommandLines::IsCodeBreakpointAt (bp, address))
        {
            return GetClearBreakpoint (bp.id, mode);
        }
    }

    action               = DebuggerActions::Make (DebugVerb::SetBreakpoint, std::format ("BP {:04X}", address), mode);
    action.command.a1    = address;
    action.command.hasA1 = true;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetEnableBreakpoint
//
//  Each goes by id, which the command carries as its count.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetEnableBreakpoint (int id, bool enable, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (enable ? DebugVerb::EnableBreakpoint : DebugVerb::DisableBreakpoint,
                                                    std::format ("{} {}", enable ? "BPE" : "BPD", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetClearBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetClearBreakpoint (int id, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearBreakpoint, std::format ("BPC {}", id), mode);



    action.command.count = (uint32_t) id;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetBreakpointStep
//
//  The lines the step runs are echoed one by one as they run, in AppleWin's
//  words, so the action itself carries no echo.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetBreakpointStep (BreakpointStep step)
{
    DebuggerAction  action;



    action.breakpointStep = std::move (step);
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetSourceBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetSourceBreakpoint (const std::string & fileName, int line, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetSourceBreakpoint, std::format ("BP {}:{}", fileName, line), mode);



    action.command.text  = fileName;
    action.command.count = (uint32_t) line;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetSaveBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetSaveBreakpoints (const std::string & path, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SaveBreakpoints, "BPSAVE " + path, mode);



    action.command.text = path;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetEnableAllBreakpoints / GetClearAllBreakpoints
//
//  The command carries `*` as its text, as the parser leaves it.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetEnableAllBreakpoints (bool enable, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (enable ? DebugVerb::EnableBreakpoint : DebugVerb::DisableBreakpoint, enable ? "BPE *" : "BPD *", mode);



    action.command.text = "*";
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetClearAllBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetClearAllBreakpoints (CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ClearBreakpoint, "BPC *", mode);



    action.command.text = "*";
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetEditBreakpoint
//
//  The definition is evaluated into the command the handler takes it as
//  when the action runs.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetEditBreakpoint (int id, const BreakpointDefinition & definition, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::EditBreakpoint, std::format ("BPEDIT {} {}", id, GetBreakpointEcho (definition)), mode);



    action.command.count = (uint32_t) id;
    action.breakpoint    = definition;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetChangeBreakpoint
//
//  E or e for enabled, T or t for temporary, S or s for whether a hit stops
//  the machine.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetChangeBreakpoint (int id, bool enabled, bool temporary, bool stops, CommandMode mode)
{
    std::string     flags  = std::format ("{}{}{}", enabled ? 'E' : 'e', temporary ? 'T' : 't', stops ? 'S' : 's');
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::ChangeBreakpoint, std::format ("BPCHANGE {} {}", id, flags), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = flags;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetDefineBreakpoint
//
//  The verb is set from the definition's type when the action runs.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetDefineBreakpoint (const BreakpointDefinition & definition, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetBreakpoint, GetBreakpointEcho (definition), mode);



    action.breakpoint = definition;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetBreakpointEcho
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointActions::GetBreakpointEcho (const BreakpointDefinition & definition)
{
    using Type = BreakpointDefinition::Type;

    static constexpr const char *                  kDrivePrefixes[] = { "", "D1 ", "D2 " };
    std::span<const DiskBreakSpecText::EventWord>  words            = DiskBreakSpecText::GetEventWords();
    std::string                                    clause           = definition.condition.empty() ? std::string() : " IF " + definition.condition;
    std::string                                    mode             = definition.isBefore ? " BEFORE" : "";
    std::string                                    state            = definition.isOff ? " OFF" : " ON";



    switch (definition.type)
    {
    case Type::Read:          return "BPMR " + definition.address + mode + clause;
    case Type::Write:         return "BPMW " + definition.address + mode + clause;
    case Type::ReadWrite:     return "BPM " + definition.address + mode + clause;
    case Type::Value:         return "BPMV " + definition.address + " " + definition.value + clause;
    case Type::Condition:     return "BPR IF " + definition.condition;
    case Type::Opcode:        return "BRKOP " + definition.value + clause;
    case Type::Brk:           return "BRK" + state + (definition.isOff ? std::string() : clause);
    case Type::Interrupt:     return "BRKINT" + state + (definition.isOff ? std::string() : clause);
    case Type::BasicError:    return "BRKBASIC" + state + (definition.isOff ? std::string() : clause);
    case Type::UnwrittenRead: return "BRKUNINIT" + state;
    case Type::BasicLine:     return "BPBASIC " + definition.address + clause;
    case Type::BasicVariable: return "BPBASICVAR " + definition.address + clause;
    case Type::VideoLine:     return definition.isOff ? std::string ("BPV OFF") : "BPV " + definition.address;
    case Type::SourceLine:    return "BP " + definition.file + ":" + definition.address + clause;
    case Type::Disk:
        return std::format ("BPDISK {}{}{}{}", kDrivePrefixes[std::clamp (definition.drive, 0, 2)],
                            words[std::clamp<size_t> ((size_t) (std::max) (definition.event, 0), 0, words.size() - 1)].word,
                            definition.arguments.empty() ? std::string() : " " + definition.arguments, clause);
    case Type::Beam:
        return definition.isOff                                    ? std::string ("BPBEAM OFF")
             : (_stricmp (definition.address.c_str(), "VBL") == 0) ? std::string ("BPBEAM VBL")
             :                                                       "BPBEAM " + definition.address + " " + definition.value;
    default:                  return "BP " + definition.address + clause;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetGroupBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetGroupBreakpoints (const std::string & arguments, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::GroupBreakpoints, "BPGROUP " + arguments, mode);



    action.command.text = arguments;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetNameBreakpoint
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetNameBreakpoint (int id, const std::string & name, CommandMode mode)
{
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetBreakpointName,
                                                    name.empty() ? std::format ("BPNAME {}", id) : std::format ("BPNAME {} \"{}\"", id, name), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = name;
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointActions::GetLogBreakpoint
//
//  The text is what the handler reads after the id: the message, GO and the
//  quoted message, or CLEAR.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerAction BreakpointActions::GetLogBreakpoint (int id, const std::string & message, bool isGo, CommandMode mode)
{
    std::string     text   = message.empty() ? std::string ("CLEAR") : isGo ? std::format ("GO \"{}\"", message) : std::format ("\"{}\"", message);
    DebuggerAction  action = DebuggerActions::Make (DebugVerb::SetBreakpointLog, std::format ("BPLOG {} {}", id, text), mode);



    action.command.count = (uint32_t) id;
    action.command.text  = (message.empty() || isGo) ? text : message;
    return action;
}
