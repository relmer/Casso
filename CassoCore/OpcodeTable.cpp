#include "Pch.h"

#include "OpcodeTable.h"
#include "Microcode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetOperandSize
//
////////////////////////////////////////////////////////////////////////////////

static Byte GetOperandSize (GlobalAddressingMode::AddressingMode mode)
{
    switch (mode)
    {
    case GlobalAddressingMode::Immediate:          return 1;
    case GlobalAddressingMode::ZeroPage:           return 1;
    case GlobalAddressingMode::ZeroPageX:          return 1;
    case GlobalAddressingMode::ZeroPageY:          return 1;
    case GlobalAddressingMode::Absolute:           return 2;
    case GlobalAddressingMode::AbsoluteX:          return 2;
    case GlobalAddressingMode::AbsoluteY:          return 2;
    case GlobalAddressingMode::ZeroPageXIndirect:  return 1;
    case GlobalAddressingMode::ZeroPageIndirectY:  return 1;
    case GlobalAddressingMode::Accumulator:        return 0;
    case GlobalAddressingMode::JumpAbsolute:       return 2;
    case GlobalAddressingMode::JumpIndirect:       return 2;
    case GlobalAddressingMode::JumpIndirectCmos:   return 2;   // 65C02 (abs) page-fixed JMP
    case GlobalAddressingMode::ZeroPageIndirect:   return 1;   // 65C02 (zp)
    case GlobalAddressingMode::AbsoluteXIndirect:  return 2;   // 65C02 (abs,X) JMP
    case GlobalAddressingMode::ZeroPageRelative:   return 2;   // 65C02 BBRn/BBSn: zp byte + rel byte
    case GlobalAddressingMode::Relative:           return 1;
    case GlobalAddressingMode::SingleByteNoOperand: return 0;
    default:                                       return 0;
    }
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

OpcodeTable::OpcodeTable ()
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
////////////////////////////////////////////////////////////////////////////////

OpcodeTable::OpcodeTable (const Microcode instructionSet[256])
{
    for (int i = 0; i < 256; i++)
    {
        const Microcode & mc = instructionSet[i];

        if (!mc.isLegal)
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

        if (it != m_table.end ())
        {
            m_table[syn.alias] = it->second;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Lookup
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::Lookup (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const
{
    auto mnemonicIt = m_table.find (mnemonic);

    if (mnemonicIt == m_table.end ())
    {
        return false;
    }

    auto modeIt = mnemonicIt->second.find ((int) mode);

    if (modeIt == mnemonicIt->second.end ())
    {
        return false;
    }

    result = modeIt->second;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMnemonic
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::IsMnemonic (const std::string & name) const
{
    return m_table.find (name) != m_table.end ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasMode
//
////////////////////////////////////////////////////////////////////////////////

bool OpcodeTable::HasMode (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const
{
    auto mnemonicIt = m_table.find (mnemonic);

    if (mnemonicIt == m_table.end ())
    {
        return false;
    }

    return mnemonicIt->second.find ((int) mode) != mnemonicIt->second.end ();
}
