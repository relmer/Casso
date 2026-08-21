#include "Pch.h"

#include "MerlinMode.h"
#include "CommandLineParser.h"
#include "Cpu65C02Table.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::SelectInstructionSet
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * MerlinMode::SelectInstructionSet (const CommandLineOptions & options, const Cpu & cpu) const
{
    (void) options;

    return cpu.GetInstructionSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::SelectExtendedInstructionSet
//
//  The 65C02, always available to switch to. Merlin's `XC` is what decides
//  whether a source reaches it, and that decision happens mid-assembly -- so
//  the table has to be in hand before the first line is read.
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * MerlinMode::SelectExtendedInstructionSet() const
{
    return GetCpu65C02InstructionSet();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::ResolveOutputName
//
//  What the object is called, once the flag and the source have both had their
//  say.
//
//  The precedence itself is NOT decided here: the assembler was handed the
//  caller's answer and reports the one in effect, so a name coming back is
//  already the winner. What is left is the case neither answered -- no flag and
//  no directive -- where the source's own name is the only thing to derive from.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinMode::ResolveOutputName (const CommandLineOptions & options, const AssemblyResult & result) const
{
    std::string  name   = result.outputFileName;
    bool         wasSet = !name.empty();



    if (!wasSet)
    {
        name = CommandLineParser::StripExtension (options.inputFile) + ".bin";
    }

    return name;
}
