#pragma once

#include "Ui/Debugger/InstructionEffect.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AutomaticWatches
//
//  What an instruction touches: the registers, flags and addresses it reads
//  or writes (FR-095). The watch pane lists these above the manual watches,
//  for the instruction at the PC and the one before it, so the values that
//  matter right now are in front of the user without being asked for.
//
//  NOTHING HERE HOLDS A TABLE OF WHAT EACH OPCODE DOES. Writes and reads come
//  from a ShadowCpu, which sees every access the core makes. What an
//  instruction READS in its registers and flags is found by asking the core
//  again with one of them changed: if the outcome differs, the instruction
//  depended on it. A register is probed by inverting it, a flag by flipping
//  it, and each probe is a whole instruction re-executed against memory that
//  is never touched.
//
//  That answers for the undocumented opcodes and for decimal mode as readily
//  as for LDA, and it cannot drift from the CPU the way a table would. The
//  cost is about a dozen scratch instructions per line, which is nothing next
//  to a stop.
//
////////////////////////////////////////////////////////////////////////////////

class AutomaticWatches
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

    //  Everything the instruction at `pc` touches, with the registers as they
    //  stand. `length` is how many bytes the instruction occupies, so its own
    //  bytes are not listed among the addresses it reads. Empty where the
    //  instruction cannot run without touching the machine -- anything
    //  reading $C000-$C0FF.
    static std::vector<Item>  Find (const IDebugExpressionContext & memory, const Cpu6502Registers & registers,
                                    Word pc, Word length);

private:
    struct Outcome
    {
        Cpu6502Registers               registers  = {};
        std::vector<ShadowCpu::Write>  writes;
        bool                           isComplete = false;

        //  `ignoredFlags` and `ignoredRegister` leave the probe's own change
        //  out of the comparison: an inverted register still holds the
        //  inverted value afterwards if the instruction never looked at it,
        //  and that is not evidence of anything.
        bool  DiffersFrom (const Outcome & other, Byte ignoredFlags, Byte Cpu6502Registers::* ignoredRegister) const;
    };

    static Outcome  Run (const IDebugExpressionContext & memory, const Cpu6502Registers & registers);
};
