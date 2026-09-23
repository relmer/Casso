#include "Pch.h"

#include "Ui/Debugger/InstructionTouches.h"

#include "Debugger/EffectiveAddress.h"
#include "Debugger/InstructionSemantics.h"





//  The flags, in the order the pane lists them.
static constexpr std::pair<char, Byte>  s_kFlags[] =
{
    { 'C', InstructionSemantics::kCarry    }, { 'Z', InstructionSemantics::kZero     },
    { 'I', InstructionSemantics::kIrqOff   }, { 'D', InstructionSemantics::kDecimal  },
    { 'V', InstructionSemantics::kOverflow }, { 'N', InstructionSemantics::kNegative },
};





////////////////////////////////////////////////////////////////////////////////
//
//  AddItem
//
//  One register, flag or address, folded into whatever is already listed for
//  it: an address both read and written is one item, not two.
//
////////////////////////////////////////////////////////////////////////////////

static void AddItem (std::vector<InstructionTouches::Item> & items, InstructionTouches::Kind kind,
                     const char * name, Word address, bool isRead, bool isWrite)
{
    for (InstructionTouches::Item & item : items)
    {
        if (item.kind == kind && item.address == address && item.name == (name != nullptr ? name : ""))
        {
            item.isRead  = item.isRead  || isRead;
            item.isWrite = item.isWrite || isWrite;
            return;
        }
    }

    items.push_back (InstructionTouches::Item { kind, (name != nullptr) ? name : "", address, isRead, isWrite });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionTouches::Find
//
////////////////////////////////////////////////////////////////////////////////

InstructionTouches::Result InstructionTouches::Find (const IDebugExpressionContext & memory, const Microcode * instructionSet,
                                                     const Cpu6502Registers & registers, Word pc, Word length)
{
    ShadowCpu                cpu (memory);
    Cpu6502Registers                before     = registers;
    const Microcode               * microcode  = nullptr;
    InstructionSemantics::Effect    semantics;
    Byte                            reads      = 0;
    Byte                            writes     = 0;
    AccessPrediction                prediction;
    Result                          result;
    Byte                            opcode     = 0;
    uint32_t                        cycles     = 0;
    HRESULT                         hr         = S_OK;



    if (instructionSet == nullptr || !memory.TryPeek (pc, opcode))
    {
        return result;
    }

    microcode = &instructionSet[opcode];
    semantics = InstructionSemantics::Find (microcode->operation);
    reads     = semantics.readsFlags;
    writes    = semantics.writesFlags;

    //  PHP reads every flag out and PLP writes every one back; the microcode
    //  rows for those point at the status register itself.
    if (const char * source = cpu.GetSourceRegister (opcode); source != nullptr && std::string (source) == "P")
    {
        reads |= 0xFF;
    }

    if (const char * destination = cpu.GetDestinationRegister (opcode); destination != nullptr && std::string (destination) == "P")
    {
        writes |= 0xFF;
    }

    //  The addresses, from the addressing mode -- the same prediction a
    //  watchpoint uses to know what an instruction is about to touch.
    hr = EffectiveAddress::Predict (instructionSet, pc, registers, memory, prediction);

    if (SUCCEEDED (hr))
    {
        for (const PredictedTouch & touch : prediction.touches)
        {
            AddItem (result.items, Kind::Address, nullptr, touch.address,
                     touch.access != PredictedAccess::Write, touch.access != PredictedAccess::Read);
        }
    }

    //  The registers, from the microcode's own source and destination. An
    //  operation that works on a register rather than on memory -- INX, ASL A
    //  -- puts its answer back where it came from, though only one pointer
    //  says so. The same row carries that pointer for ASL $0400, where memory
    //  is the target and the pointer means nothing.
    if (const char * source = cpu.GetSourceRegister (opcode))
    {
        bool  isTarget = InstructionSemantics::IsRegisterOperation (microcode->operation);
        bool  onMemory = isTarget && microcode->globalAddressingMode != GlobalAddressingMode::Accumulator &&
                                     microcode->globalAddressingMode != GlobalAddressingMode::SingleByteNoOperand;

        if (!onMemory)
        {
            AddItem (result.items, Kind::Register, source, 0, true, isTarget);
        }
    }

    if (const char * destination = cpu.GetDestinationRegister (opcode))
    {
        AddItem (result.items, Kind::Register, destination, 0, false, true);
    }

    //  The accumulator an ALU operation uses without its row saying so.
    if (semantics.readsAccumulator || semantics.writesAccumulator)
    {
        AddItem (result.items, Kind::Register, "A", 0, semantics.readsAccumulator, semantics.writesAccumulator);
    }

    switch (microcode->globalAddressingMode)
    {
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::AbsoluteXIndirect:
        AddItem (result.items, Kind::Register, "X", 0, true, false);
        break;

    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::ZeroPageIndirectY:
        AddItem (result.items, Kind::Register, "Y", 0, true, false);
        break;

    case GlobalAddressingMode::Accumulator:
        AddItem (result.items, Kind::Register, "A", 0, true, true);
        break;

    default:
        break;
    }

    //  Two operations hold the flag they act on in the OPCODE rather than in
    //  the table: a branch tests one, and CLC, SEC and their kin set or clear
    //  one. The opcode's own bits say which.
    if (microcode->operation == Microcode::Branch)
    {
        static constexpr Byte  kBranchFlags[] = { InstructionSemantics::kNegative, InstructionSemantics::kOverflow,
                                                  InstructionSemantics::kCarry,    InstructionSemantics::kZero };

        reads |= kBranchFlags[Instruction (opcode).asBranch.flag];
    }

    if (microcode->operation == Microcode::SetFlag)
    {
        static constexpr std::pair<Byte, Byte>  kSetFlags[] =
        {
            { 0x18, InstructionSemantics::kCarry   }, { 0x38, InstructionSemantics::kCarry   },
            { 0x58, InstructionSemantics::kIrqOff  }, { 0x78, InstructionSemantics::kIrqOff  },
            { 0xB8, InstructionSemantics::kOverflow },
            { 0xD8, InstructionSemantics::kDecimal }, { 0xF8, InstructionSemantics::kDecimal },
        };

        for (const auto & [which, bit] : kSetFlags)
        {
            writes |= (opcode == which) ? bit : (Byte) 0;
        }
    }

    for (const auto & [letter, bit] : s_kFlags)
    {
        if (((reads | writes) & bit) != 0)
        {
            AddItem (result.items, Kind::Flag, std::string (1, letter).c_str(), 0,
                     (reads & bit) != 0, (writes & bit) != 0);
        }
    }

    //  And what it would leave behind, from the core itself.
    before.pc = pc;
    cpu.SetRegisters (before);

    hr = cpu.Step (cycles);

    if (SUCCEEDED (hr) && cpu.IsComplete())
    {
        result.isKnown = true;
        result.after   = cpu.GetRegisters();
        result.writes  = cpu.GetWrites();
    }

    UNREFERENCED_PARAMETER (length);

    return result;
}
