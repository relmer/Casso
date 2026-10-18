#pragma once

#include "Ui/Debugger/InstructionEffect.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionTouches
//
//  An instruction's inputs and outputs: the registers, flags and addresses it
//  reads or writes (FR-095). The code pane annotates a line from these, and
//  the watch pane lists them above the manual watches, so the values that
//  matter for the instruction in front of the user are already on screen.
//
//  WHAT AN INSTRUCTION TOUCHES IS A PROPERTY OF THE INSTRUCTION, not of how
//  the values fell this time. `AND #$00` reads A even though the answer is 0
//  whatever A held, and `INC` writes Z even when Z does not move. Whether a
//  value CHANGED is a separate question, and only the highlight asks it.
//
//  So nothing here is inferred from behavior. Three sources, each already
//  describing the instruction:
//
//    addresses   EffectiveAddress, from the microcode's addressing mode
//    registers   the microcode's own source and destination registers, plus
//                the index register the addressing mode uses
//    flags       InstructionFlags, the one part the emulator holds no data
//                for, checked against the core by its own test
//
//  The values, and what the instruction leaves behind, come from running it
//  on a ShadowCpu -- the emulator's own core over memory it cannot change.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionTouches
{
public:
    enum class Kind
    {
        Register,
        Flag,
        Address,
    };

    struct Item
    {
        Kind         kind    = Kind::Register;

        //  "A", "X", "Y", "S" for a register; "C", "Z", "I", "D", "V", "N"
        //  for a flag; empty for an address.
        std::string  name;
        Word         address = 0;

        bool         isRead  = false;
        bool         isWrite = false;
    };

    struct Result
    {
        //  False where the instruction could not be run. The items are
        //  valid either way: what it touches is known even where the values
        //  are not.
        bool                           isKnown = false;

        std::vector<Item>              items;

        //  Where the instruction left the machine: the registers after it,
        //  and what it wrote. The result annotation is written from these,
        //  so the pane runs the instruction ONCE for both columns.
        Cpu6502Registers               after   = {};
        std::vector<ShadowCpu::Write>  writes;

        //  The cycles the instruction took on the core: a taken branch
        //  and a page crossing are in the count.
        uint32_t                       cycles  = 0;

        //  What a read with no peek feeds; each write it feeds has
        //  isUnknown set.
        InstructionEffect::Unknowns    unknowns;
    };

    //  Everything the instruction at `pc` touches, with the registers as they
    //  stand. `length` is how many bytes it occupies.
    static Result  Find (const IDebugExpressionContext & memory, const Microcode * instructionSet,
                         const Cpu6502Registers & registers, Word pc, Word length);

private:
    //  One run of the instruction on a ShadowCpu.
    struct Trial
    {
        Cpu6502Registers               after          = {};
        std::vector<ShadowCpu::Write>  writes;
        uint32_t                       cycles         = 0;
        bool                           hasUnknownRead = false;
    };

    static HRESULT  RunTrial    (const IDebugExpressionContext & memory, const Cpu6502Registers & registers, Byte fill, Trial & trial);
    static void     MarkUnknown (const Trial & first, const Trial & other, Result & result);
};
