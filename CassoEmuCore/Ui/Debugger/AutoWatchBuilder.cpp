#include "Pch.h"

#include "Ui/Debugger/AutoWatchBuilder.h"
#include "Ui/Debugger/CodeAnnotations.h"
#include "Ui/Debugger/InstructionLookup.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AutoWatchBuilder::UpdateLastStop
//
//  A new stop. The instruction that just ran is known only when this stop
//  is exactly where the last one's instruction would have put the machine,
//  which is what a step looks like; after a free run it is not known, and
//  nothing is marked rather than the wrong instruction.
//
//  The branch that brought the machine to the PC, by a step or a run that
//  stopped, is the CPU's last transfer when that is a branch, jump or call
//  whose target is the PC: PC moves on in order from a transfer's target,
//  so the instruction at the target runs only after a transfer to it. The
//  snapshot has its address, so the window draws its arrow whether or not
//  its line is in a view. The same is true of the branch, jump or call at
//  the PC, and its target.
//
////////////////////////////////////////////////////////////////////////////////

void AutoWatchBuilder::UpdateLastStop (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    const Cpu6502Registers      & now     = session.GetTarget().GetRegisters();
    Word                          length  = InstructionLookup::GetInstructionLength (session, now.pc);
    InstructionTouches::Result    current = InstructionTouches::Find (session, session.GetTarget().GetInstructionSet(), now, now.pc, length);
    bool                          isStep  = false;
    Word                          from    = 0;
    DisassemblyData               transfer;
    DisassemblyData               atPc;



    if (!m_lastStop.isValid || !IsSameRegisters (m_lastStop.at, now))
    {
        isStep = m_lastStop.isValid && m_lastStop.here.isKnown && IsSameRegisters (m_lastStop.here.after, now);

        m_lastStop.previous = isStep ? std::optional<InstructionTouches::Result> (m_lastStop.here) : std::nullopt;
        m_lastStop.at       = now;
        m_lastStop.isValid  = true;
    }

    //  At the same stop, memory the debugger edited can change what the
    //  instruction does, so it is worked out again on every build.
    m_lastStop.here = current;

    if (session.GetTarget().TryGetLastBranch (from))
    {
        (void) DataDirectiveHandlers::Disassemble (session, from, std::nullopt, 1, transfer);
    }

    if (!transfer.lines.empty() && transfer.lines.front().instruction.hasTarget && transfer.lines.front().instruction.target == now.pc)
    {
        snapshot.branchedFrom = from;
    }

    (void) DataDirectiveHandlers::Disassemble (session, now.pc, std::nullopt, 1, atPc);

    if (!atPc.lines.empty() && atPc.lines.front().dataOrigin == DataOrigin::None && atPc.lines.front().instruction.hasTarget)
    {
        snapshot.pcTarget        = atPc.lines.front().instruction.target;
        snapshot.isPcTargetTaken = !current.isKnown || current.after.pc == *snapshot.pcTarget;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoWatchBuilder::BuildAutoWatches
//
//  What the instruction at the PC touches, then what the one just executed
//  touched that this one does not (FR-095). UpdateLastStop works both out
//  with InstructionTouches, the same account the code pane annotates from.
//
////////////////////////////////////////////////////////////////////////////////

void AutoWatchBuilder::BuildAutoWatches (DebugSession & session, DebuggerViewSnapshot & snapshot) const
{
    const Cpu6502Registers  & now = session.GetTarget().GetRegisters();



    AddAutoWatches (session, m_lastStop.here, now, false, snapshot.autoWatches);

    if (m_lastStop.previous.has_value())
    {
        AddAutoWatches (session, *m_lastStop.previous, now, true, snapshot.autoWatches);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoWatchBuilder::IsSameRegisters
//
////////////////////////////////////////////////////////////////////////////////

bool AutoWatchBuilder::IsSameRegisters (const Cpu6502Registers & left, const Cpu6502Registers & right)
{
    return left.pc == right.pc && left.a == right.a && left.x == right.x &&
           left.y  == right.y  && left.sp == right.sp && left.p == right.p;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoWatchBuilder::AddAutoWatches
//
//  Each register, flag and address the instruction touches, with what it holds
//  NOW. Something already listed for the current instruction is not listed
//  again for the previous one.
//
////////////////////////////////////////////////////////////////////////////////

void AutoWatchBuilder::AddAutoWatches (DebugSession & session, const InstructionTouches::Result & touches,
                                        const Cpu6502Registers & now, bool isPrevious,
                                        std::vector<DebuggerViewSnapshot::AutoWatchLine> & lines)
{
    static constexpr std::pair<char, Byte>  kFlagBits[] =
    {
        { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
    };



    for (const InstructionTouches::Item & item : touches.items)
    {
        DebuggerViewSnapshot::AutoWatchLine  line;
        std::string                          symbol;
        SymbolTableId                        table = SymbolTableId::Main;
        Byte                                 value = 0;



        line.isRead     = item.isRead;
        line.isWrite    = item.isWrite;
        line.isPrevious = isPrevious;

        if (item.kind == InstructionTouches::Kind::Register)
        {
            Byte  held = CodeAnnotations::GetRegisterByte (item.name, now);

            line.key   = "R:" + item.name;
            line.label = item.name;
            line.value = std::format ("{:02X}", held);
        }
        else if (item.kind == InstructionTouches::Kind::Flag)
        {
            line.key   = "F:" + item.name;
            line.label = item.name;

            for (const auto & [letter, bit] : kFlagBits)
            {
                if (item.name[0] == letter)
                {
                    line.value = (now.p & bit) ? "1" : "0";
                }
            }
        }
        else
        {
            session.GetSymbols().TryFindName (item.address, symbol, table);

            line.key   = std::format ("M:{:04X}", item.address);
            line.label = symbol.empty() ? std::format ("${:04X}", item.address) : std::format ("{} ${:04X}", symbol, item.address);

            //  A soft switch has no value to read without operating it.
            line.value = (session.GetTarget().GetRegion (item.address) == MemoryRegion::Io || !session.TryPeek (item.address, value))
                         ? std::string ("--") : std::format ("{:02X}", value);
        }

        if (std::none_of (lines.begin(), lines.end(),
                          [&line] (const DebuggerViewSnapshot::AutoWatchLine & other) { return other.key == line.key; }))
        {
            lines.push_back (std::move (line));
        }
    }
}
