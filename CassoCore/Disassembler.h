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
    std::string        comment;
    bool               isDefined = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyOptions
//
//  Context for a disassembly beyond the bytes: names for known addresses, the
//  NMOS opcodes undefined in an instruction table, and the ProDOS MLI's
//  calling convention, whose JSR is followed by a call number and the address
//  of a parameter list rather than by the next instruction.
//
////////////////////////////////////////////////////////////////////////////////

struct DisassemblyOptions
{
    //  An absolute operand at one of these addresses is written as its name.
    //  Zero page is left as numbers, since programs reuse it for their
    //  variables.
    const std::map<Word, std::string>  * symbols = nullptr;

    //  Opcodes the table has no instruction for are decoded as the NMOS
    //  6502 decodes them: the unstable combined operations, and JAM.
    bool                                 nmosUndocumented = false;

    //  A JSR to `mliEntry` is followed by a call number, which is written
    //  as a DFB with the call's name from `mliCalls`, and by a DW of the
    //  parameter list's address.
    std::optional<Word>                  mliEntry;
    const std::map<Byte, std::string>  * mliCalls = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disassembler
//
//  A linear walk over one instruction table. It has no opinion about which
//  core the bytes are for: the caller hands over the NMOS table from a Cpu or
//  the CMOS table the emulator library builds, and the same loop reads either,
//  because the table itself holds each opcode's mnemonic, addressing mode
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
                              std::vector<DisassembledLine> & outLines,
                              const DisassemblyOptions      & options = {});

    //  The operand text for one addressing mode, given the operand bytes and
    //  the address of the instruction they follow.
    static std::string  FormatOperand (GlobalAddressingMode::AddressingMode  mode,
                                       const Byte                          * operandBytes,
                                       Word                                  instructionAddress);

    //  `2000  A9 00     LDA #$00`, one line per instruction.
    static std::string  FormatLine (const DisassembledLine & line);

    static constexpr const char *  kUndefinedMnemonic = "???";
    static constexpr const char *  kByteDirective     = "DFB";
    static constexpr const char *  kWordDirective     = "DW";

    static std::string  FormatHexByte (Byte value);
    static std::string  FormatHexWord (Word value);

private:
    //  An NMOS opcode undefined in the CPU's table.
    struct UndocumentedOpcode
    {
        Byte                                  opcode;
        const char                          * mnemonic;
        GlobalAddressingMode::AddressingMode  mode;
    };

    //  The unstable combined operations, whose results depend on the chip
    //  for some values, and the opcodes that halt the CPU. Source: groepaz,
    //  "No More Secrets: NMOS 6510 Unintended Opcodes", whose opcodes the
    //  6502 shares.
    static constexpr UndocumentedOpcode  s_kUndocumentedOpcodes[] =
    {
        { 0x0B, "ANC", GlobalAddressingMode::Immediate },
        { 0x2B, "ANC", GlobalAddressingMode::Immediate },
        { 0x4B, "ALR", GlobalAddressingMode::Immediate },
        { 0x6B, "ARR", GlobalAddressingMode::Immediate },
        { 0x8B, "ANE", GlobalAddressingMode::Immediate },
        { 0xAB, "LXA", GlobalAddressingMode::Immediate },
        { 0xCB, "SBX", GlobalAddressingMode::Immediate },
        { 0xEB, "SBC", GlobalAddressingMode::Immediate },
        { 0x93, "SHA", GlobalAddressingMode::ZeroPageIndirectY },
        { 0x9F, "SHA", GlobalAddressingMode::AbsoluteY },
        { 0x9E, "SHX", GlobalAddressingMode::AbsoluteY },
        { 0x9C, "SHY", GlobalAddressingMode::AbsoluteX },
        { 0x9B, "TAS", GlobalAddressingMode::AbsoluteY },
        { 0xBB, "LAS", GlobalAddressingMode::AbsoluteY },
        { 0x02, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x12, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x22, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x32, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x42, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x52, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x62, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x72, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0x92, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0xB2, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0xD2, "JAM", GlobalAddressingMode::SingleByteNoOperand },
        { 0xF2, "JAM", GlobalAddressingMode::SingleByteNoOperand },
    };

    static constexpr Byte  s_kJsrOpcode = 0x20;

    static const UndocumentedOpcode  * FindUndocumented (Byte opcode);
    static void                        ApplySymbol      (DisassembledLine & line, GlobalAddressingMode::AddressingMode mode, const std::map<Word, std::string> & symbols);
};
