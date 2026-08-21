#include "Pch.h"

#include "As65Mode.h"
#include "ArtifactWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::SelectInstructionSet
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * As65Mode::SelectInstructionSet (const CommandLineOptions & options, const Cpu & cpu) const
{
    return SourceAssembler::SelectInstructionSet (options, cpu);
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::SelectExtendedInstructionSet
//
//  None. The CPU is named on the command line and stands for the whole
//  assembly, so a source cannot switch to a wider one part way through -- which
//  is exactly what `-x` being a flag rather than a directive means.
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * As65Mode::SelectExtendedInstructionSet() const
{
    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::ReportAssemblyStarting
//
////////////////////////////////////////////////////////////////////////////////

void As65Mode::ReportAssemblyStarting (const CommandLineOptions & options) const
{
    if (options.verbose)
    {
        std::cerr << "Pass 1...\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::ReportAssemblyFinished
//
////////////////////////////////////////////////////////////////////////////////

void As65Mode::ReportAssemblyFinished (const CommandLineOptions & options, long long elapsedMicroseconds) const
{
    if (options.verbose)
    {
        std::cerr << "Pass 2...\n";
        std::println (stderr, "Assembly time: {} us", elapsedMicroseconds);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::ReportAssemblySucceeded
//
//  linesAssembled, NOT listing.size(): the listing is only built when one was
//  requested, so this reported "0 lines assembled" for every ordinary
//  invocation -- of a file it had just assembled correctly.
//
////////////////////////////////////////////////////////////////////////////////

void As65Mode::ReportAssemblySucceeded (const CommandLineOptions & options, const AssemblyResult & result) const
{
    if (!options.quiet)
    {
        std::cerr << result.linesAssembled << " lines assembled\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::WriteExtraArtifacts
//
//  The symbol table, the debug file and the symbol file -- the three AS65
//  writes and Merlin has no flags for.
//
//  Each is optional and each fails the same way, so they reduce to a "not
//  requested, or written successfully" chain. The symbol file names itself in
//  its failure because it is the only one whose path came from a flag the
//  reader may have mistyped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT As65Mode::WriteExtraArtifacts (const CommandLineOptions & options, const AssemblyResult & result) const
{
    HRESULT  hr         = S_OK;
    bool     wasWritten = false;



    if (options.symbolTable)
    {
        ArtifactWriter::WriteSymbolTable (result);
    }

    hr = (!options.debugInfo || options.debugFile.empty())
             ? S_OK
             : ArtifactWriter::WriteDebugInfo (result, options.debugFile);

    CHR (hr);

    hr         = options.symbolFile.empty()
                     ? S_OK
                     : ArtifactWriter::WriteSymbolFile (options.symbolFile, result.symbols);
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        std::cerr << "Error: Cannot write symbol file: " << options.symbolFile << "\n";
    }

    CHR (hr);

Error:
    return hr;
}
