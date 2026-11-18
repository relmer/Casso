#include "Pch.h"

#include "Ui/Debugger/CodeAnnotations.h"
#include "Ui/Debugger/InstructionEffect.h"

#include "Debugger/DebugSession.h"
#include "Debugger/SymbolDescriptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations::GetRegisterByte
//
//  The byte a register item stands for: A, X, Y, the status register P, or
//  the stack pointer S.
//
////////////////////////////////////////////////////////////////////////////////

Byte CodeAnnotations::GetRegisterByte (const std::string & name, const Cpu6502Registers & registers)
{
    if (name == "A") { return registers.a; }
    if (name == "X") { return registers.x; }
    if (name == "Y") { return registers.y; }
    if (name == "P") { return registers.p; }

    return registers.sp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations::GetAnnotation
//
//  What the instruction acts on, in the manner of AppleWin's and VICE's
//  monitors: the registers and flags it reads, the address it would touch
//  with the registers as they stand, and the byte there -- `X=02 $067B=A0
//  C=1`.
//
//  EVERY PART OF THAT COMES FROM InstructionTouches, which asks the core
//  itself. A branch shows the flag it tests, and a compare the register it
//  compares, because running the instruction with that input changed lands
//  it somewhere else -- not because a table here says so.
//
//  An I/O address shows what the switch does instead of a byte: the byte
//  does not exist until a read happens, and that read operates the machine
//  (FR-112).
//
////////////////////////////////////////////////////////////////////////////////

std::string CodeAnnotations::GetAnnotation (DebugSession & session, const DisassemblyLine & line,
                                            const Cpu6502Registers & registers, const InstructionTouches::Result & touches)
{
    return ComposeAnnotation (GetAnnotationParts (session, line, registers, touches), true, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations::GetAnnotationParts
//
//  The parts GetAnnotation joins. The pointer is an indirect mode's: the
//  first address the instruction touches, and the address stored there.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot::AnnotationParts CodeAnnotations::GetAnnotationParts (DebugSession & session, const DisassemblyLine & line,
                                                                           const Cpu6502Registers & registers, const InstructionTouches::Result & touches)
{
    static constexpr Word                  kZeroPageLast = 0x00FF;
    DebuggerViewSnapshot::AnnotationParts  parts;
    bool                                   isIo          = false;
    bool                                   isIndirect    = line.instruction.operand.starts_with ('(');
    bool                                   hasPointer    = false;
    Byte                                   value         = 0;
    Byte                                   low           = 0;
    Byte                                   high          = 0;
    Word                                   pointer       = 0;
    Word                                   next          = 0;



    for (const InstructionTouches::Item & item : touches.items)
    {
        if (!item.isRead)
        {
            continue;
        }

        if (item.kind == InstructionTouches::Kind::Register)
        {
            Byte  held = GetRegisterByte (item.name, registers);


            parts.registers += std::format ("{}{}={:02X}", parts.registers.empty() ? "" : " ", item.name, held);
        }
        else if (item.kind == InstructionTouches::Kind::Flag)
        {
            static constexpr std::pair<char, Byte>  kBits[] =
            {
                { 'C', 0x01 }, { 'Z', 0x02 }, { 'I', 0x04 }, { 'D', 0x08 }, { 'V', 0x40 }, { 'N', 0x80 },
            };

            for (const auto & [letter, bit] : kBits)
            {
                if (item.name[0] == letter)
                {
                    parts.flags += std::format ("{}{}={}", parts.flags.empty() ? "" : " ", letter, (registers.p & bit) ? 1 : 0);
                }
            }
        }
    }

    //  The address the instruction works on is the last one it touched: an
    //  indirect mode reads its pointer first, and the pointer is a part of
    //  its own.
    for (const InstructionTouches::Item & item : touches.items)
    {
        if (item.kind != InstructionTouches::Kind::Address)
        {
            continue;
        }

        if (isIndirect && !hasPointer)
        {
            hasPointer = true;
            pointer    = item.address;
        }

        isIo = session.GetTarget().GetRegion (item.address) == MemoryRegion::Io;

        //  An indexed operand can address a switch other than the one at its
        //  symbol, so the description is the touched address's, for the
        //  direction of the access.
        if (isIo)
        {
            std::vector<std::string>  names;
            std::string               action;

            session.GetSymbols().FindNames (item.address, names);

            action             = SymbolDescriptions::GetAction (SymbolDescriptions::ChooseByDirection (names, item.isWrite));
            parts.operandValue = action.empty() ? std::format ("${:04X}", item.address) : action;
        }
        else if (session.TryPeek (item.address, value))
        {
            parts.operandValue = std::format ("${:04X}={:02X}", item.address, value);
        }
        else
        {
            parts.operandValue = std::format ("${:04X}", item.address);
        }
    }

    //  A zero-page pointer's high byte wraps within the zero page, as the
    //  6502 reads it.
    next = (pointer <= kZeroPageLast) ? (Word) ((pointer + 1) & kZeroPageLast) : (Word) (pointer + 1);

    if (hasPointer && session.TryPeek (pointer, low) && session.TryPeek (next, high))
    {
        parts.pointer = std::format ("(${:04X})=${:04X}", pointer, (Word) (low | (high << 8)));
    }

    //  A named location in memory says what it is after its byte, so
    //  INC RNDL reads $004E=96; random seed, low byte .... An I/O
    //  address already said it in place of the byte.
    parts.about = isIo ? std::string() : SymbolDescriptions::GetAction (line.operandSymbol);

    if (parts.about.size() > 1 && isupper ((unsigned char) parts.about[0]) && islower ((unsigned char) parts.about[1]))
    {
        parts.about[0] = (char) tolower ((unsigned char) parts.about[0]);
    }

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations::ComposeAnnotation
//
//  The registers, the pointer, the operand's value and the flags, a space
//  apart, then the description of a location with a symbol after a semicolon.
//
////////////////////////////////////////////////////////////////////////////////

std::string CodeAnnotations::ComposeAnnotation (const DebuggerViewSnapshot::AnnotationParts & parts, bool withOperandValue, bool withPointer)
{
    std::string  text;



    for (const std::string * part : { &parts.registers,
                                      withPointer      ? &parts.pointer      : nullptr,
                                      withOperandValue ? &parts.operandValue : nullptr,
                                      &parts.flags })
    {
        if (part != nullptr && !part->empty())
        {
            text += (text.empty() ? "" : " ") + *part;
        }
    }

    if (!parts.about.empty())
    {
        text += (text.empty() ? "" : "; ") + parts.about;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CodeAnnotations::GetEffect
//
//  What running the PC's instruction would leave behind (FR-107), written
//  from the run InstructionTouches already made (FR-111), and the cycles
//  that run took (FR-202).
//
////////////////////////////////////////////////////////////////////////////////

std::string CodeAnnotations::GetEffect (DebugSession & session, const Cpu6502Registers & registers,
                                        const InstructionTouches::Result & touches, Word next)
{
    //  A write to a soft switch stores no byte: it operates the machine, and
    //  what it operates is what the line should say (FR-112).
    auto  describeWrite = [&session] (Word address) -> std::string
    {
        std::vector<std::string>  names;

        if (session.GetTarget().GetRegion (address) != MemoryRegion::Io)
        {
            return {};
        }

        session.GetSymbols().FindNames (address, names);

        return SymbolDescriptions::GetAction (SymbolDescriptions::ChooseByDirection (names, true));
    };
    std::string  effect;



    if (!touches.isKnown)
    {
        return {};
    }

    //  The cycles end the line. The trial run counted them with the registers
    //  as they are, so a branch's count is for the way it goes.
    effect  = InstructionEffect::Format (registers, touches.after, touches.writes, next, describeWrite, touches.unknowns);
    effect += std::format ("{}{} cycles", effect.empty() ? "" : "; ", touches.cycles);

    return effect;
}
