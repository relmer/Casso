#include "Pch.h"

#include "As65Mode.h"
#include "ArtifactWriter.h"
#include "Cpu65C02Table.h"





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::CreateInstructionSetProvider
//
//  The 6502, or the 65C02 under `-x` -- the same two CPUs Merlin chooses
//  between, chosen once for the whole file instead of from a line on. A flag
//  has already been read by the time the first line is, so the chosen set is
//  the one the assembly starts on, and there is no directive left to switch
//  anything: the provider gets one set, and that is the choice, not a
//  narrower offer.
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetProvider As65Mode::CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const
{
    const Microcode *  m6502  = cpu.GetInstructionSet();
    const Microcode *  m65C02 = GetCpu65C02InstructionSet();
    bool               isCmos = options.cpuTarget == CommandLineOptions::CpuTarget::M65C02;



    return InstructionSetProvider (isCmos ? m65C02 : m6502);
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
//  A SOURCE PRODUCING SEVERAL OUTPUTS GETS SEVERAL OF EACH, on the same rule the
//  object and the listing follow: one debug file indexed by address cannot
//  describe two outputs that both begin at $0300, because the entries collide
//  and one name wins silently. Today only Merlin cuts a source into several
//  outputs and only AS65 has these three flags, so the loop below runs once in
//  practice. It is written this way so the rule lives in one place rather than
//  becoming a per-dialect special case the day either of those changes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT As65Mode::WriteExtraArtifacts (const CommandLineOptions & options, const AssemblyResult & result) const
{
    HRESULT  hr       = S_OK;
    bool     isSingle = result.savePoints.size() <= 1;



    BAIL_OUT_IF (isSingle, WriteExtraArtifactsForOutput (options, result, options.debugFile, options.symbolFile));

    for (size_t i = 0; i < result.savePoints.size(); i++)
    {
        AssemblyResult      one    = ArtifactWriter::ForOutput (result, i);
        const std::string & given  = result.savePoints[i].name;
        std::string         object = given.empty() ? options.outputFile : given;
        std::string         debug  = options.debugFile.empty()
                                         ? std::string()
                                         : ArtifactWriter::ResolveArtifactName (object, ".dbg");
        std::string         syms   = options.symbolFile.empty()
                                         ? std::string()
                                         : ArtifactWriter::ResolveArtifactName (object, ".sym");

        hr = WriteExtraArtifactsForOutput (options, one, debug, syms);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  As65Mode::WriteExtraArtifactsForOutput
//
//  The three files for one output, under the names it was given.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT As65Mode::WriteExtraArtifactsForOutput (const CommandLineOptions & options, const AssemblyResult & result,
                                                const std::string & debugFile, const std::string & symbolFile)
{
    HRESULT  hr         = S_OK;
    bool     wasWritten = false;



    if (options.symbolTable)
    {
        ArtifactWriter::WriteSymbolTable (result);
    }

    hr = (!options.debugInfo || debugFile.empty())
             ? S_OK
             : ArtifactWriter::WriteDebugInfo (result, debugFile);

    CHR (hr);

    hr         = symbolFile.empty()
                     ? S_OK
                     : ArtifactWriter::WriteSymbolFile (symbolFile, result.symbols);
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        std::cerr << "Error: Cannot write symbol file: " << symbolFile << "\n";
    }

    CHR (hr);

Error:
    return hr;
}
