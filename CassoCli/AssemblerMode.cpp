#include "Pch.h"

#include "AssemblerMode.h"
#include "ArtifactWriter.h"
#include "As65Mode.h"
#include "Assembler.h"
#include "AssemblerExitCode.h"
#include "DialectReporting.h"
#include "MerlinMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::CreateFor
//
//  The base knowing its derived classes is the price of a factory, and is paid
//  here once rather than at every site that has a dialect in hand.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<AssemblerMode> AssemblerMode::CreateFor (DialectId dialect)
{
    std::unique_ptr<AssemblerMode>  mode;



    if (dialect == DialectId::Merlin)
    {
        mode = std::make_unique<MerlinMode>();
    }
    else
    {
        mode = std::make_unique<As65Mode>();
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::Run
//
//  The whole of an assembler subcommand, in the order it happens.
//
//  Each artifact is optional but fails the run the same way, so the exit code
//  is set once from the assembly and each write only ever replaces it with "no
//  output". The alternative -- a distinct code per artifact -- would tell a
//  script which file failed while breaking every script that already knows 2
//  means "no output".
//
//  THE HRESULT IS NOT THE EXIT CODE IN DISGUISE. A missing input is a bad
//  argument, a source that will not assemble is bad data, and a write that
//  failed carries whatever the writer said; each is reported as what it was,
//  and the exit code is set alongside rather than derived from it. An assembly
//  that warned is the case that shows why: it succeeded, and it exits 1.
//
//  The assembler's base directory comes from the input file's own path, so an
//  include resolves relative to the source that names it rather than to the
//  shell's working directory. That is what makes a build work the same from
//  any directory.
//
//  The object's name is asked for AFTER the assembly, because a dialect whose
//  source names its own object cannot answer before the source has been read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblerMode::Run (const CommandLineOptions & options, int & exitCode) const
{
    using Clock = std::chrono::high_resolution_clock;



    const int                       kNoOutput  = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    HRESULT                         hr         = S_OK;
    AssemblerOptions                asmOptions;
    DefaultFileReader               fileReader;
    Cpu                             cpu;
    SourceAssembler::Result         ar;
    std::vector<DialectReportLine>  reports;
    //  The same request with the resolved object name in it, since the name is
    //  only known once the source has had its say.
    CommandLineOptions              writeOptions;
    Clock::time_point               startTime;
    Clock::time_point               endTime;
    size_t                          lastSep    = 0;
    bool                            hasInput   = !options.inputFile.empty();



    exitCode = kNoOutput;

    if (!hasInput)
    {
        std::cerr << "Error: No input file specified\n";
    }

    CBREx (hasInput, E_INVALIDARG);

    asmOptions            = SourceAssembler::BuildOptions (options);
    asmOptions.fileReader = &fileReader;

    lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    cpu.Reset();
    ReportAssemblyStarting (options);

    startTime = Clock::now();
    ar        = SourceAssembler::Assemble (options.inputFile, CreateInstructionSetProvider (options, cpu), asmOptions);
    endTime   = Clock::now();

    ReportAssemblyFinished (options, std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime).count());

    SourceAssembler::ReportDiagnostics (ar);

    //  What is worth saying about the dialect and the CPU, and where it may be
    //  said, is decided in core. This prints what comes back and chooses
    //  nothing: several of those cases report nothing at all, and a caller
    //  reimplementing the rule is a caller that ends up printing always.
    reports  = DialectReporting::BuildReport (asmOptions, SourceAssembler::BuildCpuReport (options, ar.result));
    SourceAssembler::ReportToStderr (reports);

    exitCode = AssemblerExitCode::ToProcessCode (AssemblerExitCode::FromResult (ar.result));

    CBREx (ar.ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ReportAssemblySucceeded (options, ar.result);

    hr = options.generateListing ? ArtifactWriter::WriteListing (ar.result, options, reports) : S_OK;
    CHRF (hr, exitCode = kNoOutput);

    writeOptions            = options;
    writeOptions.outputFile = ResolveOutputName (options, ar.result);

    hr = ArtifactWriter::WriteBinary (ar.result, writeOptions);
    CHRF (hr, exitCode = kNoOutput);

    hr = WriteExtraArtifacts (options, ar.result);
    CHRF (hr, exitCode = kNoOutput);

    if (options.verbose)
    {
        std::cerr << "Assembly successful\n";
        std::println (stderr, "  Output:  {}", writeOptions.outputFile);
        std::println (stderr, "  Bytes:   {}", ar.result.bytes.size());
        std::println (stderr, "  Start:   ${:04X}", ar.result.startAddress);
        std::println (stderr, "  End:     ${:04X}", ar.result.endAddress);
        std::println (stderr, "  Symbols: {}", ar.result.symbols.size());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::ResolveOutputName
//
//  The name the flags resolved, which is what a dialect that does not let its
//  source name the object is left with.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblerMode::ResolveOutputName (const CommandLineOptions & options, const AssemblyResult & result) const
{
    (void) result;

    return options.outputFile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::ReportAssemblyStarting
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::ReportAssemblyStarting (const CommandLineOptions & options) const
{
    (void) options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::ReportAssemblyFinished
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::ReportAssemblyFinished (const CommandLineOptions & options, long long elapsedMicroseconds) const
{
    (void) options;
    (void) elapsedMicroseconds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::ReportAssemblySucceeded
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::ReportAssemblySucceeded (const CommandLineOptions & options, const AssemblyResult & result) const
{
    (void) options;
    (void) result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::WriteExtraArtifacts
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblerMode::WriteExtraArtifacts (const CommandLineOptions & options, const AssemblyResult & result) const
{
    (void) options;
    (void) result;

    return S_OK;
}
