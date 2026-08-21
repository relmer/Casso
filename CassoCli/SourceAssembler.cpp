#include "Pch.h"

#include "SourceAssembler.h"
#include "HostFile.h"
#include "Assembler.h"
#include "Cpu65C02Table.h"
#include "DiagnosticFormatter.h"
#include "DialectRegistry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::SelectInstructionSet
//
//  Picks the assembler's target instruction table from `-x`. The default is the
//  strict 6502 table (`cpu`), so 65C02-only opcodes never assemble by accident;
//  `-x` substitutes the CMOS 65C02 (Rockwell R65C02) table.
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * SourceAssembler::SourceAssembler::SelectInstructionSet (const CommandLineOptions & options, const Cpu & cpu)
{
    const Microcode *  set = nullptr;



    if (options.cpuTarget == CommandLineOptions::CpuTarget::M65C02)
    {
        set = GetCpu65C02InstructionSet();
    }
    else
    {
        set = cpu.GetInstructionSet();
    }

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::BuildOptions
//
//  The dialect is carried across WITH its provenance. Both, because a dialect
//  the invocation named needs no report and one the caller merely inherited
//  does -- and the dialect alone cannot say which happened, since the default
//  is also a dialect a caller can ask for by name.
//
//  The output name goes across as the CALLER's answer, which beats any name the
//  source gives itself. Empty when no output flag was given, which is how a
//  dialect whose source names its own object gets to.
//
////////////////////////////////////////////////////////////////////////////////

AssemblerOptions SourceAssembler::BuildOptions (const CommandLineOptions & options)
{
    AssemblerOptions  asmOptions  = {};



    asmOptions.dialect            = options.dialect;
    asmOptions.dialectSelection   = options.dialectSelection;
    asmOptions.outputFileName     = options.outputFile;
    asmOptions.fillByte           = options.fillByte;
    asmOptions.generateListing    = options.generateListing;
    asmOptions.warningMode        = options.warningMode;
    asmOptions.cycleCounts        = options.cycleCounts;
    asmOptions.macroExpansion     = options.macroExpansion;
    asmOptions.pageHeight         = options.pageHeight;
    asmOptions.pageWidth          = options.pageWidth;
    asmOptions.pass1Listing       = options.pass1Listing;
    asmOptions.symbolTable        = options.symbolTable;
    asmOptions.debugInfo          = options.debugInfo;
    asmOptions.verbose            = options.verbose;
    asmOptions.quiet              = options.quiet;
    asmOptions.disableOpt         = options.disableOpt;
    asmOptions.predefinedSymbols  = options.predefinedSymbols;

    // Carried so an assembler diagnostic naming a flag names it the way this
    // invocation used for its flags. The assembler never sees a command line.
    asmOptions.flagPrefix         = options.flagPrefix;

    return asmOptions;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::Assemble
//
//  Reads one source file and assembles it, bundling the result with the input
//  path so diagnostics can be attributed later.
//
//  Carrying the filename in the result is what lets ReportAssemblyDiagnostics
//  print `file:line: error:` without being handed the path separately -- the
//  format editors parse to jump to the offending line.
//
//  An unreadable file and a failed assembly both come back as ok == false, so
//  the caller has one failure test. They are distinguished for the USER by the
//  message, which is where the distinction actually matters.
//
//  A second instruction table is OPTIONAL and is what a dialect selecting its
//  CPU in the source switches to. Null means there is nothing to switch to, and
//  the assembler says so rather than pretending the wider set arrived -- so a
//  caller passing one table has not quietly promised two.
//
////////////////////////////////////////////////////////////////////////////////

SourceAssembler::Result SourceAssembler::Assemble (const std::string & inputFile,
                                                      const Microcode instructionSet[256],
                                                      const Microcode extendedSet[256],
                                                      const AssemblerOptions & asmOptions)
{
    HRESULT                  hr        = S_OK;
    SourceAssembler::Result  ar        = {};
    std::string              source;
    bool                     canSwitch = extendedSet != nullptr;

    ar.inputFile = inputFile;



    hr = HostFile::ReadAll (inputFile, source);

    if (FAILED (hr))
    {
        std::cerr << "Error: Cannot read input file: " << inputFile << "\n";
        ar.ok = false;
    }
    else if (canSwitch)
    {
        Assembler  asm6502 (instructionSet, extendedSet, asmOptions);

        ar.result = asm6502.Assemble (source);
        ar.ok     = ar.result.success;
    }
    else
    {
        Assembler  asm6502 (instructionSet, asmOptions);

        ar.result = asm6502.Assemble (source);
        ar.ok     = ar.result.success;
    }

    return ar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::ReportDiagnostics
//
////////////////////////////////////////////////////////////////////////////////

void SourceAssembler::ReportDiagnostics (const SourceAssembler::Result & ar)
{
    // The input path is the FALLBACK, not the answer. A diagnostic that carries
    // its own file names that file -- which is how an error inside an included
    // file stops being attributed to the top-level source.
    for (const auto & w : ar.result.warnings)
    {
        std::println (stderr, "{}", DiagnosticFormatter::Format (w, ar.inputFile, DiagnosticSeverity::Warning));
    }

    for (const auto & e : ar.result.errors)
    {
        std::println (stderr, "{}", DiagnosticFormatter::Format (e, ar.inputFile, DiagnosticSeverity::Error));
    }

    if (!ar.ok)
    {
        std::println (stderr, "Assembly failed with {} error(s)", ar.result.errors.size());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::BuildCpuReport
//
//  What to call the instruction set the assembly ran on, and what chose it.
//
//  The NAME comes from here because the tables come from here. Core's assembler
//  receives instruction sets as unnamed tables, so only the caller that handed
//  them over knows which processors they are.
//
//  The source's own selection outranks the flag's absence, and both outrank the
//  default. That order matters: a run whose source selected the wider set and
//  whose command line said nothing would otherwise report the narrow default,
//  which is worse than saying nothing at all.
//
////////////////////////////////////////////////////////////////////////////////

CpuReport SourceAssembler::SourceAssembler::BuildCpuReport (const CommandLineOptions & options, const AssemblyResult & result)
{
    CpuReport  report;
    bool       isCmos = options.cpuTarget == CommandLineOptions::CpuTarget::M65C02;



    report.name      = isCmos ? "65c02" : "6502";
    report.selection = options.hasCpuTarget ? CpuSelection::StatedOnCommandLine
                                            : CpuSelection::DialectDefault;

    if (result.extendedSetSelectedInSource)
    {
        report.name      = "65c02";
        report.selection = CpuSelection::SelectedInSource;
    }

    return report;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceAssembler::ReportToStderr
//
//  Prints the reports due on stderr, and only those.
//
//  Which sink a report belongs to is decided in core; this walks the list and
//  prints the ones addressed here. Nothing selects stdout, and nothing may:
//  stdout carries the listing when no listing file is named, and a line printed
//  there lands inside the artifact a build script is piping.
//
////////////////////////////////////////////////////////////////////////////////

void SourceAssembler::ReportToStderr (const std::vector<DialectReportLine> & reports)
{
    for (const DialectReportLine & report : reports)
    {
        if (report.sink == ReportSink::StandardError)
        {
            std::cerr << report.text << "\n";
        }
    }
}
