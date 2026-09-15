#pragma once

#include "GlobalAddressingModes.h"

class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassembledInstruction
//
//  One decoded instruction. The operand is Monitor text ($3E, #$A0, ($3E),Y);
//  target is the absolute address a branch or jump goes to, when it has one.
//
////////////////////////////////////////////////////////////////////////////////

struct DisassembledInstruction
{
    Word               address    = 0;
    std::vector<Byte>  bytes;
    std::string        mnemonic;
    std::string        operand;
    bool               hasTarget  = false;
    Word               target     = 0;
    bool               documented = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler
//
////////////////////////////////////////////////////////////////////////////////

class Disassembler
{
public:
    explicit Disassembler (const Microcode * instructionSet);

    static constexpr size_t  kMaxInstructionBytes = 3;

    size_t   GetLength       (Byte opcode) const;
    HRESULT  DisassembleOne  (Word                      address,
                              std::span<const Byte>     bytes,
                              DisassembledInstruction & instruction) const;

private:
    static void  FormatOperand (GlobalAddressingMode::AddressingMode   mode,
                                Word                                   address,
                                std::span<const Byte>                  bytes,
                                DisassembledInstruction              & instruction);

    const Microcode * m_instructionSet = nullptr;
};
