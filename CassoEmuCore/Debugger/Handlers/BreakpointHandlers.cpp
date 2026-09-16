#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"

#include "Config/IFileSystem.h"
#include "Debugger/AppleWinFormatter.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugSession.h"
#include "Debugger/Disassembler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    BreakpointListData  list;



    switch (command.verb)
    {
    case DebugVerb::SetBreakpoint:              SetAddress    (session, command, reply);                         return true;
    case DebugVerb::SetConditionalBreakpoint:
    case DebugVerb::SetRegisterBreakpoint:      SetCondition  (session, command, reply);                         return true;
    case DebugVerb::SetBreakpointAndWatchpoint: SetBoth       (session, command, reply);                         return true;
    case DebugVerb::SetMemoryWatchpoint:        SetWatchpoint (session, command, WatchAccess::ReadWrite, reply); return true;
    case DebugVerb::SetReadWatchpoint:          SetWatchpoint (session, command, WatchAccess::Read,      reply); return true;
    case DebugVerb::SetWriteWatchpoint:         SetWatchpoint (session, command, WatchAccess::Write,     reply); return true;
    case DebugVerb::BreakOnBrk:                 SetBrk        (session, command, reply);                         return true;
    case DebugVerb::BreakOnOpcode:              SetOpcode     (session, command, reply);                         return true;
    case DebugVerb::BreakOnInterrupt:           SetInterrupt  (session, command, reply);                         return true;
    case DebugVerb::ClearBreakpoint:            Clear         (session, command, reply);                         return true;
    case DebugVerb::DisableBreakpoint:          Enable        (session, command, false, reply);                  return true;
    case DebugVerb::EnableBreakpoint:           Enable        (session, command, true,  reply);                  return true;
    case DebugVerb::EditBreakpoint:             Edit          (session, command, reply);                         return true;
    case DebugVerb::ChangeBreakpoint:           Change        (session, command, reply);                         return true;
    case DebugVerb::SaveBreakpoints:            Save          (session, command, reply);                         return true;

    case DebugVerb::ListBreakpoints:
        ListAll (session, list);
        reply.data = list;
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::ListAll
//
//  Both tables, in id order.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::ListAll (DebugSession & session, BreakpointListData & list)
{
    for (const Breakpoint & entry : session.GetBreakpoints().GetAll())
    {
        list.breakpoints.push_back (MakeInfo (entry));
    }

    for (const Watchpoint & entry : session.GetWatchpoints().GetAll())
    {
        list.breakpoints.push_back (MakeInfo (entry));
    }

    std::sort (list.breakpoints.begin(), list.breakpoints.end(),
               [] (const BreakpointInfo & a, const BreakpointInfo & b) { return a.id < b.id; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::MakeInfo (Breakpoint)
//
////////////////////////////////////////////////////////////////////////////////

BreakpointInfo BreakpointHandlers::MakeInfo (const Breakpoint & entry)
{
    BreakpointInfo  info;



    info.id        = entry.id;
    info.kind      = entry.kind;
    info.address   = entry.first;
    info.last      = entry.last;
    info.opcode    = entry.opcode;
    info.condition = entry.condition.text;
    info.enabled   = entry.enabled;
    info.temporary = entry.temporary;
    info.stops     = entry.stops;
    info.hits      = entry.hits;
    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::MakeInfo (Watchpoint)
//
////////////////////////////////////////////////////////////////////////////////

BreakpointInfo BreakpointHandlers::MakeInfo (const Watchpoint & entry)
{
    BreakpointInfo  info;



    info.id        = entry.id;
    info.kind      = BreakpointKind::Memory;
    info.address   = entry.first;
    info.last      = entry.last;
    info.access    = entry.access;
    info.mode      = entry.mode;
    info.enabled   = entry.enabled;
    info.temporary = entry.temporary;
    info.stops     = entry.stops;
    info.hits      = entry.hits;
    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::MakeDefinition
//
//  A condition is written back as BPR with its register first, so `A=41`
//  becomes `BPR A =41`. An I/O entry has no command of its own and is
//  written as the memory watchpoint it behaves as.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHandlers::MakeDefinition (const BreakpointInfo & info)
{
    static constexpr const char * kAccessNames[] = { "BPMR", "BPMW", "BPM" };
    std::string                   range          = (info.last > info.address)
                                                 ? std::format ("{:04X}:{:04X}", info.address, info.last)
                                                 : std::format ("{:04X}", info.address);
    size_t                        op             = info.condition.find_first_of ("<>=!");



    switch (info.kind)
    {
    case BreakpointKind::Address:   return "BP " + range;
    case BreakpointKind::Register:  return "BPR " + info.condition.substr (0, op) + " " + info.condition.substr (op == std::string::npos ? 0 : op);
    case BreakpointKind::Opcode:    return std::format ("BRKOP {:02X}", info.opcode);
    case BreakpointKind::Brk:       return "BRK ON";
    case BreakpointKind::Interrupt: return "BRKINT ON";
    default:                        return std::string (kAccessNames[(int) info.access]) + " " + range + (info.mode == WatchMode::Before ? " BEFORE" : "");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetAddress
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetAddress (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word        last = command.hasA2 ? command.a2 : command.a1;
    int         id   = session.GetBreakpoints().AddAddress (command.a1, last);
    Breakpoint  entry;



    session.OnStopConditionsChanged();
    session.GetBreakpoints().TryFind (id, entry);
    reply.data = BreakpointSetData { MakeInfo (entry) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetCondition
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetCondition (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    int         id = session.GetBreakpoints().AddCondition (command.expression);
    Breakpoint  entry;



    session.OnStopConditionsChanged();
    session.GetBreakpoints().TryFind (id, entry);
    reply.data = BreakpointSetData { MakeInfo (entry) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetWatchpoint
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetWatchpoint (DebugSession & session, const DebugCommand & command, WatchAccess access, Reply & reply)
{
    Word        last = command.hasA2 ? command.a2 : command.a1;
    int         id   = session.GetWatchpoints().Add (access, command.a1, last, GetMode (command));
    Watchpoint  entry;



    session.OnStopConditionsChanged();
    session.GetWatchpoints().TryFind (id, entry);
    reply.data = BreakpointSetData { MakeInfo (entry) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetBoth
//
//  BPA: a breakpoint on the program counter and a watchpoint on the same
//  range, listed together.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetBoth (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Word                last     = command.hasA2 ? command.a2 : command.a1;
    int                 first    = session.GetBreakpoints().AddAddress (command.a1, last);
    int                 second   = session.GetWatchpoints().Add (WatchAccess::ReadWrite, command.a1, last);
    BreakpointListData  list;
    Breakpoint          breakpoint;
    Watchpoint          watchpoint;



    session.OnStopConditionsChanged();
    session.GetBreakpoints().TryFind (first,  breakpoint);
    session.GetWatchpoints().TryFind (second, watchpoint);
    list.breakpoints = { MakeInfo (breakpoint), MakeInfo (watchpoint) };
    reply.data = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetBrk
//
//  BRK [0|1|2|3|ALL] [ON|OFF]. 0 is the BRK opcode; 1, 2 and 3 are the
//  invalid opcodes of that length, each of which becomes an opcode entry.
//  With no ON or OFF the current setting is reported.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetBrk (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    int                  selector    = kBrkSelector;
    std::optional<bool>  isOn;
    std::string          error;
    BreakpointListData   added;
    Breakpoint           entry;
    int                  firstLength = 1;
    int                  lastLength  = kMaxLength;



    if (!TryParseBrkArguments (command.text, selector, isOn, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }

    if (!isOn.has_value())
    {
        ReportBrk (session, reply);
        return;
    }

    if (selector != kAllSelector)
    {
        firstLength = selector;
        lastLength  = selector;
    }

    for (int length = firstLength; length <= lastLength; ++length)
    {
        if (length == kBrkSelector && *isOn && !session.GetBreakpoints().HasBrk())
        {
            session.GetBreakpoints().TryFind (session.GetBreakpoints().AddBrk(), entry);
            added.breakpoints.push_back (MakeInfo (entry));
        }
        else if (length == kBrkSelector && !*isOn)
        {
            session.GetBreakpoints().ClearKind (BreakpointKind::Brk);
        }
        else if (length != kBrkSelector && *isOn)
        {
            AddInvalidOpcodes (session, length, added);
        }
        else if (length != kBrkSelector)
        {
            RemoveInvalidOpcodes (session, length);
        }
    }

    session.OnStopConditionsChanged();

    if (*isOn && added.breakpoints.empty())
    {
        reply.data = MessageData { { "Nothing to add: the setting was already on, or this CPU has no invalid opcodes of that length." } };
    }
    else if (*isOn)
    {
        reply.data = added;
    }
    else
    {
        ReportBrk (session, reply);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::TryParseBrkArguments
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::TryParseBrkArguments (const std::string & text, int & selector, std::optional<bool> & isOn, std::string & error)
{
    std::istringstream  stream (text);
    std::string         token;



    while (stream >> token)
    {
        if      (token == "ON")                                     { isOn = true; }
        else if (token == "OFF")                                    { isOn = false; }
        else if (token == "ALL")                                    { selector = kAllSelector; }
        else if (token.size() == 1 && token[0] >= '0' && token[0] <= '3') { selector = token[0] - '0'; }
        else
        {
            error = "BRK takes 0, 1, 2, 3 or ALL, then ON or OFF.";
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::AddInvalidOpcodes
//
//  One opcode entry per invalid opcode of the length, skipping any already
//  present.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::AddInvalidOpcodes (DebugSession & session, int length, BreakpointListData & added)
{
    Breakpoint  entry;



    for (int opcode = 0; opcode < kOpcodeCount; ++opcode)
    {
        if (!IsInvalidOfLength (session, (Byte) opcode, length) || session.GetBreakpoints().HasOpcode ((Byte) opcode))
        {
            continue;
        }

        session.GetBreakpoints().TryFind (session.GetBreakpoints().AddOpcode ((Byte) opcode), entry);
        added.breakpoints.push_back (MakeInfo (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::RemoveInvalidOpcodes
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::RemoveInvalidOpcodes (DebugSession & session, int length)
{
    std::vector<int>  ids;



    for (const Breakpoint & entry : session.GetBreakpoints().GetAll())
    {
        if (entry.kind == BreakpointKind::Opcode && IsInvalidOfLength (session, entry.opcode, length))
        {
            ids.push_back (entry.id);
        }
    }

    for (int id : ids)
    {
        session.GetBreakpoints().TryClear (id);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::IsInvalidOfLength
//
//  An opcode the CPU's table does not define, whose length the disassembler
//  reports as the given one; an undefined opcode with no length counts as
//  one byte.
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::IsInvalidOfLength (DebugSession & session, Byte opcode, int length)
{
    const Microcode  * set    = session.GetTarget().GetInstructionSet();
    Disassembler       disassembler (set);
    size_t             bytes  = 0;



    if (set == nullptr || set[opcode].isLegal)
    {
        return false;
    }

    bytes = disassembler.GetLength (opcode);
    return (int) std::max<size_t> (bytes, 1) == length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::ReportBrk
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::ReportBrk (DebugSession & session, Reply & reply)
{
    MessageData  message;
    std::string  invalid = "Invalid opcodes:";



    message.lines.push_back (std::string ("BRK opcode: ") + (session.GetBreakpoints().HasBrk() ? "on" : "off"));

    for (int length = 1; length <= kMaxLength; ++length)
    {
        bool  isAnyPresent = false;
        bool  isAnyMissing = false;



        for (int opcode = 0; opcode < kOpcodeCount; ++opcode)
        {
            if (IsInvalidOfLength (session, (Byte) opcode, length))
            {
                isAnyPresent |= session.GetBreakpoints().HasOpcode ((Byte) opcode);
                isAnyMissing |= !session.GetBreakpoints().HasOpcode ((Byte) opcode);
            }
        }

        invalid += std::format (" {}-byte {}", length, isAnyPresent && !isAnyMissing ? "on" : (isAnyPresent ? "partial" : "off"));
    }

    message.lines.push_back (invalid);
    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetOpcode
//
//  BRKOP with opcodes adds an entry for each; alone, it lists the opcode
//  entries.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetOpcode (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    BreakpointListData  list;
    Breakpoint          entry;



    for (Byte opcode : command.values)
    {
        session.GetBreakpoints().TryFind (session.GetBreakpoints().AddOpcode (opcode), entry);
        list.breakpoints.push_back (MakeInfo (entry));
    }

    session.OnStopConditionsChanged();

    if (command.values.empty())
    {
        for (const Breakpoint & existing : session.GetBreakpoints().GetAll())
        {
            if (existing.kind == BreakpointKind::Opcode)
            {
                list.breakpoints.push_back (MakeInfo (existing));
            }
        }
    }

    reply.data = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetInterrupt
//
//  BRKINT [ON|OFF]; alone, it reports the setting.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetInterrupt (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Breakpoint  entry;



    if (command.text == "ON")
    {
        if (!HasInterrupt (session))
        {
            session.GetBreakpoints().AddInterrupt();
            session.OnStopConditionsChanged();
        }

        for (const Breakpoint & existing : session.GetBreakpoints().GetAll())
        {
            if (existing.kind == BreakpointKind::Interrupt)
            {
                entry = existing;
            }
        }

        reply.data = BreakpointSetData { MakeInfo (entry) };
    }
    else if (command.text == "OFF")
    {
        session.GetBreakpoints().ClearKind (BreakpointKind::Interrupt);
        session.OnStopConditionsChanged();
        reply.data = MessageData { { "Break on interrupt: off" } };
    }
    else if (command.text.empty())
    {
        reply.data = MessageData { { std::string ("Break on interrupt: ") + (HasInterrupt (session) ? "on" : "off") } };
    }
    else
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BRKINT takes ON or OFF.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::HasInterrupt
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::HasInterrupt (DebugSession & session)
{
    const std::vector<Breakpoint> & entries = session.GetBreakpoints().GetAll();



    return std::any_of (entries.begin(), entries.end(), [] (const Breakpoint & entry) { return entry.kind == BreakpointKind::Interrupt; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::Clear
//
//  BPC # or BPC *.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::Clear (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    int   id      = (int) command.count;
    bool  isFound = false;



    if (command.text == "*")
    {
        session.ClearAllBreakpoints();
        reply.data = MessageData { { "All breakpoints cleared." } };
        return;
    }

    isFound = session.GetBreakpoints().TryClear (id) || session.GetWatchpoints().TryClear (id);
    session.OnStopConditionsChanged();

    if (!isFound)
    {
        SetNoSuch (reply, id);
        return;
    }

    reply.data = MessageData { { std::format ("Breakpoint #{} cleared.", id) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::Enable
//
//  BPD and BPE, with an id or *. The reply shows the entry, or the list.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::Enable (DebugSession & session, const DebugCommand & command, bool enabled, Reply & reply)
{
    int                 id = (int) command.count;
    BreakpointListData  list;
    BreakpointInfo      info;



    if (command.text == "*")
    {
        for (const Breakpoint & entry : session.GetBreakpoints().GetAll())
        {
            session.GetBreakpoints().TrySetEnabled (entry.id, enabled);
        }

        for (const Watchpoint & entry : session.GetWatchpoints().GetAll())
        {
            session.GetWatchpoints().TrySetEnabled (entry.id, enabled);
        }

        session.OnStopConditionsChanged();
        ListAll (session, list);
        reply.data = list;
        return;
    }

    if (!session.GetBreakpoints().TrySetEnabled (id, enabled) && !session.GetWatchpoints().TrySetEnabled (id, enabled))
    {
        SetNoSuch (reply, id);
        return;
    }

    session.OnStopConditionsChanged();
    TryFindInfo (session, id, info);
    reply.data = BreakpointSetData { info };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::Edit
//
//  BPEDIT # definition: the definition is parsed as its own line and must be
//  one of the setting commands. The old entry is removed and the new one
//  takes its id, enabled state and flags, with its hit count at zero.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::Edit (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    int                  id           = (int) command.count;
    BreakpointInfo       old;
    AppleWinParseResult  parsed;
    Breakpoint           breakpoint;
    Watchpoint           watchpoint;
    bool                 isWatchpoint = false;



    if (command.text == "*" || !TryFindInfo (session, id, old))
    {
        SetNoSuch (reply, id);
        return;
    }

    parsed = AppleWinParser::Parse (command.text, session);

    if (parsed.status != ParseStatus::Ok)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", parsed.error.empty() ? "BPEDIT # takes the breakpoint's new definition." : parsed.error);
        return;
    }

    if (!TryMakeEntry (parsed.command, id, old, breakpoint, watchpoint, isWatchpoint))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments",
                        "BPEDIT # takes one definition as BP, BPX, BPR, BPM, BPMR, BPMW or BRKOP would take it.");
        return;
    }

    session.GetBreakpoints().TryClear (id);
    session.GetWatchpoints().TryClear (id);

    if (isWatchpoint)
    {
        session.GetWatchpoints().TryAdopt (watchpoint);
        reply.data = BreakpointSetData { MakeInfo (watchpoint) };
    }
    else
    {
        session.GetBreakpoints().TryAdopt (breakpoint);
        reply.data = BreakpointSetData { MakeInfo (breakpoint) };
    }

    session.OnStopConditionsChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::TryMakeEntry
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::TryMakeEntry (
    const DebugCommand    & definition,
    int                     id,
    const BreakpointInfo  & old,
    Breakpoint            & breakpoint,
    Watchpoint            & watchpoint,
    bool                  & isWatchpoint)
{
    Word  last = definition.hasA2 ? definition.a2 : definition.a1;



    breakpoint.id        = id;
    breakpoint.enabled   = old.enabled;
    breakpoint.temporary = old.temporary;
    breakpoint.stops     = old.stops;
    watchpoint.id        = id;
    watchpoint.enabled   = old.enabled;
    watchpoint.temporary = old.temporary;
    watchpoint.stops     = old.stops;
    watchpoint.first     = definition.a1;
    watchpoint.last      = last;
    watchpoint.mode      = GetMode (definition);
    isWatchpoint         = false;

    switch (definition.verb)
    {
    case DebugVerb::SetBreakpoint:
        breakpoint.kind  = BreakpointKind::Address;
        breakpoint.first = definition.a1;
        breakpoint.last  = last;
        return true;

    case DebugVerb::SetConditionalBreakpoint:
    case DebugVerb::SetRegisterBreakpoint:
        breakpoint.kind      = BreakpointKind::Register;
        breakpoint.condition = definition.expression;
        return true;

    case DebugVerb::BreakOnOpcode:
        breakpoint.kind   = BreakpointKind::Opcode;
        breakpoint.opcode = definition.values.empty() ? 0 : definition.values[0];
        return definition.values.size() == 1;

    case DebugVerb::SetMemoryWatchpoint: watchpoint.access = WatchAccess::ReadWrite; isWatchpoint = true; return true;
    case DebugVerb::SetReadWatchpoint:   watchpoint.access = WatchAccess::Read;      isWatchpoint = true; return true;
    case DebugVerb::SetWriteWatchpoint:  watchpoint.access = WatchAccess::Write;     isWatchpoint = true; return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::Change
//
//  BPCHANGE # flags: E or e for enabled, T or t for temporary, S or s for
//  whether a hit stops the machine.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::Change (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    int             id = (int) command.count;
    BreakpointInfo  info;



    if (command.text == "*" || !TryFindInfo (session, id, info))
    {
        SetNoSuch (reply, id);
        return;
    }

    for (char flag : command.text)
    {
        switch (flag)
        {
        case 'E': info.enabled   = true;  break;
        case 'e': info.enabled   = false; break;
        case 'T': info.temporary = true;  break;
        case 't': info.temporary = false; break;
        case 'S': info.stops     = true;  break;
        case 's': info.stops     = false; break;
        default:
            reply.SetError (CommandStatus::Error, "invalid arguments", "BPCHANGE # takes flags: E or e, T or t, S or s.");
            return;
        }
    }

    if (!session.GetBreakpoints().TrySetEnabled (id, info.enabled))
    {
        session.GetWatchpoints().TrySetEnabled (id, info.enabled);
        session.GetWatchpoints().TrySetFlags   (id, info.temporary, info.stops);
    }
    else
    {
        session.GetBreakpoints().TrySetFlags (id, info.temporary, info.stops);
    }

    session.OnStopConditionsChanged();
    TryFindInfo (session, id, info);
    reply.data = BreakpointSetData { info };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::Save
//
//  BPSAVE file: a script that clears the table and recreates every entry in
//  id order, then disables and flags entries by their new sequential ids.
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::Save (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    IFileSystem       * files = session.GetFileSystem();
    BreakpointListData  list;
    HRESULT             hr    = S_OK;



    if (command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BPSAVE takes a file name.");
        return;
    }

    if (files == nullptr)
    {
        reply.SetError (CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        return;
    }

    hr = files->WriteAllText (session.ResolvePath (command.text), MakeScript (session));

    if (FAILED (hr))
    {
        reply.SetError (CommandStatus::Error, "file not written", std::format ("{} could not be written.", command.text));
        return;
    }

    ListAll (session, list);
    reply.data = MessageData { { std::format ("Saved {} breakpoints to {}.", list.breakpoints.size(), command.text) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::MakeScript
//
//  A clear line, one definition per entry in id order, then disable and
//  flag lines by the sequential ids the replay produces.
//
////////////////////////////////////////////////////////////////////////////////

std::string BreakpointHandlers::MakeScript (DebugSession & session)
{
    BreakpointListData  list;
    std::string         script = "BPC *\n";
    std::string         flags;



    ListAll (session, list);

    for (const BreakpointInfo & info : list.breakpoints)
    {
        script += MakeDefinition (info) + "\n";
    }

    for (size_t i = 0; i < list.breakpoints.size(); ++i)
    {
        flags  = list.breakpoints[i].temporary ? "T" : "";
        flags += list.breakpoints[i].stops     ? ""  : "s";

        if (!list.breakpoints[i].enabled)
        {
            script += std::format ("BPD {}\n", i);
        }

        if (!flags.empty())
        {
            script += std::format ("BPCHANGE {} {}\n", i, flags);
        }
    }

    return script;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::TryFindInfo
//
////////////////////////////////////////////////////////////////////////////////

bool BreakpointHandlers::TryFindInfo (DebugSession & session, int id, BreakpointInfo & info)
{
    Breakpoint  breakpoint;
    Watchpoint  watchpoint;



    if (session.GetBreakpoints().TryFind (id, breakpoint))
    {
        info = MakeInfo (breakpoint);
        return true;
    }

    if (session.GetWatchpoints().TryFind (id, watchpoint))
    {
        info = MakeInfo (watchpoint);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::SetNoSuch
//
////////////////////////////////////////////////////////////////////////////////

void BreakpointHandlers::SetNoSuch (Reply & reply, int id)
{
    reply.SetError (CommandStatus::Error, "no such breakpoint", std::format ("There is no breakpoint #{}. BPL lists them.", id));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlers::GetMode
//
////////////////////////////////////////////////////////////////////////////////

WatchMode BreakpointHandlers::GetMode (const DebugCommand & command)
{
    return command.text == "BEFORE" ? WatchMode::Before : WatchMode::After;
}
