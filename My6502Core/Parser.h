#pragma once

#include "AssemblerTypes.h"
#include "GlobalAddressingModes.h"



struct ParsedLine
{
    std::string                          label;
    std::string                          mnemonic;
    std::string                          operand;
    int                                  lineNumber;
    bool                                 isEmpty;
};



struct ClassifiedOperand
{
    GlobalAddressingMode::AddressingMode mode;
    int                                  value;
    bool                                 isLabel;
    std::string                          labelName;
};



class OpcodeTable;



class Parser
{
public:
    static std::vector<std::string> SplitLines (const std::string & source);
    static ParsedLine               ParseLine  (const std::string & line, int lineNumber);

    static ClassifiedOperand ClassifyOperand (const std::string & operand, const std::string & mnemonic);
    static bool              ParseValue      (const std::string & text, int & value);
    static bool              ValidateLabel   (const std::string & label, const OpcodeTable & opcodeTable, std::string & errorMessage);
};
