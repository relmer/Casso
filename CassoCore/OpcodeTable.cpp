#include "Pch.h"

#include "OpcodeTable.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetOperandSize
//
//  Operand bytes that follow the opcode, by addressing mode. Grouped by
//  size rather than listed one mode per line: the whole point of the
//  function is that these modes agree on a width, and the old one-return-
//  per-mode form buried that under nineteen near-identical lines.
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetOperandSize (GlobalAddressingMode::AddressingMode mode)
{
    Byte  size = 0;



    switch (mode)
    {
    // One operand byte: a zero-page address, an immediate value, or a
    // signed branch displacement.
    case GlobalAddressingMode::Immediate:
    case GlobalAddressingMode::ZeroPage:
    case GlobalAddressingMode::ZeroPageX:
    case GlobalAddressingMode::ZeroPageY:
    case GlobalAddressingMode::ZeroPageXIndirect:
    case GlobalAddressingMode::ZeroPageIndirectY:
    case GlobalAddressingMode::ZeroPageIndirect:      // 65C02 (zp)
    case GlobalAddressingMode::Relative:
        size = 1;
        break;

    // Two operand bytes: a full 16-bit address, or the 65C02 bit-branch
    // pair (zp byte + rel byte).
    case GlobalAddressingMode::Absolute:
    case GlobalAddressingMode::AbsoluteX:
    case GlobalAddressingMode::AbsoluteY:
    case GlobalAddressingMode::JumpAbsolute:
    case GlobalAddressingMode::JumpIndirect:
    case GlobalAddressingMode::JumpIndirectCmos:      // 65C02 (abs) page-fixed JMP
    case GlobalAddressingMode::AbsoluteXIndirect:     // 65C02 (abs,X) JMP
    case GlobalAddressingMode::ZeroPageRelative:      // 65C02 BBRn/BBSn
        size = 2;
        break;

    // No operand byte: the opcode is the whole instruction.
    case GlobalAddressingMode::Accumulator:
    case GlobalAddressingMode::SingleByteNoOperand:
    default:
        break;
    }

    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToUpperCase
//
////////////////////////////////////////////////////////////////////////////////

static std::string ToUpperCase (const char * name)
{
    std::string result (name);

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
////////////////////////////////////////////////////////////////////////////////

OpcodeTable::OpcodeTable()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCycleCounts — standard 6502 cycle counts per opcode
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetCycleCounts (Byte opcode)
{
    // Standard 6502 cycle counts (no page-crossing or branch-taken penalties)
    static const Byte s_cycles[256] =
    {
        7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,0,  // $00-$0F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $10-$1F
        6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0,  // $20-$2F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $30-$3F
        6,6,0,0,0,3,5,0,3,2,2,0,3,4,6,0,  // $40-$4F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $50-$5F
        6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0,  // $60-$6F
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $70-$7F
        0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0,  // $80-$8F
        2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0,  // $90-$9F
        2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0,  // $A0-$AF
        2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0,  // $B0-$BF
        2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,  // $C0-$CF
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $D0-$DF
        2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0,  // $E0-$EF
        2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0,  // $F0-$FF
    };

    return s_cycles[opcode];
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
//  Inverts the instruction set: builds a mnemonic-and-addressing-mode lookup
//  so the ASSEMBLER can find the opcode for what a source line says.
//
//  The CPU indexes by opcode; the assembler needs the opposite direction, and
//  building it from the same Microcode table is what keeps the two from ever
//  disagreeing about what an instruction does.
//
//  Two classes of opcode are deliberately EXCLUDED. Illegal opcodes and
//  assembler-hidden fills -- the 65C02's reserved-NOP slots, for instance --
//  still execute and still disassemble, but must not be selectable by
//  mnemonic. Including them would let a filler NOP shadow the real $EA, so a
//  plain `NOP` could assemble to the wrong byte.
//
//  Synonyms are aliased to an existing mnemonic's entries rather than given
//  their own, so an alias cannot drift from the instruction it names. They
//  exist because period assemblers accepted these spellings and period sources
//  still use them.
//
//  Mnemonics are uppercased on the way in, so lookup is case-insensitive
//  without a comparator.
//
////////////////////////////////////////////////////////////////////////////////

OpcodeTable::OpcodeTable (const Microcode instructionSet[256])
{
    for (int i = 0; i < 256; i++)
    {
        const Microcode & mc = instructionSet[i];

        // Skip illegal opcodes and assembler-hidden fills (e.g. the 65C02
        // reserved-NOP slots): they execute and disassemble, but must not be
        // selectable by mnemonic -- otherwise a filler NOP would shadow $EA.
        if (!mc.isLegal || mc.assemblerHidden)
        {
            continue;
        }

        std::string mnemonic = ToUpperCase (mc.instructionName);
        int         mode     = (int) mc.globalAddressingMode;

        OpcodeEntry entry = {};
        entry.opcode      = (Byte) i;
        entry.operandSize = GetOperandSize (mc.globalAddressingMode);
        entry.cycleCounts = GetCycleCounts ((Byte) i);

        m_table[mnemonic][mode] = entry;
    }

    // Instruction synonyms: alias → existing mnemonic's entries
    struct Synonym { const char * alias; const char * target; };
    Synonym synonyms[] =
    {
        { "DISABLE", "SEI" },
        { "ENABLE",  "CLI" },
        { "STC",     "SEC" },
        { "STI",     "SEI" },
        { "STD",     "SED" },
    };

    for (const auto & syn : synonyms)
    {
        auto it = m_table.find (syn.target);

        if (it != m_table.end())
        {
            m_table[syn.alias] = it->second;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryLookup
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::TryLookup (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const
{
    auto  mnemonicIt = m_table.find (mnemonic);
    bool  found      = false;



    // Two levels: an unknown mnemonic and a known one that lacks this
    // addressing mode both mean "no encoding", and `result` is untouched.
    if (mnemonicIt != m_table.end())
    {
        auto  modeIt = mnemonicIt->second.find ((int) mode);

        if (modeIt != mnemonicIt->second.end())
        {
            result = modeIt->second;
            found  = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMnemonic
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::IsMnemonic (const std::string & name) const
{
    return m_table.find (name) != m_table.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasMode
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::HasMode (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const
{
    auto  mnemonicIt = m_table.find (mnemonic);



    // Short-circuit order guards the inner find: an unknown mnemonic has no
    // per-mode map to search.
    return mnemonicIt != m_table.end()
        && mnemonicIt->second.find ((int) mode) != mnemonicIt->second.end();
}
