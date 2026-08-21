#pragma once

#include "AssemblerMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode
//
//  The `merlin` subcommand: assemble one Merlin source and write its object.
//
//  Both instruction tables go across, because Merlin selects its CPU in the
//  source and a provider with nothing to switch to would leave such a source
//  told it had reached the wider processor while the assembler stayed on the
//  narrow one. It is also why there is no CPU flag: the source decides, and a
//  flag accepted here would assemble source the real assembler rejects.
//
//  A clean run says NOTHING, which is why this grammar has no quiet flag to
//  silence it and why none of the progress hooks is overridden. The line AS65
//  prints and offers a flag against is a historical courtesy, and a subcommand
//  added today can simply not print it.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinMode : public AssemblerMode
{
public:
    InstructionSetProvider  CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const override;

protected:
    std::string             ResolveOutputName            (const CommandLineOptions & options, const AssemblyResult & result) const override;
};
