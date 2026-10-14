#pragma once

#include "Cpu6502.h"

class IDebugExpressionContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ShadowCpu
//
//  A 6502 that reads the machine without touching it and writes nowhere.
//
//  A read returns the byte from `IDebugExpressionContext::TryPeek`, the
//  side-effect-free view of the memory pane, or for $C000-$C0FF from
//  `TryPeekIo`, the switch's own side-effect-free peek. A read neither
//  returns a byte for gets the fill value set for the run and marks the run
//  as having an unknown read. Writes are listed instead of performed.
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
        Word  address   = 0;
        Byte  value     = 0;

        //  The value came out different when an unknown read returned a
        //  different byte.
        bool  isUnknown = false;
    };

    explicit ShadowCpu (const IDebugExpressionContext & memory) : m_memory (&memory) {}

    //  The byte a read with no peek returns, and whether any read was one.
    void                       SetUnknownReadValue (Byte value) { m_unknownValue = value; }
    bool                       HasUnknownRead      () const     { return m_hasUnknownRead; }
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

    const IDebugExpressionContext  * m_memory         = nullptr;
    std::vector<Write>               m_writes;
    std::vector<Word>                m_reads;
    Byte                             m_unknownValue   = 0;
    bool                             m_hasUnknownRead = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect
//
//  What executing one instruction leaves behind, written for the code pane:
//  `A=A0 N=1 Z=0`, `$067B=3B`, `PC=$DB02`. A push shows as the value
//  pushed rather than as stack writes: `S=CC pushed $DD97 (returns to $DD98)`.
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

    //  What an unknown read feeds: the registers and flags that came out
    //  different when the read returned a different byte. Each is written
    //  as "?".
    struct Unknowns
    {
        bool  a     = false;
        bool  x     = false;
        bool  y     = false;
        bool  s     = false;
        bool  pc    = false;
        Byte  flags = 0;
    };

    //  `next` is the address of the following instruction, so a PC that
    //  merely advanced is not reported and a branch or jump is. Empty when
    //  the instruction could not be run, or changed nothing anyone can see.
    static std::string  Format (const Cpu6502Registers        & before,
                                const Cpu6502Registers        & after,
                                const std::vector<ShadowCpu::Write> & writes,
                                Word                            next,
                                const WriteText               & describeWrite = nullptr,
                                const Unknowns                & unknowns      = {});

private:
    static constexpr Word  kStackPage = 0x01;

    static std::string  GetFlagChanges (Byte before, Byte after, Byte unknown = 0);
    static std::string  GetPush        (Byte spBefore, Byte spAfter, const std::vector<ShadowCpu::Write> & writes);
    static bool         IsPushedByte   (Byte spBefore, Byte spAfter, Word address);
};
