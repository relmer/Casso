#include "Pch.h"

#include "Ui/Debugger/InstructionLookup.h"

#include "Debugger/DebugSession.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup::GetInstructionAt
//
//  The instruction that starts at an address, as the disassembly lists it,
//  or none for an I/O address, which a read would operate.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DisassemblyLine> InstructionLookup::GetInstructionAt (DebugSession & session, Word address)
{
    DisassemblyData  data;



    if (session.GetTarget().GetRegion (address) == MemoryRegion::Io)
    {
        return std::nullopt;
    }

    (void) DataDirectiveHandlers::Disassemble (session, address, address, 1, data);

    if (data.lines.empty() || data.lines.front().instruction.address != address)
    {
        return std::nullopt;
    }

    return data.lines.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup::GetInstructionLength
//
////////////////////////////////////////////////////////////////////////////////

Word InstructionLookup::GetInstructionLength (DebugSession & session, Word address)
{
    DisassemblyData  data;



    (void) DataDirectiveHandlers::Disassemble (session, address, std::nullopt, 1, data);

    if (data.lines.empty() || data.lines[0].instruction.bytes.empty())
    {
        return 1;
    }

    return (Word) data.lines[0].instruction.bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup::GetPreviousInstruction
//
//  Code cannot be disassembled backward with certainty, so this takes the
//  longest instruction that ends exactly where the given one starts, and one
//  byte back when none does. A data block's lines are known: they run from
//  the block's start, each as long as its first.
//
////////////////////////////////////////////////////////////////////////////////

Word InstructionLookup::GetPreviousInstruction (DebugSession & session, Word address)
{
    Word            before = (Word) (address - 1);
    DataBlockEntry  block;
    DataOrigin      origin = DataOrigin::None;
    Word            length = 0;



    if (session.TryFindData (before, block, origin))
    {
        length = GetInstructionLength (session, block.first);
        return (Word) (block.first + (before - block.first) / length * length);
    }

    for (Word back = 3; back >= 1; back--)
    {
        if (GetInstructionLength (session, (Word) (address - back)) == back)
        {
            return (Word) (address - back);
        }
    }

    return (Word) (address - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup::GetReturnAddress
//
//  JSR pushes the address of its own last byte, so RTS returns one past what
//  the stack holds.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> InstructionLookup::GetReturnAddress (DebugSession & session)
{
    const IDebugTarget  & target = session.GetTarget();
    Byte                  s      = target.GetRegisters().sp;
    Byte                  low    = 0;
    Byte                  high   = 0;



    if (!target.TryPeek ((Word) (0x0100 + (Byte) (s + 1)), low) ||
        !target.TryPeek ((Word) (0x0100 + (Byte) (s + 2)), high))
    {
        return std::nullopt;
    }

    return (Word) (((high << 8) | low) + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionLookup::GetOperandAddress
//
//  A branch, jump or absolute operand is disassembled with four hex digits; a
//  zero-page or immediate operand has two, and is not taken as an address.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> InstructionLookup::GetOperandAddress (DebugSession & session, Word address)
{
    DisassemblyData  data;
    size_t           dollar  = std::string::npos;
    unsigned         operand = 0;



    (void) DataDirectiveHandlers::Disassemble (session, address, std::nullopt, 1, data);

    if (data.lines.empty())
    {
        return std::nullopt;
    }

    if (data.lines[0].instruction.hasTarget)
    {
        return data.lines[0].instruction.target;
    }

    const std::string & text = data.lines[0].instruction.operand;

    dollar = text.find ('$');

    if (dollar == std::string::npos || text.size() < dollar + 5 ||
        std::from_chars (text.data() + dollar + 1, text.data() + dollar + 5, operand, 16).ptr != text.data() + dollar + 5)
    {
        return std::nullopt;
    }

    return (Word) operand;
}
