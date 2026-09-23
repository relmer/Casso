#pragma once

#include "Cpu6502.h"
#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu
//
//  A 6502 that reads the machine without touching it and writes nowhere.
//
//  Reads are answered from `IDebugExpressionContext::TryPeek`, the same
//  side-effect-free view the memory pane shows; a read it declines -- every
//  address in $C000-$C0FF, where reading is how the machine is operated --
//  stops the whole prediction. Writes are recorded instead of performed.
//
//  `m_readPages` is left null, so no read takes the base class's inline fast
//  path to real memory: every one arrives at ReadByteSlow.
//
////////////////////////////////////////////////////////////////////////////////

class ShadowCpu : public Cpu6502
{
public:
    struct Write
    {
        Word  address = 0;
        Byte  value   = 0;
    };

    explicit ShadowCpu (const IDebugExpressionContext & memory) : m_memory (&memory) {}

    //  False when a read could not be answered, which makes the run worthless.
    bool                       IsComplete () const { return m_isComplete; }
    const std::vector<Write> & GetWrites  () const { return m_writes; }

    void  WriteByte    (Word address, Byte value) override;
    void  WriteWord    (Word address, Word value) override;
    Byte  ReadByteSlow (Word address)             override;
    Word  ReadWord     (Word address)             override;

private:
    const IDebugExpressionContext  * m_memory     = nullptr;
    std::vector<Write>               m_writes;
    bool                             m_isComplete = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect
//
//  What executing one instruction would leave behind, written for the code
//  pane: `A=A0 N=1 Z=0`, `$067B=3B`, `PC=$DB02`.
//
//  THE EMULATOR ITSELF ANSWERS. The instruction is executed by the same core
//  the machine runs, over a ShadowCpu, and the answer is whatever changed
//  between the registers going in and coming out. A table of what each
//  opcode does, written beside the CPU, is what this replaces: it could
//  disagree with the CPU, and a prediction that disagrees with the machine is
//  worse than none. Decimal mode, the undocumented opcodes and every flag
//  edge come out right because nothing here knows about them.
//
//  ONLY THE INSTRUCTION AT THE PC HAS AN ANSWER. Every other line would need
//  the registers as they will be when execution reaches it, which nothing
//  knows, so the pane asks for this on the current line alone.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionEffect
{
public:
    //  Empty when the instruction cannot be predicted: a read the peek
    //  refuses, or an instruction that changes nothing a person can see.
    //  `next` is the address of the following instruction, so a PC that
    //  merely advanced is not reported and a branch or jump is.
    static std::string  Describe (const IDebugExpressionContext & memory, const Cpu6502Registers & registers, Word next);

private:
    static std::string  GetFlagChanges (Byte before, Byte after);
};
