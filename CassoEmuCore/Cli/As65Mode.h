#pragma once

#include "AssemblerMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode
//
//  The `as65` subcommand: AS65-compatible assembly.
//
//  What it adds to AssemblerMode is everything AS65's own command line
//  promises and Merlin's does not -- the CPU flag, the Pass 1 / Pass 2
//  progress, the "N lines assembled" line, and the three further artifacts
//  (`-t`, `-g`, and the symbol file).
//
//  The two verbose pass lines are cosmetic. Assemble runs both passes
//  internally, so they bracket a single call rather than marking real
//  boundaries; the timing figure spans both.
//
////////////////////////////////////////////////////////////////////////////////

class As65Mode : public AssemblerMode
{
public:
    InstructionSetProvider  CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const override;

protected:
    void                    ReportAssemblyStarting       (const CommandLineOptions & options) const override;
    void                    ReportAssemblyFinished       (const CommandLineOptions & options, long long elapsedMicroseconds) const override;
    void                    ReportAssemblySucceeded      (const CommandLineOptions & options, const AssemblyResult & result) const override;

    HRESULT                 WriteExtraArtifacts          (const CommandLineOptions & options, const AssemblyResult & result) const override;

private:
    //  The symbol table, the debug file and the symbol file for ONE output,
    //  under names the caller has already resolved. Split out so a source that
    //  produces several gets a set for each, on the same rule the object and the
    //  listing follow.
    static HRESULT          WriteExtraArtifactsForOutput (const CommandLineOptions & options, const AssemblyResult & result,
                                                          const std::string & debugFile, const std::string & symbolFile);
};
