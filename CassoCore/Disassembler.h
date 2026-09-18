#pragma once

#include "Pch.h"

#include "GlobalAddressingModes.h"


class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassembledInstruction
//
//  One decoded instruction: where it sits, the bytes it occupies, and the
//  text an assembler would accept for it. The operand is Monitor text ($3E,
//  #$A0, ($3E),Y); target is the absolute address a branch or jump goes to,
//  when it has one.
//
//  `operandAddress` is the address the operand names, when it names one: the
//  location for a direct or indexed mode, the pointer for an indirect one, the
//  destination for a branch or jump. It is what a symbol is looked up by.
//
//  `isDefined` is false for an opcode the table has no instruction for and for
//  an instruction the buffer ends inside. Both render as `???` over the bytes
//  they cover, so a listing never invents an operand the bytes do not hold.
//  `documented` is narrower: an undocumented opcode is defined but hidden from
//  the assembler.
//
////////////////////////////////////////////////////////////////////////////////

struct DisassembledInstruction
{
    Word               address           = 0;
    std::vector<Byte>  bytes;
    std::string        mnemonic;
    std::string        operand;
    bool               hasTarget         = false;
    Word               target            = 0;
    bool               hasOperandAddress = false;
    Word               operandAddress    = 0;
    bool               isDefined         = false;
    bool               documented        = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler
//
//  Decodes against one instruction table. It has no opinion about which core
//  the bytes are for: the caller hands over the NMOS table from a Cpu or the
//  CMOS table the emulator library builds, and the same code reads either,
//  because the table itself carries each opcode's mnemonic, addressing mode
//  and legality.
//
//  Two ways in. An instance decodes one instruction at an address, which is
//  what a debugger walking live memory needs. Disassemble walks a whole buffer
//  linearly, which is what a view of a file of unknown provenance needs: a
//  file has no entry point to follow, and a view that tried to be clever about
//  data versus code would be wrong about half the time and silent about which
//  half.
//
////////////////////////////////////////////////////////////////////////////////

class Disassembler
{
public:
    explicit Disassembler (const Microcode * instructionSet);

    static constexpr size_t        kMaxInstructionBytes = 3;
    static constexpr const char *  kUndefinedMnemonic   = "???";

    size_t   GetLength       (Byte opcode) const;
    HRESULT  DisassembleOne  (Word                      address,
                              std::span<const Byte>     bytes,
                              DisassembledInstruction & instruction) const;

    static void  Disassemble (std::span<const Byte>                   bytes,
                              Word                                    origin,
                              const Microcode                       * instructionSet,
                              std::vector<DisassembledInstruction>  & outLines);

    //  `2000  A9 00     LDA #$00`, one line per instruction.
    static std::string  FormatLine (const DisassembledInstruction & line);

    //  Operand text with the address it names written as a name instead:
    //  `($06),Y` becomes `(PTR),Y`. The text comes back unchanged when the
    //  address does not appear in it as an address, or the name is empty.
    static std::string  SubstituteSymbol (const std::string & operand, Word address, const std::string & name);

private:
    void  Decode (Word                      address,
                  std::span<const Byte>     bytes,
                  DisassembledInstruction & instruction) const;

    static void  FormatOperand     (GlobalAddressingMode::AddressingMode   mode,
                                    Word                                   address,
                                    std::span<const Byte>                  bytes,
                                    DisassembledInstruction              & instruction);
    static void  SetOperandAddress (GlobalAddressingMode::AddressingMode   mode,
                                    Byte                                   zp,
                                    Word                                   absolute,
                                    DisassembledInstruction              & instruction);
    static bool  IsAddressAt       (const std::string & operand, size_t at, size_t length);

    const Microcode * m_instructionSet = nullptr;
};
