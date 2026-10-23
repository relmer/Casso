#include "Pch.h"

#include "Ui/Debugger/InstructionTouches.h"

#include "Debugger/EffectiveAddress.h"
#include "Debugger/InstructionSemantics.h"
#include "Debugger/IDebugExpressionContext.h"





//  The flags, in the order the pane lists them.
static constexpr std::pair<char, Byte>  s_kFlags[] =
{
    { 'C', InstructionSemantics::kCarry    }, { 'Z', InstructionSemantics::kZero     },
    { 'I', InstructionSemantics::kIrqOff   }, { 'D', InstructionSemantics::kDecimal  },
    { 'V', InstructionSemantics::kOverflow }, { 'N', InstructionSemantics::kNegative },
};

//  The bytes a read with no peek returns, one run each: between them every
//  bit is both 0 and 1, so anything the byte feeds comes out different.
static constexpr Byte  s_kFills[] = { 0x00, 0xFF, 0x55, 0xAA };





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
    ShadowCpu                cpu (memory, instructionSet);
    Cpu6502Registers                before     = registers;
    const Microcode               * microcode  = nullptr;
    InstructionSemantics::Effect    semantics;
    Byte                            reads      = 0;
    Byte                            writes     = 0;
    AccessPrediction                prediction;
    Result                          result;
    Trial                           first;
    Trial                           other;
    Byte                            opcode     = 0;
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

    //  And the registers and memory after it, from the core itself. With a
    //  read that has no peek, the instruction is run again with each other
    //  fill value, and whatever comes out different between the runs is
    //  unknown: the core's own execution determines what the byte feeds.
    before.pc = pc;

    hr = RunTrial (memory, instructionSet, before, s_kFills[0], first);

    if (SUCCEEDED (hr))
    {
        result.isKnown = true;
        result.after   = first.after;
        result.writes  = first.writes;
        result.cycles  = first.cycles;
    }

    for (size_t i = 1; SUCCEEDED (hr) && first.hasUnknownRead && i < std::size (s_kFills); i++)
    {
        hr = RunTrial (memory, instructionSet, before, s_kFills[i], other);

        if (SUCCEEDED (hr))
        {
            MarkUnknown (first, other, result);
        }
    }

    UNREFERENCED_PARAMETER (length);

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionTouches::RunTrial
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InstructionTouches::RunTrial (const IDebugExpressionContext & memory, const Microcode * instructionSet, const Cpu6502Registers & registers, Byte fill, Trial & trial)
{
    HRESULT    hr  = S_OK;
    ShadowCpu  cpu (memory, instructionSet);



    cpu.SetUnknownReadValue (fill);
    cpu.SetRegisters        (registers);

    hr = cpu.Step (trial.cycles);
    CHR (hr);

    trial.after          = cpu.GetRegisters();
    trial.writes         = cpu.GetWrites();
    trial.hasUnknownRead = cpu.HasUnknownRead();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionTouches::MarkUnknown
//
//  A register, flag or written value that differs from the first run is
//  fed by the unknown read. When the writes went to different addresses,
//  every write is unknown.
//
////////////////////////////////////////////////////////////////////////////////

void InstructionTouches::MarkUnknown (const Trial & first, const Trial & other, Result & result)
{
    InstructionEffect::Unknowns  & unknowns  = result.unknowns;
    bool                           isAligned = first.writes.size() == other.writes.size();



    unknowns.a     = unknowns.a  || first.after.a  != other.after.a;
    unknowns.x     = unknowns.x  || first.after.x  != other.after.x;
    unknowns.y     = unknowns.y  || first.after.y  != other.after.y;
    unknowns.s     = unknowns.s  || first.after.sp != other.after.sp;
    unknowns.pc    = unknowns.pc || first.after.pc != other.after.pc;
    unknowns.flags = (Byte) (unknowns.flags | (first.after.p ^ other.after.p));

    for (size_t i = 0; isAligned && i < first.writes.size(); i++)
    {
        isAligned = first.writes[i].address == other.writes[i].address;
    }

    for (size_t i = 0; i < result.writes.size(); i++)
    {
        result.writes[i].isUnknown = result.writes[i].isUnknown || !isAligned || first.writes[i].value != other.writes[i].value;
    }
}
