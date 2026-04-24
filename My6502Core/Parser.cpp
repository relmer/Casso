#include "Pch.h"

#include "Parser.h"



std::vector<std::string> Parser::SplitLines (const std::string & source)
{
    (void) source;
    return {};
}



ParsedLine Parser::ParseLine (const std::string & line, int lineNumber)
{
    (void) line;

    ParsedLine result = {};
    result.lineNumber = lineNumber;
    result.isEmpty    = true;
    return result;
}



ClassifiedOperand Parser::ClassifyOperand (const std::string & operand, const std::string & mnemonic)
{
    (void) operand;
    (void) mnemonic;

    ClassifiedOperand result = {};
    result.mode    = GlobalAddressingMode::SingleByteNoOperand;
    result.value   = 0;
    result.isLabel = false;
    return result;
}



bool Parser::ParseValue (const std::string & text, int & value)
{
    (void) text;
    (void) value;
    return false;
}
