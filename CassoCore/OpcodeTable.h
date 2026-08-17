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

    bool TryLookup  (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode, OpcodeEntry & result) const;
    bool IsMnemonic (const std::string & name) const;
    bool HasMode    (const std::string & mnemonic, GlobalAddressingMode::AddressingMode mode) const;

    // Every spelling this table answers to, so a sweep can ask about all of them
    // rather than about the ones somebody listed -- the same reason
    // DirectiveTable::GetAllSpellings exists. Built from the instruction set, so
    // an opcode gaining a mnemonic joins it without anyone editing a test.
    std::vector<std::string> GetAllMnemonics () const;

private:
    std::unordered_map<std::string, std::unordered_map<int, OpcodeEntry>> m_table;
};
