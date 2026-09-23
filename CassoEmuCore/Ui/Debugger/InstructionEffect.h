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

    //  Every address read, the instruction's own bytes among them: the
    //  caller knows where the instruction sits and drops those itself.
    const std::vector<Word>  & GetReads   () const { return m_reads; }

    //  Which register an opcode takes its value from and leaves it in, as
    //  "A", "X", "Y" or "S", from the microcode's own pointers. Only a CPU
    //  can answer: the pointers hold the addresses of its own registers.
    const char *  GetSourceRegister      (Byte opcode) const;
    const char *  GetDestinationRegister (Byte opcode) const;

    void  WriteByte    (Word address, Byte value) override;
    void  WriteWord    (Word address, Word value) override;
    Byte  ReadByteSlow (Word address)             override;
    Word  ReadWord     (Word address)             override;

private:
    const char *  GetRegisterName (const Byte * which) const;

    const IDebugExpressionContext  * m_memory     = nullptr;
    std::vector<Write>               m_writes;
    std::vector<Word>                m_reads;
    bool                             m_isComplete = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect
//
//  What executing one instruction leaves behind, written for the code pane:
//  `A=A0 N=1 Z=0`, `$067B=3B`, `PC=$DB02`.
//
//  THIS ONLY WRITES THE WORDS. The instruction was run by InstructionTouches,
//  over a ShadowCpu, using the same core the machine runs -- so decimal mode,
//  the undocumented opcodes and every flag edge are right because nothing
//  here knows about them. A table of what each opcode does, kept beside the
//  CPU, is what this replaced: it could disagree with the CPU, and a
//  prediction that disagrees with the machine is worse than none.
//
//  ONLY THE INSTRUCTION AT THE PC HAS AN ANSWER. Every other line would need
//  the registers as they will be when execution reaches it, which nothing
//  knows, so the pane asks for this on the current line alone.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionEffect
{
public:
    //  `describeWrite` is asked about each address written. Where it answers,
    //  its words stand in place of "address=value": a write to a soft switch
    //  stores nothing, it operates the machine (FR-112).
    using WriteText = std::function<std::string (Word address)>;

    //  `next` is the address of the following instruction, so a PC that
    //  merely advanced is not reported and a branch or jump is. Empty when
    //  the instruction could not be run, or changed nothing anyone can see.
    static std::string  Format (const Cpu6502Registers        & before,
                                const Cpu6502Registers        & after,
                                const std::vector<ShadowCpu::Write> & writes,
                                Word                            next,
                                const WriteText               & describeWrite = nullptr);

private:
    static std::string  GetFlagChanges (Byte before, Byte after);
};
