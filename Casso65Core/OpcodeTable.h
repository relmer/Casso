#pragma once

#include "AssemblerTypes.h"
#include "GlobalAddressingModes.h"





class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  OpcodeTable
//
////////////////////////////////////////////////////////////////////////////////

class OpcodeTable
{
public:
    OpcodeTable ();
    OpcodeTable (const Microcode instructionSet[256]);

    bool Lookup   (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const;
    bool IsMnemonic (const std::string & name) const;
    bool HasMode  (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const;

private:
    std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>> m_table;
};
