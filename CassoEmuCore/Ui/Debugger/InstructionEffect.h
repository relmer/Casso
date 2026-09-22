#pragma once

#include "I6502DebugInfo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionEffect
//
//  What executing one instruction would leave behind, written for the code
//  pane: `A=A0 N=1 Z=0`, `$067B=3B`, `PC=$DB02`.
//
//  ONLY THE INSTRUCTION AT THE PC HAS AN ANSWER. Every other line would need
//  the registers as they will be when execution reaches it, which nothing
//  knows, so the pane asks for this on the current line alone.
//
//  Nothing here executes: the result is computed from the registers, the
//  operand's value and, for a store, the address. An instruction this does
//  not model returns an empty string rather than a guess.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionEffect
{
public:
    struct Input
    {
        std::string          mnemonic;
        Cpu6502Registers     registers;

        //  The byte the instruction reads: the immediate itself, or the byte
        //  at the effective address. Absent where the operand is an address
        //  that cannot be read, such as I/O.
        std::optional<Byte>  value;

        //  The effective address, for the instructions that write one.
        std::optional<Word>  address;

        //  Where a branch or jump goes, as disassembled.
        std::optional<Word>  target;

        //  The address of the instruction after this one, which a branch not
        //  taken and a JSR's saved address both need.
        Word                 next = 0;
    };

    static std::string  Describe (const Input & input);

private:
    static std::string  GetFlags   (Byte value);
    static std::string  GetCompare (Byte left, Byte right);
};
