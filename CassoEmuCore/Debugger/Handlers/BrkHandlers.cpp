#include "Pch.h"

#include "Debugger/Handlers/BrkHandlers.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/DebugSession.h"
#include "Microcode.h"
#include "OpcodeTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::SetBrk
//
//  BRK [0|1|2|3|ALL] [ON|OFF]. 0 is the BRK opcode; 1, 2 and 3 are the
//  invalid opcodes of that length, each of which becomes an opcode entry.
//  With no ON or OFF the current setting is reported.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::SetBrk (DebugSession & session, const DebugCommand & command, Reply & reply)
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

    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

    if (!isOn.has_value())
    {
        ReportBrk (session, reply);
        return;
    }

    if (selector == kAllSelector)
    {
        firstLength = kBrkSelector;
    }
    else
    {
        firstLength = selector;
        lastLength  = selector;
    }

    for (int length = firstLength; length <= lastLength; ++length)
    {
        if (length == kBrkSelector && *isOn && !session.GetBreakpoints().HasBrk())
        {
            session.GetBreakpoints().TryFind (session.GetBreakpoints().AddBrk(), entry);
            session.GetBreakpoints().TrySetCondition (entry.id, command.expression);
            entry.condition = command.expression;
            added.breakpoints.push_back (BreakpointHandlers::MakeInfo (entry));
        }
        else if (length == kBrkSelector && *isOn)
        {
            //  BRK ON again, with or without IF, sets the condition, and a
            //  condition that changed is reported with the entry.
            for (const Breakpoint & existing : session.GetBreakpoints().GetAll())
            {
                entry = (existing.kind == BreakpointKind::Brk) ? existing : entry;
            }

            if (entry.condition.text != command.expression.text)
            {
                session.GetBreakpoints().TrySetCondition (entry.id, command.expression);
                entry.condition = command.expression;
                added.breakpoints.push_back (BreakpointHandlers::MakeInfo (entry));
            }
        }
        else if (length == kBrkSelector && !*isOn)
        {
            session.GetBreakpoints().ClearKind (BreakpointKind::Brk);
            RemoveBrkOpcodes (session);
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
//  BrkHandlers::TryParseBrkArguments
//
////////////////////////////////////////////////////////////////////////////////

bool BrkHandlers::TryParseBrkArguments (const std::string & text, int & selector, std::optional<bool> & isOn, std::string & error)
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
//  BrkHandlers::AddInvalidOpcodes
//
//  One opcode entry per invalid opcode of the length, skipping any already
//  present.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::AddInvalidOpcodes (DebugSession & session, int length, BreakpointListData & added)
{
    Breakpoint  entry;



    for (int opcode = 0; opcode < kOpcodeCount; ++opcode)
    {
        if (!IsInvalidOfLength (session, (Byte) opcode, length) || session.GetBreakpoints().HasOpcode ((Byte) opcode))
        {
            continue;
        }

        session.GetBreakpoints().TryFind (session.GetBreakpoints().AddOpcode ((Byte) opcode), entry);
        added.breakpoints.push_back (BreakpointHandlers::MakeInfo (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::RemoveInvalidOpcodes
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::RemoveInvalidOpcodes (DebugSession & session, int length)
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
//  BrkHandlers::RemoveBrkOpcodes
//
//  Opcode entries on BRK stop exactly as the BRK entry does, so BRK 0 OFF
//  clears them too.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::RemoveBrkOpcodes (DebugSession & session)
{
    std::vector<int>  ids;



    for (const Breakpoint & entry : session.GetBreakpoints().GetAll())
    {
        if (entry.kind == BreakpointKind::Opcode && entry.opcode == kBrkOpcode)
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
//  BrkHandlers::IsInvalidOfLength
//
//  An undocumented opcode of the given length on the target's CPU.
//
////////////////////////////////////////////////////////////////////////////////

bool BrkHandlers::IsInvalidOfLength (DebugSession & session, Byte opcode, int length)
{
    const Microcode  * set = session.GetTarget().GetInstructionSet();



    if (set == nullptr)
    {
        return false;
    }

    return GetInvalidLength (set, opcode) == length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::GetInvalidLength
//
//  The length of an undocumented opcode, or 0 for a documented one.
//
//  An opcode the table executes but hides from the assembler (the stable
//  NMOS undocumented opcodes, the 65C02's NOP fill) takes the length of its
//  addressing mode. Of the opcodes the table leaves undefined, those in
//  column 2 are the NMOS JAMs, which halt the CPU and count as one byte. The
//  rest are the unstable NMOS opcodes: they decode their operand the way the
//  ALU instruction in the same row and column group does ($9B as $99, $AB
//  as $A9), so they take that instruction's length.
//
////////////////////////////////////////////////////////////////////////////////

int BrkHandlers::GetInvalidLength (const Microcode * set, Byte opcode)
{
    constexpr Byte     kColumnMask = 0x0F;
    constexpr Byte     kJamColumn  = 0x02;
    constexpr Byte     kGroupMask  = 0x03;
    constexpr Byte     kAluGroup   = 0x01;
    const Microcode  & entry       = set[opcode];
    Byte               sibling     = (Byte) ((opcode & ~kGroupMask) | kAluGroup);



    if (entry.isLegal)
    {
        return entry.assemblerHidden ? 1 + OpcodeTable::GetOperandSize (entry.globalAddressingMode) : 0;
    }

    if ((opcode & kColumnMask) == kJamColumn || !set[sibling].isLegal)
    {
        return 1;
    }

    return 1 + OpcodeTable::GetOperandSize (set[sibling].globalAddressingMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::ReportBrk
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::ReportBrk (DebugSession & session, Reply & reply)
{
    MessageData  message;
    std::string  invalid = "Invalid opcodes:";



    message.lines.push_back (std::string ("BRK opcode: ") + (session.GetBreakpoints().HasBrk() || session.GetBreakpoints().HasOpcode (kBrkOpcode) ? "on" : "off"));

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
//  BrkHandlers::SetOpcode
//
//  BRKOP with opcodes, or mnemonics, adds an entry for each opcode, every
//  opcode of a mnemonic on this CPU; alone, it lists the opcode entries, or
//  reports that there are none. An unknown mnemonic adds nothing.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::SetOpcode (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    BreakpointListData        list;
    std::istringstream        mnemonics (command.text);
    std::string               mnemonic;
    std::vector<Byte>         opcodes   = command.values;
    std::vector<Byte>         found;



    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

    while (mnemonics >> mnemonic)
    {
        if (!TryFindOpcodes (session, mnemonic, found))
        {
            reply.SetError (CommandStatus::Error, "unknown mnemonic", std::format ("{} is not an instruction of this CPU.", mnemonic));
            return;
        }

        opcodes.insert (opcodes.end(), found.begin(), found.end());
    }

    std::ranges::sort (opcodes);
    opcodes.erase (std::ranges::unique (opcodes).begin(), opcodes.end());

    for (Byte opcode : opcodes)
    {
        AddOpcodeEntry (session, opcode, command.expression, list);
    }

    session.OnStopConditionsChanged();

    if (opcodes.empty())
    {
        for (const Breakpoint & existing : session.GetBreakpoints().GetAll())
        {
            if (existing.kind == BreakpointKind::Opcode)
            {
                list.breakpoints.push_back (BreakpointHandlers::MakeInfo (existing));
            }
        }

        if (list.breakpoints.empty())
        {
            reply.data = MessageData { { "No opcode breakpoints." } };
            return;
        }
    }

    reply.data = list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::AddOpcodeEntry
//
//  An opcode's entry, added unless one is there; the BRK opcode is the BRK
//  breakpoint, so BRKOP 00 and BRK ON make the same one entry.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::AddOpcodeEntry (DebugSession & session, Byte opcode, const Expression & condition, BreakpointListData & list)
{
    BreakpointTable  & table = session.GetBreakpoints();
    Breakpoint         entry;
    int                id    = -1;



    for (const Breakpoint & existing : table.GetAll())
    {
        bool  isSame = (opcode == kBrkOpcode) ? existing.kind == BreakpointKind::Brk
                                              : existing.kind == BreakpointKind::Opcode && existing.opcode == opcode;

        id = (isSame && id < 0) ? existing.id : id;
    }

    if (id < 0)
    {
        id = (opcode == kBrkOpcode) ? table.AddBrk() : table.AddOpcode (opcode);
    }

    table.TrySetCondition (id, condition);

    if (table.TryFind (id, entry))
    {
        list.breakpoints.push_back (BreakpointHandlers::MakeInfo (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::TryFindOpcodes
//
//  Every opcode whose instruction is the mnemonic on the target's CPU,
//  ignoring case; false when there is none.
//
////////////////////////////////////////////////////////////////////////////////

bool BrkHandlers::TryFindOpcodes (DebugSession & session, const std::string & mnemonic, std::vector<Byte> & opcodes)
{
    const Microcode  * instructionSet = session.GetTarget().GetInstructionSet();
    int                opcode         = 0;



    opcodes.clear();

    for (opcode = 0; instructionSet != nullptr && opcode < kOpcodeCount; opcode++)
    {
        const char  * name = instructionSet[opcode].instructionName;

        if (name != nullptr && _stricmp (name, mnemonic.c_str()) == 0)
        {
            opcodes.push_back ((Byte) opcode);
        }
    }

    return !opcodes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::SetInterrupt
//
//  BRKINT [ON|OFF]; alone, it reports the setting.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::SetInterrupt (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    Breakpoint  entry;



    if (!BreakpointHandlers::TryValidateCondition (session, command.expression, false, false, reply))
    {
        return;
    }

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

        session.GetBreakpoints().TrySetCondition (entry.id, command.expression);
        entry.condition = command.expression;

        reply.data = BreakpointSetData { BreakpointHandlers::MakeInfo (entry) };
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
//  BrkHandlers::HasInterrupt
//
////////////////////////////////////////////////////////////////////////////////

bool BrkHandlers::HasInterrupt (DebugSession & session)
{
    const std::vector<Breakpoint> & entries = session.GetBreakpoints().GetAll();



    return std::any_of (entries.begin(), entries.end(), [] (const Breakpoint & entry) { return entry.kind == BreakpointKind::Interrupt; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::SetUnwritten
//
//  BRKUNINIT [ON|OFF]: stop after a read of RAM nothing has written since
//  power-on. Each form reports the setting and what the heat map has seen.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::SetUnwritten (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    bool  isOn  = command.text == "ON";
    bool  isOff = command.text == "OFF";



    if (!isOn && !isOff && !command.text.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", "BRKUNINIT takes ON or OFF.");
        return;
    }

    if (isOn || isOff)
    {
        session.SetUnwrittenBreak (isOn);
    }

    ReportUnwritten (session, reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::ReportUnwritten
//
//  The setting; whether the heat map knows which RAM has been written, which
//  it learns only at a power cycle unless it was on from power-on; and the
//  reads before written it counted outside the ranges left out.
//
////////////////////////////////////////////////////////////////////////////////

void BrkHandlers::ReportUnwritten (DebugSession & session, Reply & reply)
{
    HeatUnwrittenStatus  status  = session.GetTarget().GetUnwrittenStatus();
    MessageData          message;
    std::string          counted;



    message.lines.push_back (std::string ("Break on read before written: ") + (session.HasUnwrittenBreak() ? "on" : "off"));

    if (!status.isOn)
    {
        message.lines.push_back ("The heat map is off, so reads aren't tracked.");
    }
    else if (!status.isTracking)
    {
        message.lines.push_back ("Not tracking yet: the heat map started after power-on. Power-cycle the machine to start tracking.");
    }
    else
    {
        counted = std::format ("Reads before written counted: {} at {} address{}", status.reads, status.addresses, status.addresses == 1 ? "" : "es");

        if (status.ignoredSpans > 0)
        {
            counted += std::format (", outside {} range{} left out", status.ignoredSpans, status.ignoredSpans == 1 ? "" : "s");
        }

        message.lines.push_back (counted + ".");
    }

    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BrkHandlers::IsBrkOn
//
//  BRK's arguments as a definition: ON, alone or after the BRK selector 0.
//  Any other selector adds an entry per invalid opcode, and a definition
//  replaces exactly one entry.
//
////////////////////////////////////////////////////////////////////////////////

bool BrkHandlers::IsBrkOn (const std::string & text)
{
    int                  selector = kBrkSelector;
    std::optional<bool>  isOn;
    std::string          error;



    if (!TryParseBrkArguments (text, selector, isOn, error))
    {
        return false;
    }

    return selector == kBrkSelector && isOn == true;
}
