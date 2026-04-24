#include "Pch.h"

#include "OpcodeTable.h"
#include "Microcode.h"



OpcodeTable::OpcodeTable ()
{
}



OpcodeTable::OpcodeTable (const Microcode instructionSet[256])
{
    (void) instructionSet;
}



bool OpcodeTable::Lookup (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const
{
    (void) mnemonic;
    (void) mode;
    (void) result;
    return false;
}



bool OpcodeTable::IsMnemonic (const std::string & name) const
{
    (void) name;
    return false;
}



bool OpcodeTable::HasMode (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const
{
    (void) mnemonic;
    (void) mode;
    return false;
}
