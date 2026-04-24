#include "Pch.h"

#include "Assembler.h"
#include "Parser.h"



Assembler::Assembler (const Microcode instructionSet[256], AssemblerOptions options) :
    m_opcodeTable (instructionSet),
    m_options     (options)
{
}



AssemblyResult Assembler::Assemble (const std::string & sourceText)
{
    (void) sourceText;

    AssemblyResult result = {};
    result.success = false;
    return result;
}
