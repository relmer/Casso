#include "Pch.h"

#include "AssemblerMode.h"
#include "ArtifactWriter.h"
#include "As65Mode.h"
#include "Assembler.h"
#include "As65ExitStatus.h"
#include "DialectReporting.h"
#include "ImageArtifactSink.h"
#include "MerlinMode.h"
#include "Win32DiskFileIo.h"





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

HRESULT AssemblerMode::Run (const CommandLineOptions & options, int & exitCode,
                            FileReader * sourceReader, ArtifactSink * artifacts) const
{
    using Clock = std::chrono::high_resolution_clock;



    const int                       kNoOutput  = As65ExitStatus::kNoOutput;
    HRESULT                         hr         = S_OK;
    AssemblerOptions                asmOptions;
    DefaultFileReader               fileReader;
    FileArtifactSink                fileSink;
    Win32DiskFileIo                 diskFileIo;
    ImageArtifactSink               imageSink (diskFileIo);
    //  Empty means no image was named, which is the one question deciding
    //  where the object goes. Asked once so the two sinks cannot disagree.
    bool                            toImage    = !options.imagePath.empty();
    ArtifactSink                  * chosen     = toImage ? static_cast<ArtifactSink *> (&imageSink)
                                                         : static_cast<ArtifactSink *> (&fileSink);
    ArtifactSink                  * out        = artifacts ? artifacts : chosen;
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
    asmOptions.fileReader = sourceReader ? sourceReader : &fileReader;

    lastSep = options.inputFile.find_last_of ("/\\");

    if (lastSep != std::string::npos)
    {
        asmOptions.baseDir = options.inputFile.substr (0, lastSep);
    }

    cpu.Reset();
    ReportAssemblyStarting (options);

    startTime = Clock::now();
    ar        = SourceAssembler::Assemble (options.inputFile, CreateInstructionSetProvider (options, cpu),
                                          asmOptions, sourceReader);
    endTime   = Clock::now();

    ReportAssemblyFinished (options, std::chrono::duration_cast<std::chrono::microseconds> (endTime - startTime).count());

    SourceAssembler::ReportDiagnostics (ar);

    //  What is worth saying about the dialect and the CPU, and where it may be
    //  said, is decided in core. This prints what comes back and chooses
    //  nothing: several of those cases report nothing at all, and a caller
    //  reimplementing the rule is a caller that ends up printing always.
    reports  = DialectReporting::BuildReport (asmOptions, SourceAssembler::BuildCpuReport (options, ar.result));
    SourceAssembler::ReportToStderr (reports);

    //  THE DOCUMENTED NUMBERS, WHICH THIS DID NOT RETURN.
    //
    //  As65ExitStatus was written to move an assembly error off 2 and
    //  warnings off 1 -- 1 being as65's bad command line, which a ported
    //  build script branches on -- and it was tested, documented, and never
    //  called. The executable kept an older mapper with no status 3 in it at
    //  all, so every page in the help described a numbering the tool did not
    //  use: an assembly error exited 2 and a warning exited 1.
    exitCode = As65ExitStatus::GetAssemblyStatus (ar.sourceRead, ar.ok, !ar.result.warnings.empty());

    CBREx (ar.ok, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    ReportAssemblySucceeded (options, ar.result);

    hr = RefuseUnusableOutputRequest (options, ar.result);
    CHRF (hr, exitCode = kNoOutput);

    //  RESOLVED BEFORE THE LISTING, not after it. A listing that names no file
    //  of its own is written beside the object and takes its name from it, so
    //  the object's name has to be settled first. It used to be settled only in
    //  time for the object, which is fine while every listing either names
    //  itself or goes to standard output and wrong the moment one is derived.
    writeOptions            = options;
    writeOptions.outputFile = ResolveOutputName (options, ar.result);

    hr = options.generateListing ? out->WriteListing (ar.result, writeOptions, reports) : S_OK;
    CHRF (hr, ReportSinkDiagnostics (*out); exitCode = kNoOutput);

    hr = out->WriteBinary (ar.result, writeOptions);
    CHRF (hr, ReportSinkDiagnostics (*out); exitCode = kNoOutput);

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
//  AssemblerMode::ReportSinkDiagnostics
//
//  What the sink had to say about a write that failed.
//
//  THE SINK CARRIES ITS REFUSALS AND THIS PRINTS THEM. A library has no
//  business owning a console, which is why the words are carried rather than
//  written where they are decided -- and for one release nothing on this side
//  read them back, so every refusal on the disk path exited non-zero in
//  silence: no image, wrong type for the filesystem, volume full, locked file,
//  illegal name, image held by another program. All of them.
//
//  Empty for the sink that writes host files, which says its own piece as it
//  goes, so this prints nothing on that path rather than a blank line.
//
////////////////////////////////////////////////////////////////////////////////

void AssemblerMode::ReportSinkDiagnostics (const ArtifactSink & sink)
{
    const std::string &  said = sink.GetDiagnostics();



    if (!said.empty())
    {
        std::cerr << said;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblerMode::RefuseUnusableOutputRequest
//
//  What the invocation asked for against what the source turned out to produce.
//
//  ASKED HERE BECAUSE THIS IS WHERE BOTH ARE VISIBLE, which is the same reason
//  the precedence between a flag and a directive is settled by the assembler
//  rather than guessed by the parser. Neither of these can be answered from the
//  command line alone: how many outputs a source produces, and whether it
//  states a file type, are known only once it has been assembled.
//
//  Refused BEFORE anything is written, so a target that cannot serve the
//  request is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssemblerMode::RefuseUnusableOutputRequest (const CommandLineOptions & options,
                                                    const AssemblyResult & result) const
{
    HRESULT  hr        = S_OK;
    bool     several   = result.savePoints.size() > 1;
    bool     namedOnce = !options.onDiskName.empty();
    bool     typedHere = false;
    bool     usable    = true;



    //  ONE NAME CANNOT SERVE SEVERAL FILES. Applying it to each output in turn
    //  would leave each overwriting the last, and the tool would report success
    //  having written one file where the source asked for three. This is not
    //  the case two saves under one name make: there the SOURCE said so and the
    //  period assembler allows it, where here an option said it about outputs
    //  the option's author could not have seen.
    if (several && namedOnce)
    {
        std::println (stderr,
                      "Error: this source produces {} outputs and a single name was given for them",
                      result.savePoints.size());
        usable = false;
    }

    //  A TYPE WITH NO FILESYSTEM TO SET IT ON. The directive that states one
    //  names a ProDOS file type, and a host file has no such thing, so the
    //  reason it was once refused outright still stands whenever no image is
    //  named. Unlike the naming directives beside it there is no host meaning to
    //  fall back to.
    for (const SavePoint & span : result.savePoints)
    {
        typedHere = typedHere || span.hasFileType;
    }

    if (typedHere && options.imagePath.empty())
    {
        std::println (stderr,
                      "Error: the source sets a filesystem file type and no image was named");
        std::println (stderr,
                      "       add {}{}disk <image>, or remove the directive",
                      options.flagPrefix, options.flagPrefix == '/' ? "" : "-");
        usable = false;
    }

    //  NOT E_INVALIDARG, which asserts and marks a coding error. Both conditions
    //  above are things a person typed or wrote, so they earn a verdict at the
    //  edge rather than an assertion. The same code an assembly error returns.
    CBREx (usable, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

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
