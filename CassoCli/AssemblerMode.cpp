#include "Pch.h"

#include "AssemblerMode.h"
#include "ArtifactWriter.h"
#include "Assembler.h"
#include "AssemblerExitCode.h"
#include "DialectReporting.h"





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
//  The assembler's base directory comes from the input file's own path, so an
//  include resolves relative to the source that names it rather than to the
//  shell's working directory. That is what makes a build work the same from
//  any directory.
//
//  The object's name is asked for AFTER the assembly, because a dialect whose
//  source names its own object cannot answer before the source has been read.
//
////////////////////////////////////////////////////////////////////////////////

int AssemblerMode::Run (const CommandLineOptions & options) const
{
    using Clock = std::chrono::high_resolution_clock;



    HRESULT                         hr           = S_OK;
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
    size_t                          lastSep      = 0;
    int                             exitCode     = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    bool                            hasInput     = !options.inputFile.empty();
    bool                            wasWritten   = false;



    if (!hasInput)
    {
        std::cerr << "Error: No input file specified\n";
    }

    BAIL_OUT_IF (!hasInput, S_OK);

    asmOptions            = SourceAssembler::BuildOptions (options);
    asmOptions.fileReader = &fileReader;

    lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    cpu.Reset();
    BeforeAssembly (options);

    startTime = Clock::now();
    ar        = SourceAssembler::Assemble (options.inputFile,
                                           NarrowInstructionSet (options, cpu),
                                           WideInstructionSet(),
                                           asmOptions);
    endTime   = Clock::now();

    AfterAssembly (options, std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime).count());

    SourceAssembler::ReportDiagnostics (ar);

    //  What is worth saying about the dialect and the CPU, and where it may be
    //  said, is decided in core. This prints what comes back and chooses
    //  nothing: several of those cases report nothing at all, and a caller
    //  reimplementing the rule is a caller that ends up printing always.
    reports  = DialectReporting::BuildReport (asmOptions, SourceAssembler::BuildCpuReport (options, ar.result));
    SourceAssembler::ReportToStderr (reports);

    exitCode = AssemblerExitCode::ToProcessCode (AssemblerExitCode::FromResult (ar.result));

    BAIL_OUT_IF (!ar.ok, S_OK);

    ReportAssembled (options, ar.result);

    hr         = options.generateListing ? ArtifactWriter::WriteListing (ar.result, options, reports) : S_OK;
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

    writeOptions            = options;
    writeOptions.outputFile = OutputName (options, ar.result);

    hr         = ArtifactWriter::WriteBinary (ar.result, writeOptions);
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

    hr         = WriteExtraArtifacts (options, ar.result);
    wasWritten = SUCCEEDED (hr);

    if (!wasWritten)
    {
        exitCode = AssemblerExitCode::ToProcessCode (AssemblyExitCode::NoOutput);
    }

    BAIL_OUT_IF (!wasWritten, S_OK);

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
    return exitCode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::OutputName
//
//  The name the flags resolved, which is what a dialect that does not let its
//  source name the object is left with.
//
////////////////////////////////////////////////////////////////////////////////

std::string AssemblerMode::OutputName (const CommandLineOptions & options, const AssemblyResult & result) const
{
    (void) result;

    return options.outputFile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::BeforeAssembly
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::BeforeAssembly (const CommandLineOptions & options) const
{
    (void) options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::AfterAssembly
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::AfterAssembly (const CommandLineOptions & options, long long elapsedMicroseconds) const
{
    (void) options;
    (void) elapsedMicroseconds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::ReportAssembled
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::ReportAssembled (const CommandLineOptions & options, const AssemblyResult & result) const
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
