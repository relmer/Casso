#include "Pch.h"

#include "OpcodeTable.h"
#include "Microcode.h"



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
    case GlobalAddressingMode::Relative:           return 1;
    case GlobalAddressingMode::SingleByteNoOperand: return 0;
    default:                                       return 0;
    }
}



static std::string ToUpperCase (const char * name)
{
    std::string result (name);

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}



OpcodeTable::OpcodeTable ()
{
}



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

        m_table[mnemonic][mode] = entry;
    }
}



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



bool OpcodeTable::IsMnemonic (const std::string & name) const
{
    return m_table.find (name) != m_table.end ();
}



bool OpcodeTable::HasMode (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const
{
    auto mnemonicIt = m_table.find (mnemonic);

    if (mnemonicIt == m_table.end ())
    {
        return false;
    }

    return mnemonicIt->second.find ((int) mode) != mnemonicIt->second.end ();
}
