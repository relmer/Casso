#pragma once

#include "Pch.h"

#include "GlobalAddressingModes.h"


class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassembledLine
//
//  One instruction as a hex view shows it: where it sits, the bytes it
//  occupies, and the text an assembler would accept for it.
//
//  `isDefined` is false for an opcode the table has no instruction for and for
//  an instruction the buffer ends inside. Both render as `???` over the bytes
//  they cover, so the view never invents an operand the file does not hold.
//
////////////////////////////////////////////////////////////////////////////////

struct DisassembledLine
{
    Word               address   = 0;
    std::vector<Byte>  bytes;
    std::string        mnemonic;
    std::string        operand;
    bool               isDefined = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler
//
//  A linear walk over one instruction table. It has no opinion about which
//  core the bytes are for: the caller hands over the NMOS table from a Cpu or
//  the CMOS table the emulator library builds, and the same loop reads either,
//  because the table itself carries each opcode's mnemonic, addressing mode
//  and legality.
//
//  Linear on purpose. A file of unknown provenance has no entry point to
//  follow, and a view that tried to be clever about data versus code would be
//  wrong about half the time and silent about which half.
//
////////////////////////////////////////////////////////////////////////////////

class Disassembler
{
public:
    static void  Disassemble (std::span<const Byte>           bytes,
                              Word                            origin,
                              const Microcode               * instructionSet,
                              std::vector<DisassembledLine> & outLines);

    //  The operand text for one addressing mode, given the operand bytes and
    //  the address of the instruction they follow.
    static std::string  FormatOperand (GlobalAddressingMode::AddressingMode  mode,
                                       const Byte                          * operandBytes,
                                       Word                                  instructionAddress);

    //  `2000  A9 00     LDA #$00`, one line per instruction.
    static std::string  FormatLine (const DisassembledLine & line);

    static constexpr const char *  kUndefinedMnemonic = "???";

private:
    static std::string  FormatHexByte (Byte value);
    static std::string  FormatHexWord (Word value);
};
