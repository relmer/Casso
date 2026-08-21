#include "Pch.h"

#include "MerlinMode.h"
#include "CommandLineParser.h"
#include "Cpu65C02Table.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::CreateInstructionSetProvider
//
//  The 6502 to start on and the 65C02 to switch to. Merlin's `XC` is what
//  decides whether a source reaches the 65C02, and that decision happens
//  mid-assembly -- so both tables have to be in hand before the first line is
//  read. There is no command-line say in it, which is why `options` goes
//  unread.
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetProvider MerlinMode::CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const
{
    (void) options;

    return InstructionSetProvider (cpu.GetInstructionSet(), GetCpu65C02InstructionSet());
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
